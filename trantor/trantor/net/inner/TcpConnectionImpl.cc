/**
 *
 *  @file TcpConnectionImpl.cc
 *  @author An Tao
 *
 *  Public header file in trantor lib.
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the License file.
 *
 *
 */

#include "TcpConnectionImpl.h"
#include "Socket.h"
#include "Channel.h"
#ifdef __linux__
#include <sys/sendfile.h>
#endif
#include <sys/types.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <WinSock2.h>
#include <Windows.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#endif
#ifdef _WIN32
#define stat _stati64
#endif
#include <regex>

using namespace trantor;

#ifdef USE_OPENSSL
namespace trantor
{
namespace internal
{
#ifdef _WIN32
// Code yanked from stackoverflow
// https://stackoverflow.com/questions/9507184/can-openssl-on-windows-use-the-system-certificate-store
inline bool loadWindowsSystemCert(X509_STORE *store)
{
    auto hStore = CertOpenSystemStoreW((HCRYPTPROV_LEGACY)NULL, L"ROOT");

    if (!hStore)
    {
        return false;
    }

    PCCERT_CONTEXT pContext = NULL;
    while ((pContext = CertEnumCertificatesInStore(hStore, pContext)) !=
           nullptr)
    {
        auto encoded_cert =
            static_cast<const unsigned char *>(pContext->pbCertEncoded);

        auto x509 = d2i_X509(NULL, &encoded_cert, pContext->cbCertEncoded);
        if (x509)
        {
            X509_STORE_add_cert(store, x509);
            X509_free(x509);
        }
    }

    CertFreeCertificateContext(pContext);
    CertCloseStore(hStore, 0);

    return true;
}
#endif

inline std::string certNameToRegex(const std::string &certName)
{
    std::string result;
    result.reserve(certName.size() + 11);

    bool isStar = false;
    bool isLeadingStar = true;
    for (char ch : certName)
    {
        if (isStar == false)
        {
            if (ch == '*')
                isStar = true;
            else if (ch == '.')
            {
                result += "\\.";
                isLeadingStar = false;
            }
            else
            {
                result.push_back(ch);
                isLeadingStar = false;
            }
        }
        else
        {
            if (ch == '.' && isLeadingStar)
                result += "([^.]*\\.|)?";
            else
                result += std::string("[^.]*") + ch;
            isStar = false;
        }
    }
    assert(isStar == false);
    return result;
}

inline bool verifyName(const std::string &certName, const std::string &hostname)
{
    std::regex re(certNameToRegex(certName));
    return std::regex_match(hostname, re);
}

inline bool verifyCommonName(X509 *cert, const std::string &hostname)
{
    X509_NAME *subjectName = X509_get_subject_name(cert);

    if (subjectName != nullptr)
    {
        std::array<char, BUFSIZ> name;
        auto length = X509_NAME_get_text_by_NID(subjectName,
                                                NID_commonName,
                                                name.data(),
                                                (int)name.size());
        if (length == -1)
            return false;

        return verifyName(std::string(name.begin(), name.begin() + length),
                          hostname);
    }

    return false;
}

inline bool verifyAltName(X509 *cert, const std::string &hostname)
{
    bool good = false;
    auto altNames = static_cast<const struct stack_st_GENERAL_NAME *>(
        X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr));

    if (altNames)
    {
        int numNames = sk_GENERAL_NAME_num(altNames);

        for (int i = 0; i < numNames && !good; i++)
        {
            auto val = sk_GENERAL_NAME_value(altNames, i);
            if (val->type != GEN_DNS)
            {
                LOG_WARN << "Name using IP addresses are not supported. Open "
                            "an issue if you need that feature";
                continue;
            }
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
            auto name = (const char *)ASN1_STRING_get0_data(val->d.ia5);
#else
            auto name = (const char *)ASN1_STRING_data(val->d.ia5);
#endif
            auto name_len = (size_t)ASN1_STRING_length(val->d.ia5);
            good = verifyName(std::string(name, name + name_len), hostname);
        }
    }

    GENERAL_NAMES_free((STACK_OF(GENERAL_NAME) *)altNames);
    return good;
}

}  // namespace internal

void initOpenSSL()
{
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || \
    (defined(LIBRESSL_VERSION_NUMBER) &&      \
     LIBRESSL_VERSION_NUMBER < 0x20700000L)
    // Initialize OpenSSL once;
    static std::once_flag once;
    std::call_once(once, []() {
        SSL_library_init();
        ERR_load_crypto_strings();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
    });
#endif
}

class SSLContext
{
  public:
    explicit SSLContext(bool useOldTLS, bool enableValidtion)
    {
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
        ctxPtr_ = SSL_CTX_new(TLS_method());
        if (!useOldTLS)
        {
            SSL_CTX_set_min_proto_version(ctxPtr_, TLS1_2_VERSION);
        }
        else
        {
            LOG_WARN << "TLS 1.0/1.1 are enabled. They are considered "
                        "obsolete, insecure standards and should only be "
                        "used for legacy purpose.";
        }
#else
        ctxPtr_ = SSL_CTX_new(SSLv23_method());
        if (!useOldTLS)
        {
            SSL_CTX_set_options(ctxPtr_, SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
        }
        else
        {
            LOG_WARN << "TLS 1.0/1.1 are enabled. They are considered "
                        "obsolete, insecure standards and should only be "
                        "used for legacy purpose.";
        }
#endif
#ifdef _WIN32
        if (enableValidtion)
            internal::loadWindowsSystemCert(SSL_CTX_get_cert_store(ctxPtr_));
#else
        if (enableValidtion)
            SSL_CTX_set_default_verify_paths(ctxPtr_);
#endif
    }
    ~SSLContext()
    {
        if (ctxPtr_)
        {
            SSL_CTX_free(ctxPtr_);
        }
    }

    SSL_CTX *get()
    {
        return ctxPtr_;
    }

  private:
    SSL_CTX *ctxPtr_;
};
class SSLConn
{
  public:
    explicit SSLConn(SSL_CTX *ctx)
    {
        SSL_ = SSL_new(ctx);
    }
    ~SSLConn()
    {
        if (SSL_)
        {
            SSL_free(SSL_);
        }
    }
    SSL *get()
    {
        return SSL_;
    }

