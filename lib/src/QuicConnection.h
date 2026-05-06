/**
 *
 *  @file QuicConnection.h
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

#include "QuicServer.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"

#include <trantor/net/EventLoop.h>
#include <trantor/utils/NonCopyable.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
// ngtcp2_crypto backend header is included via QuicServer.h
#include <nghttp3/nghttp3.h>
#include <openssl/ssl.h>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace drogon
{

/**
 * @brief Represents a single QUIC connection with an HTTP/3 session.
 *
 * Each QuicConnection wraps:
 *   - ngtcp2_conn: QUIC protocol state machine
 *   - nghttp3_conn: HTTP/3 framing layer
 *   - SSL: TLS 1.3 session for QUIC encryption
 *
 * Incoming HTTP/3 requests are converted into HttpRequestImpl objects
 * and dispatched through the same callback pipeline as HTTP/1.1.
 */
class QuicConnection : trantor::NonCopyable,
                       public std::enable_shared_from_this<QuicConnection>
{
  public:
    QuicConnection(QuicServer *server,
                   trantor::EventLoop *loop,
                   const ngtcp2_cid &dcid,
                   const ngtcp2_cid &scid,
                   const struct sockaddr *remoteAddr,
                   socklen_t remoteAddrLen,
                   const struct sockaddr *localAddr,
                   socklen_t localAddrLen,
                   uint32_t version,
                   SSL_CTX *sslCtx);

    ~QuicConnection();

    /**
     * @brief Initialize the QUIC connection and TLS handshake.
     * @return true on success
     */
    bool init();

    /**
     * @brief Feed a received UDP packet into this connection.
     * @param pi Packet info (ECN, etc.)
     * @param data Packet data
     * @param datalen Packet length
     * @param remoteAddr Remote address
     * @param remoteAddrLen Remote address length
     * @return 0 on success, negative on error
     */
    int onRead(const ngtcp2_pkt_info *pi,
               const uint8_t *data,
               size_t datalen,
               const struct sockaddr *remoteAddr,
               socklen_t remoteAddrLen);

    /**
     * @brief Write pending QUIC packets to the UDP socket.
     * @return 0 on success, negative on error
     */
    int onWrite();

    /**
     * @brief Handle timer expiry for retransmission.
     * @return 0 on success, negative on error
     */
    int handleExpiry();

    /**
     * @brief Check if the connection is in draining state.
     */
    bool isDraining() const
    {
        return draining_;
    }

    /**
     * @brief Get the source CID (used as key in server's map).
     */
    const ngtcp2_cid &scid() const
    {
        return scid_;
    }

    /**
     * @brief Set the request callback for HTTP/3 requests.
     */
    void setRequestCallback(QuicRequestCallback cb)
    {
        requestCallback_ = std::move(cb);
    }

  private:
    // ---- ngtcp2 callbacks (static, dispatched to instance) ----

    static int onRecvStreamData(ngtcp2_conn *conn,
                                uint32_t flags,
                                int64_t stream_id,
                                uint64_t offset,
                                const uint8_t *data,
                                size_t datalen,
                                void *user_data,
                                void *stream_user_data);

    static int onAckedStreamDataOffset(ngtcp2_conn *conn,
                                       int64_t stream_id,
                                       uint64_t offset,
                                       uint64_t datalen,
                                       void *user_data,
                                       void *stream_user_data);

    static int onStreamOpen(ngtcp2_conn *conn,
                            int64_t stream_id,
                            void *user_data);

    static int onStreamClose(ngtcp2_conn *conn,
                             uint32_t flags,
                             int64_t stream_id,
                             uint64_t app_error_code,
                             void *user_data,
                             void *stream_user_data);

    static int onExtendMaxStreamsBidi(ngtcp2_conn *conn,
                                      uint64_t max_streams,
                                      void *user_data);

    static int onExtendMaxStreamData(ngtcp2_conn *conn,
                                     int64_t stream_id,
                                     uint64_t max_data,
                                     void *user_data,
                                     void *stream_user_data);

    static void onRandCallback(uint8_t *dest,
                               size_t destlen,
                               const ngtcp2_rand_ctx *rand_ctx);

    static int onGetNewConnectionId(ngtcp2_conn *conn,
                                    ngtcp2_cid *cid,
                                    uint8_t *token,
                                    size_t cidlen,
                                    void *user_data);

    static int onHandshakeCompleted(ngtcp2_conn *conn, void *user_data);

    // ---- nghttp3 callbacks ----

    static int onH3RecvHeader(nghttp3_conn *conn,
                              int64_t stream_id,
                              int32_t token,
                              nghttp3_rcbuf *name,
                              nghttp3_rcbuf *value,
                              uint8_t flags,
                              void *user_data,
                              void *stream_user_data);

    static int onH3EndHeaders(nghttp3_conn *conn,
                              int64_t stream_id,
                              int fin,
                              void *user_data,
                              void *stream_user_data);

    static int onH3RecvData(nghttp3_conn *conn,
                            int64_t stream_id,
                            const uint8_t *data,
                            size_t datalen,
                            void *user_data,
                            void *stream_user_data);

    static int onH3EndStream(nghttp3_conn *conn,
                             int64_t stream_id,
                             void *user_data,
                             void *stream_user_data);

    static int onH3StopSending(nghttp3_conn *conn,
                               int64_t stream_id,
                               uint64_t app_error_code,
                               void *user_data,
                               void *stream_user_data);

    static int onH3ResetStream(nghttp3_conn *conn,
                               int64_t stream_id,
                               uint64_t app_error_code,
                               void *user_data,
                               void *stream_user_data);

    static int onH3AckedStreamData(nghttp3_conn *conn,
                                   int64_t stream_id,
                                   uint64_t datalen,
                                   void *user_data,
                                   void *stream_user_data);

    // ---- Internal helpers ----

    /**
     * @brief Initialize the HTTP/3 session on this connection.
     */
    int setupHttp3Connection();

    /**
     * @brief Send an HTTP/3 response for a given stream.
     */
    void sendResponse(int64_t streamId, const HttpResponsePtr &resp);

    /**
     * @brief Write outgoing QUIC packets to UDP.
     */
    int writePackets();

    /**
     * @brief Schedule the retransmission timer.
     */
    void updateTimer();

    // Server back-pointer
    QuicServer *server_;
    trantor::EventLoop *loop_;

    // QUIC connection
    ngtcp2_conn *conn_{nullptr};
    ngtcp2_cid dcid_;
    ngtcp2_cid scid_;

    // TLS
    SSL *ssl_{nullptr};
    ngtcp2_crypto_conn_ref connRef_;  // Required by ngtcp2_crypto_quictls

    // HTTP/3 connection
    nghttp3_conn *h3conn_{nullptr};

    // Remote/local addresses
    ngtcp2_sockaddr_union remoteAddr_;
    socklen_t remoteAddrLen_;
    ngtcp2_sockaddr_union localAddr_;
    socklen_t localAddrLen_;

    // QUIC version
    uint32_t version_;

    // Connection state
    bool draining_{false};
    bool handshakeCompleted_{false};

    // Timer for retransmissions
    trantor::TimerId retransTimerId_{trantor::InvalidTimerId};

    // HTTP/3 stream state
    struct H3Stream
    {
        HttpRequestImplPtr request;
        std::string body;
        bool headersComplete{false};
    };
    std::unordered_map<int64_t, H3Stream> streams_;

    // Response data pending send
    struct PendingResponse
    {
        std::string headers;
        std::string body;
        size_t headersSent{0};
        size_t bodySent{0};
    };
    std::unordered_map<int64_t, PendingResponse> pendingResponses_;

    // Transmit buffer
    std::array<uint8_t, 65536> sendBuf_;

    // Request callback
    QuicRequestCallback requestCallback_;
};

}  // namespace drogon

#endif  // DROGON_HAS_HTTP3
