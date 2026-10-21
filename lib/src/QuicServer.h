/**
 *
 *  @file QuicServer.h
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

#pragma once

#ifdef DROGON_HAS_HTTP3

#include <trantor/net/EventLoop.h>
#include <trantor/net/Channel.h>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/utils/Logger.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
// ngtcp2 v1.x renamed crypto_quictls to crypto_ossl
#if __has_include(<ngtcp2/ngtcp2_crypto_quictls.h>)
#include <ngtcp2/ngtcp2_crypto_quictls.h>
#define DROGON_NGTCP2_CRYPTO_QUICTLS 1
#elif __has_include(<ngtcp2/ngtcp2_crypto_ossl.h>)
#include <ngtcp2/ngtcp2_crypto_ossl.h>
#define DROGON_NGTCP2_CRYPTO_OSSL 1
#endif
#include <nghttp3/nghttp3.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <array>
#include <vector>

namespace drogon
{
class QuicConnection;
class HttpRequestImpl;
class HttpResponse;

using HttpRequestImplPtr = std::shared_ptr<HttpRequestImpl>;
using HttpResponsePtr = std::shared_ptr<HttpResponse>;

/**
 * @brief Callback for processing HTTP requests received over QUIC.
 * The same signature as the existing HTTP/1.1 path so we can reuse
 * the routing pipeline.
 */
using QuicRequestCallback =
    std::function<void(const HttpRequestImplPtr &,
                       std::function<void(const HttpResponsePtr &)> &&)>;

/**
 * @brief QuicServer manages a UDP socket and dispatches incoming QUIC
 * packets to QuicConnection instances. It integrates with Trantor's
 * event loop via Channel for non-blocking I/O.
 *
 * Architecture:
 *   UDP Socket (Channel) -> QuicServer -> QuicConnection (per client)
 *                                         -> ngtcp2 (QUIC)
 *                                         -> nghttp3 (HTTP/3)
 *                                         -> HttpRequest pipeline
 */
class QuicServer : trantor::NonCopyable
{
  public:
    /**
     * @brief Construct a QuicServer.
     * @param loop The event loop to run in.
     * @param listenAddr The address to bind the UDP socket to.
     * @param certPath Path to the TLS certificate file.
     * @param keyPath Path to the TLS private key file.
     */
    QuicServer(trantor::EventLoop *loop,
               const trantor::InetAddress &listenAddr,
               const std::string &certPath,
               const std::string &keyPath);

    ~QuicServer();

    /**
     * @brief Start listening for QUIC connections.
     */
    void start();

    /**
     * @brief Stop and close the server.
     */
    void stop();

    /**
     * @brief Set the callback for processing HTTP/3 requests.
     * This should be set to HttpServer::onHttpRequest to reuse
     * the existing routing pipeline.
     */
    void setRequestCallback(QuicRequestCallback cb)
    {
        requestCallback_ = std::move(cb);
    }

    /**
     * @brief Set the IO event loops for distributing connections.
     */
    void setIoLoops(const std::vector<trantor::EventLoop *> &ioLoops)
    {
        ioLoops_ = ioLoops;
    }

    /**
     * @brief Get the listen address.
     */
    const trantor::InetAddress &address() const
    {
        return listenAddr_;
    }

  private:
    /**
     * @brief Called when the UDP socket is readable.
     * Reads packets and dispatches them to connections.
     */
    void onRead();

    /**
     * @brief Create a new QUIC connection for an incoming initial packet.
     */
    QuicConnection *createConnection(const ngtcp2_cid &dcid,
                                     const ngtcp2_cid &scid,
                                     const struct sockaddr *remoteAddr,
                                     socklen_t remoteAddrLen,
                                     const struct sockaddr *localAddr,
                                     socklen_t localAddrLen,
                                     uint32_t version);

    /**
     * @brief Look up an existing connection by destination CID.
     */
    QuicConnection *findConnection(const ngtcp2_cid &dcid);

    /**
     * @brief Remove a connection (called when it closes).
     */
    void removeConnection(const ngtcp2_cid &dcid);

    /**
     * @brief Send a UDP packet.
     */
    ssize_t sendPacket(const struct sockaddr *remoteAddr,
                       socklen_t remoteAddrLen,
                       const uint8_t *data,
                       size_t datalen);

    /**
     * @brief Send a version negotiation packet.
     */
    void sendVersionNegotiation(const ngtcp2_pkt_hd &hd,
                                const struct sockaddr *remoteAddr,
                                socklen_t remoteAddrLen);

    /**
     * @brief Send a stateless retry packet.
     */
    void sendRetry(const ngtcp2_pkt_hd &hd,
                   const struct sockaddr *remoteAddr,
                   socklen_t remoteAddrLen);

    /**
     * @brief Initialize the SSL context for QUIC/TLS 1.3.
     */
    bool initTlsContext(const std::string &certPath,
                        const std::string &keyPath);

    /**
     * @brief Generate a stateless reset token for a CID.
     */
    bool generateStatelessResetToken(uint8_t *token,
                                     const ngtcp2_cid &cid);

    // Event loop
    trantor::EventLoop *loop_;
    trantor::InetAddress listenAddr_;

    // UDP socket and Channel
    int udpFd_{-1};
    std::unique_ptr<trantor::Channel> udpChannel_;

    // TLS context
    SSL_CTX *sslCtx_{nullptr};

    // Connection map: DCID -> QuicConnection
    struct CidHash
    {
        size_t operator()(const ngtcp2_cid &cid) const
        {
            // FNV-1a hash over the CID bytes
            size_t hash = 14695981039346656037ULL;
            for (size_t i = 0; i < cid.datalen; ++i)
            {
                hash ^= static_cast<size_t>(cid.data[i]);
                hash *= 1099511628211ULL;
            }
            return hash;
        }
    };

    struct CidEqual
    {
        bool operator()(const ngtcp2_cid &a, const ngtcp2_cid &b) const
        {
            return ngtcp2_cid_eq(&a, &b);
        }
    };

    std::unordered_map<ngtcp2_cid,
                       std::shared_ptr<QuicConnection>,
                       CidHash,
                       CidEqual>
        connections_;

    // Static secret for stateless reset tokens
    std::array<uint8_t, 32> staticSecret_;

    // Request callback (shared with HttpServer)
    QuicRequestCallback requestCallback_;

    // IO loops for distributing work
    std::vector<trantor::EventLoop *> ioLoops_;
    size_t nextLoopIdx_{0};

    // Receive buffer
    std::array<uint8_t, 65536> recvBuf_;

    friend class QuicConnection;
};

}  // namespace drogon

#endif  // DROGON_HAS_HTTP3