  private:
    SSL *SSL_;
};

std::shared_ptr<SSLContext> newSSLContext(bool useOldTLS, bool validateCert)
{  // init OpenSSL
    initOpenSSL();
    return std::make_shared<SSLContext>(useOldTLS, validateCert);
}
std::shared_ptr<SSLContext> newSSLServerContext(const std::string &certPath,
                                                const std::string &keyPath,
                                                bool useOldTLS)
{
    auto ctx = newSSLContext(useOldTLS, false);
    auto r = SSL_CTX_use_certificate_chain_file(ctx->get(), certPath.c_str());
    if (!r)
    {
#ifndef _MSC_VER
        LOG_FATAL << strerror(errno);
#else
        LOG_FATAL << strerror_tl(errno);
#endif
        abort();
    }
    r = SSL_CTX_use_PrivateKey_file(ctx->get(),
                                    keyPath.c_str(),
                                    SSL_FILETYPE_PEM);
    if (!r)
    {
#ifndef _MSC_VER
        LOG_FATAL << strerror(errno);
#else
        LOG_FATAL << strerror_tl(errno);
#endif
        abort();
    }
    r = SSL_CTX_check_private_key(ctx->get());
    if (!r)
    {
#ifndef _MSC_VER
        LOG_FATAL << strerror(errno);
#else
        LOG_FATAL << strerror_tl(errno);
#endif
        abort();
    }
    return ctx;
}
}  // namespace trantor
#else
namespace trantor
{
std::shared_ptr<SSLContext> newSSLServerContext(const std::string &certPath,
                                                const std::string &keyPath,
                                                bool useOldTLS)
{
    LOG_FATAL << "OpenSSL is not found in your system!";
    abort();
}
}  // namespace trantor
#endif

