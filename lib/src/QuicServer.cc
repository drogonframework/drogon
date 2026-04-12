/**
 *
 *  @file QuicServer.cc
 *  @author S Bala Vignesh
 *
 *  Copyright 2026, S Bala Vignesh. All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#ifdef DROGON_HAS_HTTP3

#include "QuicServer.h"
#include "QuicConnection.h"
#include <trantor/utils/Logger.h>
#include <openssl/rand.h>
#include <cstring>
#include <cerrno>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace drogon
{

QuicServer::QuicServer(trantor::EventLoop *loop,
                       const trantor::InetAddress &listenAddr,
                       const std::string &certPath,
                       const std::string &keyPath)
    : loop_(loop), listenAddr_(listenAddr)
{
    // Generate random static secret for stateless reset tokens
    RAND_bytes(staticSecret_.data(), staticSecret_.size());

    if (!initTlsContext(certPath, keyPath))
    {
        LOG_FATAL << "Failed to initialize TLS context for QUIC";
        abort();
    }

    LOG_INFO << "QuicServer created for " << listenAddr.toIpPort();
}

QuicServer::~QuicServer()
{
    stop();
    if (sslCtx_)
    {
        SSL_CTX_free(sslCtx_);
        sslCtx_ = nullptr;
    }
}

bool QuicServer::initTlsContext(const std::string &certPath,
                                const std::string &keyPath)
{
    sslCtx_ = SSL_CTX_new(TLS_server_method());
    if (!sslCtx_)
    {
        LOG_ERROR << "SSL_CTX_new failed";
        return false;
    }

    // Require TLS 1.3 for QUIC
    SSL_CTX_set_min_proto_version(sslCtx_, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(sslCtx_, TLS1_3_VERSION);

    // Load certificate
    if (SSL_CTX_use_certificate_chain_file(sslCtx_, certPath.c_str()) != 1)
    {
        LOG_ERROR << "Failed to load certificate: " << certPath;
        return false;
    }

    // Load private key
    if (SSL_CTX_use_PrivateKey_file(
            sslCtx_, keyPath.c_str(), SSL_FILETYPE_PEM) != 1)
    {
        LOG_ERROR << "Failed to load private key: " << keyPath;
        return false;
    }

    // Set ALPN for h3 (mandatory for QUIC — RFC 9001 Section 8.1)
    static const uint8_t h3Alpn[] = {2, 'h', '3'};
    SSL_CTX_set_alpn_select_cb(
        sslCtx_,
        [](SSL *,
           const unsigned char **out,
           unsigned char *outlen,
           const unsigned char *in,
           unsigned int inlen,
           void *) -> int {
            if (SSL_select_next_proto(
                    const_cast<unsigned char **>(out),
                    outlen,
                    h3Alpn,
                    sizeof(h3Alpn),
                    in,
                    inlen) != OPENSSL_NPN_NEGOTIATED)
            {
                // QUIC mandates ALPN — must fail hard
                return SSL_TLSEXT_ERR_ALERT_FATAL;
            }
            return SSL_TLSEXT_ERR_OK;
        },
        nullptr);

    // Enable early data (0-RTT)
    SSL_CTX_set_max_early_data(sslCtx_, UINT32_MAX);

    // Configure TLS for QUIC — API differs between ngtcp2 backends
#ifdef DROGON_NGTCP2_CRYPTO_QUICTLS
    // Legacy quictls backend: configure at SSL_CTX level
    if (ngtcp2_crypto_quictls_configure_server_context(sslCtx_) != 0)
    {
        LOG_ERROR << "ngtcp2_crypto_quictls_configure_server_context failed";
        return false;
    }
#elif defined(DROGON_NGTCP2_CRYPTO_OSSL)
    // Modern ossl backend: initialize the library once.
    // Per-session configuration is done in QuicConnection::init().
    if (ngtcp2_crypto_ossl_init() != 0)
    {
        LOG_ERROR << "ngtcp2_crypto_ossl_init failed";
        return false;
    }
#endif

    LOG_INFO << "TLS context initialized for QUIC (TLS 1.3, h3 ALPN)";
    return true;
}

void QuicServer::start()
{
    loop_->assertInLoopThread();

    // Create UDP socket
    int domain = listenAddr_.isIpV6() ? AF_INET6 : AF_INET;
    udpFd_ = ::socket(domain, SOCK_DGRAM, 0);
    if (udpFd_ < 0)
    {
        LOG_FATAL << "Failed to create UDP socket: " << strerror(errno);
        abort();
    }

    // Set non-blocking
#ifndef _WIN32
    int flags = ::fcntl(udpFd_, F_GETFL, 0);
    ::fcntl(udpFd_, F_SETFL, flags | O_NONBLOCK);
#else
    u_long mode = 1;
    ioctlsocket(udpFd_, FIONBIO, &mode);
#endif

    // Allow address reuse
    int optval = 1;
    ::setsockopt(
        udpFd_, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof(optval));

    // Enable ECN (Explicit Congestion Notification) if available
#ifdef IP_RECVTOS
    ::setsockopt(
        udpFd_, IPPROTO_IP, IP_RECVTOS, (const char *)&optval, sizeof(optval));
#endif
#ifdef IPV6_RECVTCLASS
    if (domain == AF_INET6)
    {
        ::setsockopt(
            udpFd_, IPPROTO_IPV6, IPV6_RECVTCLASS,
            (const char *)&optval, sizeof(optval));
    }
#endif

    // Enable pktinfo for local address detection
#ifdef IP_PKTINFO
    ::setsockopt(
        udpFd_, IPPROTO_IP, IP_PKTINFO, (const char *)&optval, sizeof(optval));
#endif
#ifdef IPV6_RECVPKTINFO
    if (domain == AF_INET6)
    {
        ::setsockopt(
            udpFd_, IPPROTO_IPV6, IPV6_RECVPKTINFO,
            (const char *)&optval, sizeof(optval));
    }
#endif

    // Bind
    auto sa = listenAddr_.getSockAddr();
    socklen_t salen =
        domain == AF_INET6 ? sizeof(struct sockaddr_in6)
                           : sizeof(struct sockaddr_in);

    if (::bind(udpFd_, reinterpret_cast<const struct sockaddr *>(&sa),
               salen) < 0)
    {
        LOG_FATAL << "Failed to bind UDP socket to "
                  << listenAddr_.toIpPort() << ": " << strerror(errno);
#ifndef _WIN32
        ::close(udpFd_);
#else
        closesocket(udpFd_);
#endif
        udpFd_ = -1;
        abort();
    }

    // Register with event loop via Channel
    udpChannel_ = std::make_unique<trantor::Channel>(loop_, udpFd_);
    udpChannel_->setReadCallback([this] { onRead(); });
    udpChannel_->enableReading();

    LOG_INFO << "QuicServer listening on " << listenAddr_.toIpPort()
             << " (HTTP/3, UDP)";
}

void QuicServer::stop()
{
    if (udpChannel_)
    {
        udpChannel_->disableAll();
        udpChannel_->remove();
        udpChannel_.reset();
    }
    if (udpFd_ >= 0)
    {
#ifndef _WIN32
        ::close(udpFd_);
#else
        closesocket(udpFd_);
#endif
        udpFd_ = -1;
    }
    connections_.clear();
}

void QuicServer::onRead()
{
    struct sockaddr_storage remoteAddrStorage;
    socklen_t remoteAddrLen = sizeof(remoteAddrStorage);

    for (;;)
    {
        auto nread = ::recvfrom(
            udpFd_,
            reinterpret_cast<char *>(recvBuf_.data()),
            recvBuf_.size(),
            0,
            reinterpret_cast<struct sockaddr *>(&remoteAddrStorage),
            &remoteAddrLen);

        if (nread < 0)
        {
#ifndef _WIN32
            if (errno == EAGAIN || errno == EWOULDBLOCK)
#else
            if (WSAGetLastError() == WSAEWOULDBLOCK)
#endif
            {
                break;
            }
            LOG_ERROR << "recvfrom error: " << strerror(errno);
            break;
        }

        if (nread == 0)
        {
            break;
        }

        // Parse the packet header to get the DCID
        ngtcp2_version_cid vc;
        int rv = ngtcp2_pkt_decode_version_cid(
            &vc,
            recvBuf_.data(),
            static_cast<size_t>(nread),
            NGTCP2_MAX_CIDLEN);

        if (rv < 0)
        {
            LOG_TRACE << "Failed to decode QUIC packet version/CID";
            continue;
        }

        // Check if we support this version
        if (vc.version != 0 &&
            !ngtcp2_is_supported_version(vc.version))
        {
            // Send version negotiation
            ngtcp2_pkt_hd hd;
            hd.version = vc.version;
            memcpy(hd.dcid.data, vc.dcid, vc.dcidlen);
            hd.dcid.datalen = vc.dcidlen;
            memcpy(hd.scid.data, vc.scid, vc.scidlen);
            hd.scid.datalen = vc.scidlen;

            sendVersionNegotiation(
                hd,
                reinterpret_cast<struct sockaddr *>(&remoteAddrStorage),
                remoteAddrLen);
            continue;
        }

        // Look up existing connection by DCID
        ngtcp2_cid dcid;
        ngtcp2_cid_init(&dcid, vc.dcid, vc.dcidlen);

        auto *conn = findConnection(dcid);
        if (conn)
        {
            // Feed packet to existing connection
            ngtcp2_pkt_info pi;
            memset(&pi, 0, sizeof(pi));

            if (conn->onRead(
                    &pi,
                    recvBuf_.data(),
                    static_cast<size_t>(nread),
                    reinterpret_cast<struct sockaddr *>(&remoteAddrStorage),
                    remoteAddrLen) != 0)
            {
                removeConnection(conn->scid());
            }
            else
            {
                conn->onWrite();
            }
        }
        else
        {
            // New connection - only accept Initial packets
            ngtcp2_pkt_hd hd;
            int rv2 = ngtcp2_accept(&hd, recvBuf_.data(),
                                    static_cast<size_t>(nread));
            if (rv2 < 0)
            {
                LOG_TRACE << "ngtcp2_accept failed, not an Initial packet";
                continue;
            }

            // Get local address
            struct sockaddr_storage localAddrStorage;
            socklen_t localAddrLen = sizeof(localAddrStorage);
            ::getsockname(
                udpFd_,
                reinterpret_cast<struct sockaddr *>(&localAddrStorage),
                &localAddrLen);

            ngtcp2_cid scid;
            ngtcp2_cid_init(&scid, hd.scid.data, hd.scid.datalen);

            auto *newConn = createConnection(
                dcid,
                scid,
                reinterpret_cast<struct sockaddr *>(&remoteAddrStorage),
                remoteAddrLen,
                reinterpret_cast<struct sockaddr *>(&localAddrStorage),
                localAddrLen,
                hd.version);

            if (newConn)
            {
                ngtcp2_pkt_info pi;
                memset(&pi, 0, sizeof(pi));

                if (newConn->onRead(
                        &pi,
                        recvBuf_.data(),
                        static_cast<size_t>(nread),
                        reinterpret_cast<struct sockaddr *>(
                            &remoteAddrStorage),
                        remoteAddrLen) != 0)
                {
                    removeConnection(newConn->scid());
                }
                else
                {
                    newConn->onWrite();
                }
            }
        }
    }
}

QuicConnection *QuicServer::createConnection(
    const ngtcp2_cid &dcid,
    const ngtcp2_cid &scid,
    const struct sockaddr *remoteAddr,
    socklen_t remoteAddrLen,
    const struct sockaddr *localAddr,
    socklen_t localAddrLen,
    uint32_t version)
{
    // Generate a new server CID
    ngtcp2_cid serverCid;
    serverCid.datalen = NGTCP2_MAX_CIDLEN;
    RAND_bytes(serverCid.data, serverCid.datalen);

    auto conn = std::make_shared<QuicConnection>(
        this,
        loop_,
        dcid,
        serverCid,
        remoteAddr,
        remoteAddrLen,
        localAddr,
        localAddrLen,
        version,
        sslCtx_);

    conn->setRequestCallback(requestCallback_);

    if (!conn->init())
    {
        LOG_ERROR << "Failed to initialize QUIC connection";
        return nullptr;
    }

    auto *rawPtr = conn.get();
    connections_[serverCid] = std::move(conn);

    // Log the new connection (handle both IPv4 and IPv6)
    if (remoteAddr->sa_family == AF_INET6)
    {
        LOG_INFO << "New QUIC connection from "
                 << trantor::InetAddress(
                        *reinterpret_cast<const struct sockaddr_in6 *>(
                            remoteAddr))
                        .toIpPort();
    }
    else
    {
        LOG_INFO << "New QUIC connection from "
                 << trantor::InetAddress(
                        *reinterpret_cast<const struct sockaddr_in *>(
                            remoteAddr))
                        .toIpPort();
    }

    return rawPtr;
}

QuicConnection *QuicServer::findConnection(const ngtcp2_cid &dcid)
{
    auto it = connections_.find(dcid);
    if (it != connections_.end())
    {
        return it->second.get();
    }
    return nullptr;
}

void QuicServer::removeConnection(const ngtcp2_cid &dcid)
{
    auto it = connections_.find(dcid);
    if (it != connections_.end())
    {
        LOG_INFO << "QUIC connection closed";
        connections_.erase(it);
    }
}

ssize_t QuicServer::sendPacket(const struct sockaddr *remoteAddr,
                               socklen_t remoteAddrLen,
                               const uint8_t *data,
                               size_t datalen)
{
    auto nwrite = ::sendto(
        udpFd_,
        reinterpret_cast<const char *>(data),
        datalen,
        0,
        remoteAddr,
        remoteAddrLen);

    if (nwrite < 0)
    {
        LOG_ERROR << "sendto error: " << strerror(errno);
    }

    return nwrite;
}

void QuicServer::sendVersionNegotiation(const ngtcp2_pkt_hd &hd,
                                        const struct sockaddr *remoteAddr,
                                        socklen_t remoteAddrLen)
{
    std::array<uint8_t, 1200> buf;

    // Supported QUIC versions to offer in Version Negotiation
    static const uint32_t supportedVersions[] = {
        NGTCP2_PROTO_VER_V1,
        NGTCP2_PROTO_VER_V2,
    };

    auto nwrite = ngtcp2_pkt_write_version_negotiation(
        buf.data(),
        buf.size(),
        0,  // unused
        hd.scid.data,
        hd.scid.datalen,
        hd.dcid.data,
        hd.dcid.datalen,
        supportedVersions,
        sizeof(supportedVersions) / sizeof(supportedVersions[0]));

    if (nwrite > 0)
    {
        sendPacket(remoteAddr, remoteAddrLen,
                   buf.data(), static_cast<size_t>(nwrite));
    }
}

void QuicServer::sendRetry(const ngtcp2_pkt_hd &hd,
                           const struct sockaddr *remoteAddr,
                           socklen_t remoteAddrLen)
{
    ngtcp2_cid newScid;
    newScid.datalen = NGTCP2_MAX_CIDLEN;
    RAND_bytes(newScid.data, newScid.datalen);

    std::array<uint8_t, 256> token;
    // In production: generate a proper retry token
    RAND_bytes(token.data(), token.size());

    std::array<uint8_t, 1200> buf;
    auto nwrite = ngtcp2_crypto_write_retry(
        buf.data(),
        buf.size(),
        hd.version,
        &hd.scid,
        &newScid,
        &hd.dcid,
        token.data(),
        token.size());

    if (nwrite > 0)
    {
        sendPacket(remoteAddr, remoteAddrLen,
                   buf.data(), static_cast<size_t>(nwrite));
    }
}

bool QuicServer::generateStatelessResetToken(uint8_t *token,
                                             const ngtcp2_cid &cid)
{
    return ngtcp2_crypto_generate_stateless_reset_token(
               token,
               staticSecret_.data(),
               staticSecret_.size(),
               &cid) == 0;
}

}  // namespace drogon

#endif  // DROGON_HAS_HTTP3