TcpConnectionImpl::TcpConnectionImpl(EventLoop *loop,
                                     int socketfd,
                                     const InetAddress &localAddr,
                                     const InetAddress &peerAddr)
    : loop_(loop),
      ioChannelPtr_(new Channel(loop, socketfd)),
      socketPtr_(new Socket(socketfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr)
{
    LOG_TRACE << "new connection:" << peerAddr.toIpPort() << "->"
              << localAddr.toIpPort();
    ioChannelPtr_->setReadCallback(
        std::bind(&TcpConnectionImpl::readCallback, this));
    ioChannelPtr_->setWriteCallback(
        std::bind(&TcpConnectionImpl::writeCallback, this));
    ioChannelPtr_->setCloseCallback(
        std::bind(&TcpConnectionImpl::handleClose, this));
    ioChannelPtr_->setErrorCallback(
        std::bind(&TcpConnectionImpl::handleError, this));
    socketPtr_->setKeepAlive(true);
    name_ = localAddr.toIpPort() + "--" + peerAddr.toIpPort();
}
TcpConnectionImpl::~TcpConnectionImpl()
{
}
#ifdef USE_OPENSSL
void TcpConnectionImpl::startClientEncryptionInLoop(
    std::function<void()> &&callback,
    bool useOldTLS,
    bool validateCert,
    const std::string &hostname)
{
    validateCert_ = validateCert;
    loop_->assertInLoopThread();
    if (isEncrypted_)
    {
        LOG_WARN << "This connection is already encrypted";
        return;
    }
    sslEncryptionPtr_ = std::make_unique<SSLEncryption>();
    sslEncryptionPtr_->upgradeCallback_ = std::move(callback);
    sslEncryptionPtr_->sslCtxPtr_ = newSSLContext(useOldTLS, validateCert_);
    sslEncryptionPtr_->sslPtr_ =
        std::make_unique<SSLConn>(sslEncryptionPtr_->sslCtxPtr_->get());
    if (validateCert)
    {
        SSL_set_verify(sslEncryptionPtr_->sslPtr_->get(),
                       SSL_VERIFY_NONE,
                       nullptr);
        validateCert_ = validateCert;
    }
    if (!hostname.empty())
    {
        SSL_set_tlsext_host_name(sslEncryptionPtr_->sslPtr_->get(),
                                 hostname.data());
        sslEncryptionPtr_->hostname_ = hostname;
    }
    isEncrypted_ = true;
    sslEncryptionPtr_->isUpgrade_ = true;
    auto r = SSL_set_fd(sslEncryptionPtr_->sslPtr_->get(), socketPtr_->fd());
    (void)r;
    assert(r);
    sslEncryptionPtr_->sendBufferPtr_ =
        std::make_unique<std::array<char, 8192>>();
    LOG_TRACE << "connectEstablished";
    ioChannelPtr_->enableWriting();
    SSL_set_connect_state(sslEncryptionPtr_->sslPtr_->get());
}
void TcpConnectionImpl::startServerEncryptionInLoop(
    const std::shared_ptr<SSLContext> &ctx,
    std::function<void()> &&callback)
{
    loop_->assertInLoopThread();
    if (isEncrypted_)
    {
        LOG_WARN << "This connection is already encrypted";
        return;
    }
    sslEncryptionPtr_ = std::make_unique<SSLEncryption>();
    sslEncryptionPtr_->upgradeCallback_ = std::move(callback);
    sslEncryptionPtr_->sslCtxPtr_ = ctx;
    sslEncryptionPtr_->isServer_ = true;
    sslEncryptionPtr_->sslPtr_ =
        std::make_unique<SSLConn>(sslEncryptionPtr_->sslCtxPtr_->get());
    isEncrypted_ = true;
    sslEncryptionPtr_->isUpgrade_ = true;
    if (sslEncryptionPtr_->isServer_ == false)
        SSL_set_verify(sslEncryptionPtr_->sslPtr_->get(),
                       SSL_VERIFY_NONE,
                       nullptr);
    auto r = SSL_set_fd(sslEncryptionPtr_->sslPtr_->get(), socketPtr_->fd());
    (void)r;
    assert(r);
    sslEncryptionPtr_->sendBufferPtr_ =
        std::make_unique<std::array<char, 8192>>();
    LOG_TRACE << "upgrade to ssl";
    SSL_set_accept_state(sslEncryptionPtr_->sslPtr_->get());
}
#endif
void TcpConnectionImpl::startServerEncryption(
    const std::shared_ptr<SSLContext> &ctx,
    std::function<void()> callback)
{
#ifndef USE_OPENSSL
    LOG_FATAL << "OpenSSL is not found in your system!";
    abort();
#else
    if (loop_->isInLoopThread())
    {
        startServerEncryptionInLoop(ctx, std::move(callback));
    }
    else
    {
        loop_->queueInLoop([thisPtr = shared_from_this(),
                            ctx,
                            callback = std::move(callback)]() mutable {
            thisPtr->startServerEncryptionInLoop(ctx, std::move(callback));
        });
    }

#endif
}
void TcpConnectionImpl::startClientEncryption(std::function<void()> callback,
                                              bool useOldTLS,
                                              bool validateCert,
                                              std::string hostname)
{
#ifndef USE_OPENSSL
    LOG_FATAL << "OpenSSL is not found in your system!";
    abort();
#else
    if (!hostname.empty())
    {
        std::transform(hostname.begin(),
                       hostname.end(),
                       hostname.begin(),
                       tolower);
        assert(sslEncryptionPtr_ != nullptr);
        sslEncryptionPtr_->hostname_ = hostname;
    }
    if (loop_->isInLoopThread())
    {
        startClientEncryptionInLoop(std::move(callback),
                                    useOldTLS,
                                    validateCert,
                                    hostname);
    }
    else
    {
        loop_->queueInLoop([thisPtr = shared_from_this(),
                            callback = std::move(callback),
                            useOldTLS,
                            hostname = std::move(hostname),
                            validateCert]() mutable {
            thisPtr->startClientEncryptionInLoop(std::move(callback),
                                                 useOldTLS,
                                                 validateCert,
                                                 hostname);
        });
    }
#endif
}
void TcpConnectionImpl::readCallback()
{
// LOG_TRACE<<"read Callback";
#ifdef USE_OPENSSL
    if (!isEncrypted_)
    {
#endif
        loop_->assertInLoopThread();
        int ret = 0;

        ssize_t n = readBuffer_.readFd(socketPtr_->fd(), &ret);
        // LOG_TRACE<<"read "<<n<<" bytes from socket";
        if (n == 0)
        {
            // socket closed by peer
            handleClose();
        }
        else if (n < 0)
        {
            if (errno == EPIPE || errno == ECONNRESET)  // TODO: any others?
            {
                LOG_DEBUG << "EPIPE or ECONNRESET, errno=" << errno;
                return;
            }
#ifdef _WIN32
            if (errno == WSAECONNABORTED)
            {
                LOG_DEBUG << "WSAECONNABORTED, errno=" << errno;
                handleClose();
                return;
            }
#endif
            LOG_SYSERR << "read socket error";
            handleClose();
            return;
        }
        extendLife();
        if (n > 0)
        {
            bytesReceived_ += n;
            if (recvMsgCallback_)
            {
                recvMsgCallback_(shared_from_this(), &readBuffer_);
            }
        }
#ifdef USE_OPENSSL
    }
    else
    {
        LOG_TRACE << "read Callback";
        loop_->assertInLoopThread();
        if (sslEncryptionPtr_->statusOfSSL_ == SSLStatus::Handshaking)
        {
            doHandshaking();
            return;
        }
        else if (sslEncryptionPtr_->statusOfSSL_ == SSLStatus::Connected)
        {
            int rd;
            bool newDataFlag = false;
            size_t readLength;
            do
            {
                readBuffer_.ensureWritableBytes(1024);
                readLength = readBuffer_.writableBytes();
                rd = SSL_read(sslEncryptionPtr_->sslPtr_->get(),
                              readBuffer_.beginWrite(),
                              static_cast<int>(readLength));
                LOG_TRACE << "ssl read:" << rd << " bytes";
                if (rd <= 0)
                {
                    int sslerr =
                        SSL_get_error(sslEncryptionPtr_->sslPtr_->get(), rd);
                    if (sslerr == SSL_ERROR_WANT_READ)
                    {
                        break;
                    }
                    else
                    {
                        LOG_TRACE << "ssl read err:" << sslerr;
                        sslEncryptionPtr_->statusOfSSL_ =
                            SSLStatus::DisConnected;
                        handleClose();
                        return;
                    }
                }
                readBuffer_.hasWritten(rd);
                newDataFlag = true;
            } while ((size_t)rd == readLength);
            if (newDataFlag)
            {
                // Run callback function
                recvMsgCallback_(shared_from_this(), &readBuffer_);
            }
        }
    }
#endif
}
void TcpConnectionImpl::extendLife()
{
    if (idleTimeout_ > 0)
    {
        auto now = Date::date();
        if (now < lastTimingWheelUpdateTime_.after(1.0))
            return;
        lastTimingWheelUpdateTime_ = now;
        auto entry = kickoffEntry_.lock();
        if (entry)
        {
            auto timingWheelPtr = timingWheelWeakPtr_.lock();
            if (timingWheelPtr)
                timingWheelPtr->insertEntry(idleTimeout_, entry);
        }
    }
}
void TcpConnectionImpl::writeCallback()
{
#ifdef USE_OPENSSL
    if (!isEncrypted_ ||
        (sslEncryptionPtr_ &&
         sslEncryptionPtr_->statusOfSSL_ == SSLStatus::Connected))
    {
#endif
        loop_->assertInLoopThread();
        extendLife();
        if (ioChannelPtr_->isWriting())
        {
            assert(!writeBufferList_.empty());
            auto writeBuffer_ = writeBufferList_.front();
#ifndef _WIN32
            if (writeBuffer_->sendFd_ < 0)
#else
        if (writeBuffer_->sendFp_ == nullptr)
#endif
            {
                if (writeBuffer_->msgBuffer_->readableBytes() <= 0)
                {
                    writeBufferList_.pop_front();
                    if (writeBufferList_.empty())
                    {
                        ioChannelPtr_->disableWriting();
                        //
                        if (writeCompleteCallback_)
                            writeCompleteCallback_(shared_from_this());
                        if (status_ == ConnStatus::Disconnecting)
                        {
                            socketPtr_->closeWrite();
                        }
                    }
                    else
                    {
                        auto fileNode = writeBufferList_.front();
#ifndef _WIN32
                        assert(fileNode->sendFd_ >= 0);
#else
                    assert(fileNode->sendFp_);
#endif
                        sendFileInLoop(fileNode);
                    }
                }
                else
                {
                    auto n =
                        writeInLoop(writeBuffer_->msgBuffer_->peek(),
                                    writeBuffer_->msgBuffer_->readableBytes());
                    if (n >= 0)
                    {
                        writeBuffer_->msgBuffer_->retrieve(n);
                    }
                    else
                    {
#ifdef _WIN32
                        if (errno != 0 && errno != EWOULDBLOCK)
#else
                    if (errno != EWOULDBLOCK)
#endif
                        {
                            // TODO: any others?
                            if (errno == EPIPE || errno == ECONNRESET)
                            {
                                LOG_DEBUG << "EPIPE or ECONNRESET, erron="
                                          << errno;
                                return;
                            }
                            LOG_SYSERR << "Unexpected error(" << errno << ")";
                            return;
                        }
                    }
                }
            }
            else
            {
                // file
                if (writeBuffer_->fileBytesToSend_ <= 0)
                {
                    writeBufferList_.pop_front();
                    if (writeBufferList_.empty())
                    {
                        ioChannelPtr_->disableWriting();
                        if (writeCompleteCallback_)
                            writeCompleteCallback_(shared_from_this());
                        if (status_ == ConnStatus::Disconnecting)
                        {
                            socketPtr_->closeWrite();
                        }
                    }
                    else
                    {
#ifndef _WIN32
                        if (writeBufferList_.front()->sendFd_ < 0)
#else
                    if (writeBufferList_.front()->sendFp_ == nullptr)
#endif
                        {
                            // There is data to be sent in the buffer.
                            auto n = writeInLoop(
                                writeBufferList_.front()->msgBuffer_->peek(),
                                writeBufferList_.front()
                                    ->msgBuffer_->readableBytes());
                            writeBufferList_.front()->msgBuffer_->retrieve(n);
                            if (n >= 0)
                            {
                                writeBufferList_.front()->msgBuffer_->retrieve(
                                    n);
                            }
                            else
                            {
#ifdef _WIN32
                                if (errno != 0 && errno != EWOULDBLOCK)
#else
                            if (errno != EWOULDBLOCK)
#endif
                                {
                                    // TODO: any others?
                                    if (errno == EPIPE || errno == ECONNRESET)
                                    {
                                        LOG_DEBUG
                                            << "EPIPE or ECONNRESET, erron="
                                            << errno;
                                        return;
                                    }
                                    LOG_SYSERR << "Unexpected error(" << errno
                                               << ")";
                                    return;
                                }
                            }
                        }
                        else
                        {
                            // more file
                            sendFileInLoop(writeBufferList_.front());
                        }
                    }
                }
                else
                {
                    sendFileInLoop(writeBuffer_);
                }
            }
        }
        else
        {
            LOG_SYSERR << "no writing but call write callback";
        }
#ifdef USE_OPENSSL
    }
    else
    {
        LOG_TRACE << "write Callback";
        loop_->assertInLoopThread();
        if (sslEncryptionPtr_->statusOfSSL_ == SSLStatus::Handshaking)
        {
            doHandshaking();
            return;
        }
    }
#endif
}
void TcpConnectionImpl::connectEstablished()
{
// loop_->assertInLoopThread();
#ifdef USE_OPENSSL
    if (!isEncrypted_)
    {
#endif
        auto thisPtr = shared_from_this();
        loop_->runInLoop([thisPtr]() {
            LOG_TRACE << "connectEstablished";
            assert(thisPtr->status_ == ConnStatus::Connecting);
            thisPtr->ioChannelPtr_->tie(thisPtr);
            thisPtr->ioChannelPtr_->enableReading();
            thisPtr->status_ = ConnStatus::Connected;
            if (thisPtr->connectionCallback_)
                thisPtr->connectionCallback_(thisPtr);
        });
#ifdef USE_OPENSSL
    }
    else
    {
        loop_->runInLoop([thisPtr = shared_from_this()]() {
            LOG_TRACE << "connectEstablished";
            assert(thisPtr->status_ == ConnStatus::Connecting);
            thisPtr->ioChannelPtr_->tie(thisPtr);
            thisPtr->ioChannelPtr_->enableReading();
            thisPtr->status_ = ConnStatus::Connected;
            if (thisPtr->sslEncryptionPtr_->isServer_)
            {
                SSL_set_accept_state(
                    thisPtr->sslEncryptionPtr_->sslPtr_->get());
            }
            else
            {
                thisPtr->ioChannelPtr_->enableWriting();
                SSL_set_connect_state(
                    thisPtr->sslEncryptionPtr_->sslPtr_->get());
            }
        });
    }
#endif
}
void TcpConnectionImpl::handleClose()
{
    LOG_TRACE << "connection closed, fd=" << socketPtr_->fd();
    loop_->assertInLoopThread();
    status_ = ConnStatus::Disconnected;
    ioChannelPtr_->disableAll();
    //  ioChannelPtr_->remove();
    auto guardThis = shared_from_this();
    if (connectionCallback_)
        connectionCallback_(guardThis);
    if (closeCallback_)
    {
        LOG_TRACE << "to call close callback";
        closeCallback_(guardThis);
    }
}
void TcpConnectionImpl::handleError()
{
    int err = socketPtr_->getSocketError();
    if (err == 0)
        return;
    if (err == EPIPE || err == ECONNRESET || err == 104)
    {
        LOG_DEBUG << "[" << name_ << "] - SO_ERROR = " << err << " "
                  << strerror_tl(err);
    }
    else
    {
        LOG_ERROR << "[" << name_ << "] - SO_ERROR = " << err << " "
                  << strerror_tl(err);
    }
}
void TcpConnectionImpl::setTcpNoDelay(bool on)
{
    socketPtr_->setTcpNoDelay(on);
}
void TcpConnectionImpl::connectDestroyed()
{
    loop_->assertInLoopThread();
    if (status_ == ConnStatus::Connected)
    {
        status_ = ConnStatus::Disconnected;
        ioChannelPtr_->disableAll();

        connectionCallback_(shared_from_this());
    }
    ioChannelPtr_->remove();
}
void TcpConnectionImpl::shutdown()
{
    auto thisPtr = shared_from_this();
    loop_->runInLoop([thisPtr]() {
        if (thisPtr->status_ == ConnStatus::Connected)
        {
            thisPtr->status_ = ConnStatus::Disconnecting;
            if (!thisPtr->ioChannelPtr_->isWriting())
            {
                thisPtr->socketPtr_->closeWrite();
            }
        }
    });
}

void TcpConnectionImpl::forceClose()
{
    auto thisPtr = shared_from_this();
    loop_->runInLoop([thisPtr]() {
        if (thisPtr->status_ == ConnStatus::Connected ||
            thisPtr->status_ == ConnStatus::Disconnecting)
        {
            thisPtr->status_ = ConnStatus::Disconnecting;
            thisPtr->handleClose();
        }
    });
}
#ifndef _WIN32
void TcpConnectionImpl::sendInLoop(const void *buffer, size_t length)
#else
void TcpConnectionImpl::sendInLoop(const char *buffer, size_t length)
#endif
{
    loop_->assertInLoopThread();
    if (status_ != ConnStatus::Connected)
    {
        LOG_WARN << "Connection is not connected,give up sending";
        return;
    }
    extendLife();
    size_t remainLen = length;
    ssize_t sendLen = 0;
    if (!ioChannelPtr_->isWriting() && writeBufferList_.empty())
    {
        // send directly
        sendLen = writeInLoop(buffer, length);
        if (sendLen < 0)
        {
            // error
#ifdef _WIN32
            if (errno != 0 && errno != EWOULDBLOCK)
#else
            if (errno != EWOULDBLOCK)
#endif
            {
                if (errno == EPIPE || errno == ECONNRESET)  // TODO: any others?
                {
                    LOG_DEBUG << "EPIPE or ECONNRESET, erron=" << errno;
                    return;
                }
                LOG_SYSERR << "Unexpected error(" << errno << ")";
                return;
            }
            sendLen = 0;
        }
        remainLen -= sendLen;
    }
    if (remainLen > 0 && status_ == ConnStatus::Connected)
    {
        if (writeBufferList_.empty())
        {
            BufferNodePtr node(new BufferNode);
            node->msgBuffer_ = std::make_shared<MsgBuffer>();
            writeBufferList_.push_back(std::move(node));
        }
#ifndef _WIN32
        else if (writeBufferList_.back()->sendFd_ >= 0)
#else
        else if (writeBufferList_.back()->sendFp_)
#endif
        {
            BufferNodePtr node(new BufferNode);
            node->msgBuffer_ = std::make_shared<MsgBuffer>();
            writeBufferList_.push_back(std::move(node));
        }
        writeBufferList_.back()->msgBuffer_->append(
            static_cast<const char *>(buffer) + sendLen, remainLen);
        if (!ioChannelPtr_->isWriting())
            ioChannelPtr_->enableWriting();
        if (highWaterMarkCallback_ &&
            writeBufferList_.back()->msgBuffer_->readableBytes() >
                highWaterMarkLen_)
        {
            highWaterMarkCallback_(
                shared_from_this(),
                writeBufferList_.back()->msgBuffer_->readableBytes());
        }
    }
}
// The order of data sending should be same as the order of calls of send()
void TcpConnectionImpl::send(const std::shared_ptr<std::string> &msgPtr)
{
    if (loop_->isInLoopThread())
    {
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        if (sendNum_ == 0)
        {
            sendInLoop(msgPtr->data(), msgPtr->length());
        }
        else
        {
            ++sendNum_;
            auto thisPtr = shared_from_this();
            loop_->queueInLoop([thisPtr, msgPtr]() {
                thisPtr->sendInLoop(msgPtr->data(), msgPtr->length());
                std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
                --thisPtr->sendNum_;
            });
        }
    }
    else
    {
        auto thisPtr = shared_from_this();
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        ++sendNum_;
        loop_->queueInLoop([thisPtr, msgPtr]() {
            thisPtr->sendInLoop(msgPtr->data(), msgPtr->length());
            std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
            --thisPtr->sendNum_;
        });
    }
}
// The order of data sending should be same as the order of calls of send()
void TcpConnectionImpl::send(const std::shared_ptr<MsgBuffer> &msgPtr)
{
    if (loop_->isInLoopThread())
    {
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        if (sendNum_ == 0)
        {
            sendInLoop(msgPtr->peek(), msgPtr->readableBytes());
        }
        else
        {
            ++sendNum_;
            auto thisPtr = shared_from_this();
            loop_->queueInLoop([thisPtr, msgPtr]() {
                thisPtr->sendInLoop(msgPtr->peek(), msgPtr->readableBytes());
                std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
                --thisPtr->sendNum_;
            });
        }
    }
    else
    {
        auto thisPtr = shared_from_this();
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        ++sendNum_;
        loop_->queueInLoop([thisPtr, msgPtr]() {
            thisPtr->sendInLoop(msgPtr->peek(), msgPtr->readableBytes());
            std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
            --thisPtr->sendNum_;
        });
    }
}
void TcpConnectionImpl::send(const char *msg, size_t len)
{
    if (loop_->isInLoopThread())
    {
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        if (sendNum_ == 0)
        {
            sendInLoop(msg, len);
        }
        else
        {
            ++sendNum_;
            auto buffer = std::make_shared<std::string>(msg, len);
            auto thisPtr = shared_from_this();
            loop_->queueInLoop([thisPtr, buffer]() {
                thisPtr->sendInLoop(buffer->data(), buffer->length());
                std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
                --thisPtr->sendNum_;
            });
        }
    }
    else
    {
        auto buffer = std::make_shared<std::string>(msg, len);
        auto thisPtr = shared_from_this();
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        ++sendNum_;
        loop_->queueInLoop([thisPtr, buffer]() {
            thisPtr->sendInLoop(buffer->data(), buffer->length());
            std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
            --thisPtr->sendNum_;
        });
    }
}
void TcpConnectionImpl::send(const void *msg, size_t len)
{
    if (loop_->isInLoopThread())
    {
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        if (sendNum_ == 0)
        {
#ifndef _WIN32
            sendInLoop(msg, len);
#else
            sendInLoop(static_cast<const char *>(msg), len);
#endif
        }
        else
        {
            ++sendNum_;
            auto buffer =
                std::make_shared<std::string>(static_cast<const char *>(msg),
                                              len);
            auto thisPtr = shared_from_this();
            loop_->queueInLoop([thisPtr, buffer]() {
                thisPtr->sendInLoop(buffer->data(), buffer->length());
                std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
                --thisPtr->sendNum_;
            });
        }
    }
    else
    {
        auto buffer =
            std::make_shared<std::string>(static_cast<const char *>(msg), len);
        auto thisPtr = shared_from_this();
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        ++sendNum_;
        loop_->queueInLoop([thisPtr, buffer]() {
            thisPtr->sendInLoop(buffer->data(), buffer->length());
            std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
            --thisPtr->sendNum_;
        });
    }
}
void TcpConnectionImpl::send(const std::string &msg)
{
    if (loop_->isInLoopThread())
    {
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        if (sendNum_ == 0)
        {
            sendInLoop(msg.data(), msg.length());
        }
        else
        {
            ++sendNum_;
            auto thisPtr = shared_from_this();
            loop_->queueInLoop([thisPtr, msg]() {
                thisPtr->sendInLoop(msg.data(), msg.length());
                std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
                --thisPtr->sendNum_;
            });
        }
    }
    else
    {
        auto thisPtr = shared_from_this();
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        ++sendNum_;
        loop_->queueInLoop([thisPtr, msg]() {
            thisPtr->sendInLoop(msg.data(), msg.length());
            std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
            --thisPtr->sendNum_;
        });
    }
}
void TcpConnectionImpl::send(std::string &&msg)
{
    if (loop_->isInLoopThread())
    {
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        if (sendNum_ == 0)
        {
            sendInLoop(msg.data(), msg.length());
        }
        else
        {
            auto thisPtr = shared_from_this();
            ++sendNum_;
            loop_->queueInLoop([thisPtr, msg = std::move(msg)]() {
                thisPtr->sendInLoop(msg.data(), msg.length());
                std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
                --thisPtr->sendNum_;
            });
        }
    }
    else
    {
        auto thisPtr = shared_from_this();
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        ++sendNum_;
        loop_->queueInLoop([thisPtr, msg = std::move(msg)]() {
            thisPtr->sendInLoop(msg.data(), msg.length());
            std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
            --thisPtr->sendNum_;
        });
    }
}

void TcpConnectionImpl::send(const MsgBuffer &buffer)
{
    if (loop_->isInLoopThread())
    {
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        if (sendNum_ == 0)
        {
            sendInLoop(buffer.peek(), buffer.readableBytes());
        }
        else
        {
            ++sendNum_;
            auto thisPtr = shared_from_this();
            loop_->queueInLoop([thisPtr, buffer]() {
                thisPtr->sendInLoop(buffer.peek(), buffer.readableBytes());
                std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
                --thisPtr->sendNum_;
            });
        }
    }
    else
    {
        auto thisPtr = shared_from_this();
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        ++sendNum_;
        loop_->queueInLoop([thisPtr, buffer]() {
            thisPtr->sendInLoop(buffer.peek(), buffer.readableBytes());
            std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
            --thisPtr->sendNum_;
        });
    }
}

void TcpConnectionImpl::send(MsgBuffer &&buffer)
{
    if (loop_->isInLoopThread())
    {
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        if (sendNum_ == 0)
        {
            sendInLoop(buffer.peek(), buffer.readableBytes());
        }
        else
        {
            ++sendNum_;
            auto thisPtr = shared_from_this();
            loop_->queueInLoop([thisPtr, buffer = std::move(buffer)]() {
                thisPtr->sendInLoop(buffer.peek(), buffer.readableBytes());
                std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
                --thisPtr->sendNum_;
            });
        }
    }
    else
    {
        auto thisPtr = shared_from_this();
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        ++sendNum_;
        loop_->queueInLoop([thisPtr, buffer = std::move(buffer)]() {
            thisPtr->sendInLoop(buffer.peek(), buffer.readableBytes());
            std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
            --thisPtr->sendNum_;
        });
    }
}
void TcpConnectionImpl::sendFile(const char *fileName,
                                 size_t offset,
                                 size_t length)
{
    assert(fileName);
#ifndef _WIN32
    int fd = open(fileName, O_RDONLY);

    if (fd < 0)
    {
        LOG_SYSERR << fileName << " open error";
        return;
    }

    if (length == 0)
    {
        struct stat filestat;
        if (stat(fileName, &filestat) < 0)
        {
            LOG_SYSERR << fileName << " stat error";
            close(fd);
            return;
        }
        length = filestat.st_size;
    }

    sendFile(fd, offset, length);
#else
#ifndef _MSC_VER
    auto fp = fopen(fileName, "rb");
#else
    FILE *fp;
    if (fopen_s(&fp, fileName, "rb") != 0)
    {
        fp = nullptr;
    }
#endif
    if (fp == nullptr)
    {
        LOG_SYSERR << fileName << " open error";
        return;
    }

    if (length == 0)
    {
        struct stat filestat;
        if (stat(fileName, &filestat) < 0)
        {
            LOG_SYSERR << fileName << " stat error";
            fclose(fp);
            return;
        }
        length = filestat.st_size;
    }

    sendFile(fp, offset, length);
#endif
}

#ifndef _WIN32
void TcpConnectionImpl::sendFile(int sfd, size_t offset, size_t length)
#else
void TcpConnectionImpl::sendFile(FILE *fp, size_t offset, size_t length)
#endif
{
    assert(length > 0);
#ifndef _WIN32
    assert(sfd >= 0);
    BufferNodePtr node(new BufferNode);
    node->sendFd_ = sfd;
#else
    assert(fp);
    BufferNodePtr node(new BufferNode);
    node->sendFp_ = fp;
#endif
    node->offset_ = static_cast<off_t>(offset);
    node->fileBytesToSend_ = length;
    if (loop_->isInLoopThread())
    {
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        if (sendNum_ == 0)
        {
            writeBufferList_.push_back(node);
            if (writeBufferList_.size() == 1)
            {
                sendFileInLoop(writeBufferList_.front());
                return;
            }
        }
        else
        {
            ++sendNum_;
            auto thisPtr = shared_from_this();
            loop_->queueInLoop([thisPtr, node]() {
                thisPtr->writeBufferList_.push_back(node);
                {
                    std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
                    --thisPtr->sendNum_;
                }

                if (thisPtr->writeBufferList_.size() == 1)
                {
                    thisPtr->sendFileInLoop(thisPtr->writeBufferList_.front());
                }
            });
        }
    }
    else
    {
        auto thisPtr = shared_from_this();
        std::lock_guard<std::mutex> guard(sendNumMutex_);
        ++sendNum_;
        loop_->queueInLoop([thisPtr, node]() {
            LOG_TRACE << "Push sendfile to list";
            thisPtr->writeBufferList_.push_back(node);

            {
                std::lock_guard<std::mutex> guard1(thisPtr->sendNumMutex_);
                --thisPtr->sendNum_;
            }

            if (thisPtr->writeBufferList_.size() == 1)
            {
                thisPtr->sendFileInLoop(thisPtr->writeBufferList_.front());
            }
        });
    }
}

void TcpConnectionImpl::sendFileInLoop(const BufferNodePtr &filePtr)
{
    loop_->assertInLoopThread();
#ifndef _WIN32
    assert(filePtr->sendFd_ >= 0);
#else
    assert(filePtr->sendFp_);
#endif
#ifdef __linux__
    if (!isEncrypted_)
    {
        auto bytesSent = sendfile(socketPtr_->fd(),
                                  filePtr->sendFd_,
                                  &filePtr->offset_,
                                  filePtr->fileBytesToSend_);
        if (bytesSent < 0)
        {
            if (errno != EAGAIN)
            {
                LOG_SYSERR << "TcpConnectionImpl::sendFileInLoop";
                if (ioChannelPtr_->isWriting())
                    ioChannelPtr_->disableWriting();
            }
            return;
        }
        if (bytesSent < filePtr->fileBytesToSend_)
        {
            if (bytesSent == 0)
            {
                LOG_SYSERR << "TcpConnectionImpl::sendFileInLoop";
                return;
            }
        }
        LOG_TRACE << "sendfile() " << bytesSent << " bytes sent";
        filePtr->fileBytesToSend_ -= bytesSent;
        if (!ioChannelPtr_->isWriting())
        {
            ioChannelPtr_->enableWriting();
        }
        return;
    }
#endif
#ifndef _WIN32
    lseek(filePtr->sendFd_, filePtr->offset_, SEEK_SET);
    if (!fileBufferPtr_)
    {
        fileBufferPtr_ = std::make_unique<std::vector<char>>(16 * 1024);
    }
    while (filePtr->fileBytesToSend_ > 0)
    {
        auto n = read(filePtr->sendFd_,
                      &(*fileBufferPtr_)[0],
                      fileBufferPtr_->size());
#else
    _fseeki64(filePtr->sendFp_, filePtr->offset_, SEEK_SET);
    if (!fileBufferPtr_)
    {
        fileBufferPtr_ = std::make_unique<std::vector<char>>(16 * 1024);
    }
    while (filePtr->fileBytesToSend_ > 0)
    {
        auto n = fread(&(*fileBufferPtr_)[0],
                       1,
                       fileBufferPtr_->size(),
                       filePtr->sendFp_);
#endif
        if (n > 0)
        {
            auto nSend = writeInLoop(&(*fileBufferPtr_)[0], n);
            if (nSend >= 0)
            {
                filePtr->fileBytesToSend_ -= nSend;
                filePtr->offset_ += static_cast<off_t>(nSend);
                if (static_cast<size_t>(nSend) < n)
                {
                    if (!ioChannelPtr_->isWriting())
                    {
                        ioChannelPtr_->enableWriting();
                    }
                    return;
                }
                else if (nSend == n)
                    continue;
            }
            if (nSend < 0)
            {
#ifdef _WIN32
                if (errno != 0 && errno != EWOULDBLOCK)
#else
                if (errno != EWOULDBLOCK)
#endif
                {
                    // TODO: any others?
                    if (errno == EPIPE || errno == ECONNRESET)
                    {
                        LOG_DEBUG << "EPIPE or ECONNRESET, erron=" << errno;
                        return;
                    }
                    LOG_SYSERR << "Unexpected error(" << errno << ")";
                    return;
                }
                break;
            }
        }
        if (n < 0)
        {
            LOG_SYSERR << "read error";
            if (ioChannelPtr_->isWriting())
                ioChannelPtr_->disableWriting();
            return;
        }
        if (n == 0)
        {
            LOG_SYSERR << "read";
            return;
        }
    }
    if (!ioChannelPtr_->isWriting())
    {
        ioChannelPtr_->enableWriting();
    }
}
#ifndef _WIN32
ssize_t TcpConnectionImpl::writeInLoop(const void *buffer, size_t length)
#else
ssize_t TcpConnectionImpl::writeInLoop(const char *buffer, size_t length)
#endif
{
#ifdef USE_OPENSSL
    if (!isEncrypted_)
    {
#endif
        bytesSent_ += length;
#ifndef _WIN32
        return write(socketPtr_->fd(), buffer, length);
#else
    errno = 0;
    return ::send(socketPtr_->fd(), buffer, static_cast<int>(length), 0);
#endif
#ifdef USE_OPENSSL
    }
    else
    {
        LOG_TRACE << "send in loop";
        loop_->assertInLoopThread();
        if (status_ != ConnStatus::Connected &&
            status_ != ConnStatus::Disconnecting)
        {
            LOG_WARN << "Connection is not connected,give up sending";
            return -1;
        }
        if (sslEncryptionPtr_->statusOfSSL_ != SSLStatus::Connected)
        {
            LOG_WARN << "SSL is not connected,give up sending";
            return -1;
        }
        // send directly
        size_t sendTotalLen = 0;
        while (sendTotalLen < length)
        {
            auto len = length - sendTotalLen;
            if (len > sslEncryptionPtr_->sendBufferPtr_->size())
            {
                len = sslEncryptionPtr_->sendBufferPtr_->size();
            }
            memcpy(sslEncryptionPtr_->sendBufferPtr_->data(),
                   static_cast<const char *>(buffer) + sendTotalLen,
                   len);
            ERR_clear_error();
            auto sendLen = SSL_write(sslEncryptionPtr_->sslPtr_->get(),
                                     sslEncryptionPtr_->sendBufferPtr_->data(),
                                     static_cast<int>(len));
            if (sendLen <= 0)
            {
                int sslerr =
                    SSL_get_error(sslEncryptionPtr_->sslPtr_->get(), sendLen);
                if (sslerr != SSL_ERROR_WANT_WRITE &&
                    sslerr != SSL_ERROR_WANT_READ)
                {
                    // LOG_ERROR << "ssl write error:" << sslerr;
                    forceClose();
                    return -1;
                }
                return sendTotalLen;
            }
            sendTotalLen += sendLen;
        }
        return sendTotalLen;
    }
#endif
}

#ifdef USE_OPENSSL

TcpConnectionImpl::TcpConnectionImpl(EventLoop *loop,
                                     int socketfd,
                                     const InetAddress &localAddr,
                                     const InetAddress &peerAddr,
                                     const std::shared_ptr<SSLContext> &ctxPtr,
                                     bool isServer,
                                     bool validateCert,
                                     const std::string &hostname)
    : loop_(loop),
      ioChannelPtr_(new Channel(loop, socketfd)),
      socketPtr_(new Socket(socketfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      isEncrypted_(true)
{
    LOG_TRACE << "new connection:" << peerAddr.toIpPort() << "->"
              << localAddr.toIpPort();
    ioChannelPtr_->setReadCallback(
        std::bind(&TcpConnectionImpl::readCallback, this));
    ioChannelPtr_->setWriteCallback(
        std::bind(&TcpConnectionImpl::writeCallback, this));
    ioChannelPtr_->setCloseCallback(
        std::bind(&TcpConnectionImpl::handleClose, this));
    ioChannelPtr_->setErrorCallback(
        std::bind(&TcpConnectionImpl::handleError, this));
    socketPtr_->setKeepAlive(true);
    name_ = localAddr.toIpPort() + "--" + peerAddr.toIpPort();
    sslEncryptionPtr_ = std::make_unique<SSLEncryption>();
    sslEncryptionPtr_->sslPtr_ = std::make_unique<SSLConn>(ctxPtr->get());
    sslEncryptionPtr_->isServer_ = isServer;
    validateCert_ = validateCert;
    if (isServer == false)
        SSL_set_verify(sslEncryptionPtr_->sslPtr_->get(),
                       SSL_VERIFY_NONE,
                       nullptr);
    if (!isServer && !hostname.empty())
    {
        SSL_set_tlsext_host_name(sslEncryptionPtr_->sslPtr_->get(),
                                 hostname.data());
        sslEncryptionPtr_->hostname_ = hostname;
    }
    assert(sslEncryptionPtr_->sslPtr_);
    auto r = SSL_set_fd(sslEncryptionPtr_->sslPtr_->get(), socketfd);
    (void)r;
    assert(r);
    isEncrypted_ = true;
    sslEncryptionPtr_->sendBufferPtr_ =
        std::make_unique<std::array<char, 8192>>();
}

bool TcpConnectionImpl::validatePeerCertificate()
{
    LOG_TRACE << "Validating peer cerificate";
    assert(sslEncryptionPtr_ != nullptr);
    assert(sslEncryptionPtr_->sslPtr_ != nullptr);
    SSL *ssl = sslEncryptionPtr_->sslPtr_->get();

    auto result = SSL_get_verify_result(ssl);
    if (result != X509_V_OK)
    {
        LOG_DEBUG << "cert error code: " << result;
        LOG_ERROR << "Server certificate is not valid";
        return false;
    }

    X509 *cert = SSL_get_peer_certificate(ssl);
    if (cert == nullptr)
    {
        LOG_ERROR << "Unable to obtain peer certificate";
        return false;
    }

    bool domainIsValid =
        internal::verifyCommonName(cert, sslEncryptionPtr_->hostname_) ||
        internal::verifyAltName(cert, sslEncryptionPtr_->hostname_);
    X509_free(cert);

    if (domainIsValid)
    {
        return true;
    }
    else
    {
        LOG_ERROR << "Domain validation failed";
        return false;
    }
}

void TcpConnectionImpl::doHandshaking()
{
    assert(sslEncryptionPtr_->statusOfSSL_ == SSLStatus::Handshaking);

    int r = SSL_do_handshake(sslEncryptionPtr_->sslPtr_->get());
    LOG_TRACE << "hand shaking: " << r;
    if (r == 1)
    {
        // Clients don't commonly have certificates. Let's not validate
        // that
        if (validateCert_ && sslEncryptionPtr_->isServer_ == false)
        {
            if (validatePeerCertificate() == false)
            {
                LOG_ERROR << "SSL certificate validation failed.";
                ioChannelPtr_->disableReading();
                sslEncryptionPtr_->statusOfSSL_ = SSLStatus::DisConnected;
                if (sslErrorCallback_)
                {
                    sslErrorCallback_(SSLError::kSSLInvalidCertificate);
                }
                forceClose();
                return;
            }
        }
        sslEncryptionPtr_->statusOfSSL_ = SSLStatus::Connected;
        if (sslEncryptionPtr_->isUpgrade_)
        {
            sslEncryptionPtr_->upgradeCallback_();
        }
        else
        {
            connectionCallback_(shared_from_this());
        }
        return;
    }
    int err = SSL_get_error(sslEncryptionPtr_->sslPtr_->get(), r);
    LOG_TRACE << "hand shaking: " << err;
    if (err == SSL_ERROR_WANT_WRITE)
    {  // SSL want writable;
        if (!ioChannelPtr_->isWriting())
            ioChannelPtr_->enableWriting();
        // ioChannelPtr_->disableReading();
    }
    else if (err == SSL_ERROR_WANT_READ)
    {  // SSL want readable;
        if (!ioChannelPtr_->isReading())
            ioChannelPtr_->enableReading();
        if (ioChannelPtr_->isWriting())
            ioChannelPtr_->disableWriting();
    }
    else
    {
        // ERR_print_errors(err);
        LOG_TRACE << "SSL handshake err: " << err;
        ioChannelPtr_->disableReading();
        sslEncryptionPtr_->statusOfSSL_ = SSLStatus::DisConnected;
        if (sslErrorCallback_)
        {
            sslErrorCallback_(SSLError::kSSLHandshakeError);
        }
        forceClose();
    }
}

#endif
