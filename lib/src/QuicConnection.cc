/**
 *
 *  @file QuicConnection.cc
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

#include "QuicConnection.h"
#include "Http3Handler.h"
#include <trantor/utils/Logger.h>
#include <openssl/rand.h>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <chrono>

namespace
{

/**
 * @brief Get current timestamp in nanoseconds for ngtcp2.
 * ngtcp2 requires timestamps in nanoseconds from a monotonic clock.
 */
ngtcp2_tstamp quicTimestamp()
{
    auto now = std::chrono::steady_clock::now();
    return static_cast<ngtcp2_tstamp>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch())
            .count());
}

}  // anonymous namespace

namespace drogon
{

QuicConnection::QuicConnection(QuicServer *server,
                               trantor::EventLoop *loop,
                               const ngtcp2_cid &dcid,
                               const ngtcp2_cid &scid,
                               const struct sockaddr *remoteAddr,
                               socklen_t remoteAddrLen,
                               const struct sockaddr *localAddr,
                               socklen_t localAddrLen,
                               uint32_t version,
                               SSL_CTX *sslCtx)
    : server_(server),
      loop_(loop),
      dcid_(dcid),
      scid_(scid),
      remoteAddrLen_(remoteAddrLen),
      localAddrLen_(localAddrLen),
      version_(version)
{
    memcpy(&remoteAddr_, remoteAddr, remoteAddrLen);
    memcpy(&localAddr_, localAddr, localAddrLen);

    // Create SSL object
    ssl_ = SSL_new(sslCtx);
    if (!ssl_)
    {
        LOG_ERROR << "SSL_new failed";
        return;
    }

    SSL_set_accept_state(ssl_);

#ifdef DROGON_NGTCP2_CRYPTO_QUICTLS
    // Legacy quictls: setup ngtcp2_crypto_conn_ref so TLS callbacks
    // can find our ngtcp2_conn.
    connRef_.get_conn = [](ngtcp2_crypto_conn_ref *ref) -> ngtcp2_conn * {
        auto *self = static_cast<QuicConnection *>(ref->user_data);
        return self->conn_;
    };
    connRef_.user_data = this;
    SSL_set_app_data(ssl_, &connRef_);
#elif defined(DROGON_NGTCP2_CRYPTO_OSSL)
    // Modern ossl: configure the SSL session for QUIC server use
    ngtcp2_crypto_ossl_configure_server_session(ssl_);
#endif
}

QuicConnection::~QuicConnection()
{
    if (retransTimerId_ != trantor::InvalidTimerId)
    {
        loop_->invalidateTimer(retransTimerId_);
    }
    if (h3conn_)
    {
        nghttp3_conn_del(h3conn_);
    }
    if (conn_)
    {
        ngtcp2_conn_del(conn_);
    }
    if (ssl_)
    {
        SSL_free(ssl_);
    }
}

bool QuicConnection::init()
{
    // Setup ngtcp2 callbacks
    ngtcp2_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));

    // Note: ngtcp2_crypto_quictls_configure_server_context is called once
    // in QuicServer::initTlsContext(), not per-connection.

    callbacks.recv_stream_data = onRecvStreamData;
    callbacks.acked_stream_data_offset = onAckedStreamDataOffset;
    callbacks.stream_open = onStreamOpen;
    callbacks.stream_close = onStreamClose;
    callbacks.extend_max_remote_streams_bidi = onExtendMaxStreamsBidi;
    callbacks.extend_max_stream_data = onExtendMaxStreamData;
    callbacks.rand = onRandCallback;
    callbacks.get_new_connection_id = onGetNewConnectionId;
    callbacks.handshake_completed = onHandshakeCompleted;

    // Use ngtcp2_crypto callbacks for TLS
    callbacks.recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb;
    callbacks.encrypt = ngtcp2_crypto_encrypt_cb;
    callbacks.decrypt = ngtcp2_crypto_decrypt_cb;
    callbacks.hp_mask = ngtcp2_crypto_hp_mask_cb;
    callbacks.recv_retry = ngtcp2_crypto_recv_retry_cb;
    callbacks.update_key = ngtcp2_crypto_update_key_cb;
    callbacks.delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb;
    callbacks.delete_crypto_cipher_ctx =
        ngtcp2_crypto_delete_crypto_cipher_ctx_cb;
    callbacks.get_path_challenge_data =
        ngtcp2_crypto_get_path_challenge_data_cb;
    callbacks.version_negotiation = ngtcp2_crypto_version_negotiation_cb;

    // Setup connection settings (separate from transport params in modern
    // ngtcp2)
    ngtcp2_settings settings;
    ngtcp2_settings_default(&settings);
    settings.initial_ts = quicTimestamp();
    settings.log_printf = nullptr;  // Enable for debug: ngtcp2_log_printf

    // Transport parameters (separate struct in modern ngtcp2)
    ngtcp2_transport_params params;
    ngtcp2_transport_params_default(&params);
    params.initial_max_data = 1024 * 1024;  // 1MB
    params.initial_max_stream_data_bidi_local = 256 * 1024;   // 256KB
    params.initial_max_stream_data_bidi_remote = 256 * 1024;
    params.initial_max_stream_data_uni = 256 * 1024;
    params.initial_max_streams_bidi = 100;
    params.initial_max_streams_uni = 3;
    params.max_idle_timeout = 30 * NGTCP2_SECONDS;

    // Generate stateless reset token
    server_->generateStatelessResetToken(
        params.stateless_reset_token, scid_);
    params.stateless_reset_token_present = 1;

    // Path
    ngtcp2_path path;
    path.local.addr = reinterpret_cast<ngtcp2_sockaddr *>(&localAddr_);
    path.local.addrlen = localAddrLen_;
    path.remote.addr = reinterpret_cast<ngtcp2_sockaddr *>(&remoteAddr_);
    path.remote.addrlen = remoteAddrLen_;

    // Create ngtcp2 server connection.
    // Note: ngtcp2_conn_server_new is a macro that expands to
    // ngtcp2_conn_server_new_versioned with version constants.
    int rv = ngtcp2_conn_server_new(
        &conn_,
        &dcid_,
        &scid_,
        &path,
        version_,
        &callbacks,
        &settings,
        &params,
        nullptr,  // mem allocator
        this);     // user_data

    if (rv != 0)
    {
        LOG_ERROR << "ngtcp2_conn_server_new failed: "
                  << ngtcp2_strerror(rv);
        return false;
    }

    // Set the TLS native handle so ngtcp2 can drive the TLS handshake
    ngtcp2_conn_set_tls_native_handle(conn_, ssl_);

    LOG_TRACE << "QUIC connection initialized, version: 0x"
              << std::hex << version_ << std::dec;

    return true;
}

// ---- ngtcp2 Callbacks ----

int QuicConnection::onRecvStreamData(ngtcp2_conn *conn,
                                     uint32_t flags,
                                     int64_t stream_id,
                                     uint64_t offset,
                                     const uint8_t *data,
                                     size_t datalen,
                                     void *user_data,
                                     void *stream_user_data)
{
    auto *self = static_cast<QuicConnection *>(user_data);
    (void)conn;
    (void)offset;
    (void)stream_user_data;

    if (self->h3conn_)
    {
        auto nconsumed = nghttp3_conn_read_stream(
            self->h3conn_,
            stream_id,
            data,
            datalen,
            flags & NGTCP2_STREAM_DATA_FLAG_FIN);

        if (nconsumed < 0)
        {
            LOG_ERROR << "nghttp3_conn_read_stream failed: "
                      << nghttp3_strerror(static_cast<int>(nconsumed));
            return NGTCP2_ERR_CALLBACK_FAILURE;
        }

        ngtcp2_conn_extend_max_stream_offset(
            self->conn_,
            stream_id,
            static_cast<uint64_t>(nconsumed));
        ngtcp2_conn_extend_max_offset(
            self->conn_,
            static_cast<uint64_t>(nconsumed));
    }

    return 0;
}

int QuicConnection::onAckedStreamDataOffset(ngtcp2_conn *conn,
                                            int64_t stream_id,
                                            uint64_t offset,
                                            uint64_t datalen,
                                            void *user_data,
                                            void *stream_user_data)
{
    auto *self = static_cast<QuicConnection *>(user_data);
    (void)conn;
    (void)offset;
    (void)stream_user_data;

    if (self->h3conn_)
    {
        int rv = nghttp3_conn_add_ack_offset(
            self->h3conn_, stream_id, datalen);
        if (rv != 0)
        {
            LOG_ERROR << "nghttp3_conn_add_ack_offset failed: "
                      << nghttp3_strerror(rv);
            return NGTCP2_ERR_CALLBACK_FAILURE;
        }
    }

    return 0;
}

int QuicConnection::onStreamOpen(ngtcp2_conn *conn,
                                 int64_t stream_id,
                                 void *user_data)
{
    (void)conn;
    (void)stream_id;
    (void)user_data;
    return 0;
}

int QuicConnection::onStreamClose(ngtcp2_conn *conn,
                                  uint32_t flags,
                                  int64_t stream_id,
                                  uint64_t app_error_code,
                                  void *user_data,
                                  void *stream_user_data)
{
    auto *self = static_cast<QuicConnection *>(user_data);
    (void)conn;
    (void)flags;
    (void)stream_user_data;

    if (self->h3conn_)
    {
        int rv = nghttp3_conn_close_stream(
            self->h3conn_, stream_id, app_error_code);
        if (rv != 0 && rv != NGHTTP3_ERR_STREAM_NOT_FOUND)
        {
            LOG_ERROR << "nghttp3_conn_close_stream failed: "
                      << nghttp3_strerror(rv);
            return NGTCP2_ERR_CALLBACK_FAILURE;
        }
    }

    // Clean up stream data
    self->streams_.erase(stream_id);
    self->pendingResponses_.erase(stream_id);

    return 0;
}

int QuicConnection::onExtendMaxStreamsBidi(ngtcp2_conn *conn,
                                           uint64_t max_streams,
                                           void *user_data)
{
    (void)conn;
    (void)max_streams;
    (void)user_data;
    return 0;
}

int QuicConnection::onExtendMaxStreamData(ngtcp2_conn *conn,
                                          int64_t stream_id,
                                          uint64_t max_data,
                                          void *user_data,
                                          void *stream_user_data)
{
    auto *self = static_cast<QuicConnection *>(user_data);
    (void)conn;
    (void)max_data;
    (void)stream_user_data;

    if (self->h3conn_)
    {
        int rv = nghttp3_conn_unblock_stream(self->h3conn_, stream_id);
        if (rv != 0 && rv != NGHTTP3_ERR_STREAM_NOT_FOUND)
        {
            LOG_ERROR << "nghttp3_conn_unblock_stream failed";
            return NGTCP2_ERR_CALLBACK_FAILURE;
        }
    }

    return 0;
}

void QuicConnection::onRandCallback(uint8_t *dest,
                                    size_t destlen,
                                    const ngtcp2_rand_ctx *rand_ctx)
{
    (void)rand_ctx;
    RAND_bytes(dest, destlen);
}

int QuicConnection::onGetNewConnectionId(ngtcp2_conn *conn,
                                         ngtcp2_cid *cid,
                                         uint8_t *token,
                                         size_t cidlen,
                                         void *user_data)
{
    auto *self = static_cast<QuicConnection *>(user_data);
    (void)conn;

    cid->datalen = cidlen;
    RAND_bytes(cid->data, cidlen);

    // Generate stateless reset token for this CID
    self->server_->generateStatelessResetToken(token, *cid);

    // Associate new CID with this connection in server
    self->server_->connections_[*cid] = self->shared_from_this();

    return 0;
}

int QuicConnection::onHandshakeCompleted(ngtcp2_conn *conn,
                                         void *user_data)
{
    auto *self = static_cast<QuicConnection *>(user_data);
    (void)conn;

    self->handshakeCompleted_ = true;
    LOG_INFO << "QUIC handshake completed";

    // Setup HTTP/3 now that the handshake is done
    int rv = self->setupHttp3Connection();
    if (rv != 0)
    {
        LOG_ERROR << "Failed to setup HTTP/3 connection";
        return NGTCP2_ERR_CALLBACK_FAILURE;
    }

    return 0;
}

// ---- nghttp3 Callbacks ----

int QuicConnection::onH3RecvHeader(nghttp3_conn *conn,
                                   int64_t stream_id,
                                   int32_t token,
                                   nghttp3_rcbuf *name,
                                   nghttp3_rcbuf *value,
                                   uint8_t flags,
                                   void *user_data,
                                   void *stream_user_data)
{
    auto *self = static_cast<QuicConnection *>(user_data);
    (void)conn;
    (void)token;
    (void)flags;
    (void)stream_user_data;

    auto nameVec = nghttp3_rcbuf_get_buf(name);
    auto valueVec = nghttp3_rcbuf_get_buf(value);

    std::string headerName(
        reinterpret_cast<const char *>(nameVec.base), nameVec.len);
    std::string headerValue(
        reinterpret_cast<const char *>(valueVec.base), valueVec.len);

    auto &stream = self->streams_[stream_id];

    // Create request on first header
    if (!stream.request)
    {
        stream.request =
            std::make_shared<HttpRequestImpl>(nullptr);
        stream.request->setVersion(drogon::Version::kHttp3);
        // HTTP/3 is always encrypted (QUIC uses TLS 1.3)
        stream.request->setSecure(true);
        // Set peer/local addresses for logging and IP-based middleware
        if (self->remoteAddr_.sa.sa_family == AF_INET6)
        {
            stream.request->setPeerAddr(
                trantor::InetAddress(self->remoteAddr_.in6));
            stream.request->setLocalAddr(
                trantor::InetAddress(self->localAddr_.in6));
        }
        else
        {
            stream.request->setPeerAddr(
                trantor::InetAddress(self->remoteAddr_.in));
            stream.request->setLocalAddr(
                trantor::InetAddress(self->localAddr_.in));
        }
    }

    // Handle pseudo-headers
    if (headerName == ":method")
    {
        stream.request->setMethod(
            Http3Handler::methodFromString(headerValue));
    }
    else if (headerName == ":path")
    {
        stream.request->setPath(headerValue);
    }
    else if (headerName == ":authority")
    {
        stream.request->addHeader("host", headerValue);
    }
    else if (headerName == ":scheme")
    {
        // Store scheme info if needed
    }
    else
    {
        // Regular header
        stream.request->addHeader(headerName, headerValue);
    }

    return 0;
}

int QuicConnection::onH3EndHeaders(nghttp3_conn *conn,
                                   int64_t stream_id,
                                   int fin,
                                   void *user_data,
                                   void *stream_user_data)
{
    auto *self = static_cast<QuicConnection *>(user_data);
    (void)conn;
    (void)fin;
    (void)stream_user_data;

    auto it = self->streams_.find(stream_id);
    if (it != self->streams_.end())
    {
        it->second.headersComplete = true;
    }

    return 0;
}

int QuicConnection::onH3RecvData(nghttp3_conn *conn,
                                 int64_t stream_id,
                                 const uint8_t *data,
                                 size_t datalen,
                                 void *user_data,
                                 void *stream_user_data)
{
    auto *self = static_cast<QuicConnection *>(user_data);
    (void)conn;
    (void)stream_user_data;

    auto it = self->streams_.find(stream_id);
    if (it != self->streams_.end())
    {
        it->second.body.append(
            reinterpret_cast<const char *>(data), datalen);
    }

    return 0;
}

int QuicConnection::onH3EndStream(nghttp3_conn *conn,
                                  int64_t stream_id,
                                  void *user_data,
                                  void *stream_user_data)
{
    auto *self = static_cast<QuicConnection *>(user_data);
    (void)conn;
    (void)stream_user_data;

    auto it = self->streams_.find(stream_id);
    if (it == self->streams_.end() || !it->second.request)
    {
        return 0;
    }

    auto &stream = it->second;

    // Set the body on the request
    if (!stream.body.empty())
    {
        stream.request->setBody(stream.body);
    }

    LOG_TRACE << "HTTP/3 request received: "
              << stream.request->methodString() << " "
              << stream.request->path();

    // Dispatch through the same pipeline as HTTP/1.1
    if (self->requestCallback_)
    {
        auto weakSelf = self->weak_from_this();
        auto capturedStreamId = stream_id;

        self->requestCallback_(
            stream.request,
            [weakSelf, capturedStreamId](const HttpResponsePtr &resp) {
                auto self2 = weakSelf.lock();
                if (self2)
                {
                    self2->sendResponse(capturedStreamId, resp);
                }
            });
    }

    return 0;
}

int QuicConnection::onH3StopSending(nghttp3_conn *conn,
                                    int64_t stream_id,
                                    uint64_t app_error_code,
                                    void *user_data,
                                    void *stream_user_data)
{
    auto *self = static_cast<QuicConnection *>(user_data);
    (void)conn;
    (void)stream_user_data;

    ngtcp2_conn_shutdown_stream_read(
        self->conn_, 0, stream_id, app_error_code);
    return 0;
}

int QuicConnection::onH3ResetStream(nghttp3_conn *conn,
                                    int64_t stream_id,
                                    uint64_t app_error_code,
                                    void *user_data,
                                    void *stream_user_data)
{
    auto *self = static_cast<QuicConnection *>(user_data);
    (void)conn;
    (void)stream_user_data;

    ngtcp2_conn_shutdown_stream_write(
        self->conn_, 0, stream_id, app_error_code);
    return 0;
}

int QuicConnection::onH3AckedStreamData(nghttp3_conn *conn,
                                        int64_t stream_id,
                                        uint64_t datalen,
                                        void *user_data,
                                        void *stream_user_data)
{
    auto *self = static_cast<QuicConnection *>(user_data);
    (void)conn;
    (void)stream_user_data;

    auto it = self->pendingResponses_.find(stream_id);
    if (it != self->pendingResponses_.end())
    {
        // Track acknowledged data for potential cleanup
        // For now, cleanup happens on stream close
    }

    return 0;
}

// ---- Internal Methods ----

int QuicConnection::setupHttp3Connection()
{
    if (h3conn_)
    {
        return 0;  // Already setup
    }

    nghttp3_callbacks h3Callbacks;
    memset(&h3Callbacks, 0, sizeof(h3Callbacks));

    h3Callbacks.recv_header = onH3RecvHeader;
    h3Callbacks.end_headers = onH3EndHeaders;
    h3Callbacks.recv_data = onH3RecvData;
    h3Callbacks.end_stream = onH3EndStream;
    h3Callbacks.stop_sending = onH3StopSending;
    h3Callbacks.reset_stream = onH3ResetStream;
    h3Callbacks.acked_stream_data = onH3AckedStreamData;

    nghttp3_settings h3Settings;
    nghttp3_settings_default(&h3Settings);
    h3Settings.qpack_max_dtable_capacity = 4096;
    h3Settings.qpack_blocked_streams = 100;

    int rv = nghttp3_conn_server_new(
        &h3conn_,
        &h3Callbacks,
        &h3Settings,
        nullptr,  // mem allocator
        this);     // user_data

    if (rv != 0)
    {
        LOG_ERROR << "nghttp3_conn_server_new failed: "
                  << nghttp3_strerror(rv);
        return -1;
    }

    // Bind control and QPACK streams
    // These are unidirectional streams required by HTTP/3
    int64_t ctrlStreamId, qpackEncStreamId, qpackDecStreamId;

    rv = ngtcp2_conn_open_uni_stream(conn_, &ctrlStreamId, nullptr);
    if (rv != 0)
    {
        LOG_ERROR << "Failed to open control stream";
        return -1;
    }

    rv = ngtcp2_conn_open_uni_stream(conn_, &qpackEncStreamId, nullptr);
    if (rv != 0)
    {
        LOG_ERROR << "Failed to open QPACK encoder stream";
        return -1;
    }

    rv = ngtcp2_conn_open_uni_stream(conn_, &qpackDecStreamId, nullptr);
    if (rv != 0)
    {
        LOG_ERROR << "Failed to open QPACK decoder stream";
        return -1;
    }

    rv = nghttp3_conn_bind_control_stream(h3conn_, ctrlStreamId);
    if (rv != 0)
    {
        LOG_ERROR << "nghttp3_conn_bind_control_stream failed: "
                  << nghttp3_strerror(rv);
        return -1;
    }

    rv = nghttp3_conn_bind_qpack_streams(
        h3conn_, qpackEncStreamId, qpackDecStreamId);
    if (rv != 0)
    {
        LOG_ERROR << "nghttp3_conn_bind_qpack_streams failed: "
                  << nghttp3_strerror(rv);
        return -1;
    }

    LOG_TRACE << "HTTP/3 session initialized (ctrl=" << ctrlStreamId
              << ", qenc=" << qpackEncStreamId
              << ", qdec=" << qpackDecStreamId << ")";

    return 0;
}

void QuicConnection::sendResponse(int64_t streamId,
                                  const HttpResponsePtr &resp)
{
    if (!h3conn_ || !conn_)
    {
        return;
    }

    // Serialize headers
    auto serialized = Http3Handler::serializeResponseHeaders(resp);

    // Get body
    auto [bodyData, bodyLen] = Http3Handler::getResponseBody(resp);

    // Store pending response data
    auto &pending = pendingResponses_[streamId];
    if (bodyLen > 0)
    {
        pending.body.assign(
            reinterpret_cast<const char *>(bodyData), bodyLen);
    }

    // Create nghttp3 data reader if there's a body
    nghttp3_data_reader dataReader;
    nghttp3_data_reader *dataReaderPtr = nullptr;

    if (bodyLen > 0)
    {
        dataReader.read_data = [](nghttp3_conn *conn,
                                  int64_t stream_id,
                                  nghttp3_vec *vec,
                                  size_t veccnt,
                                  uint32_t *pflags,
                                  void *user_data,
                                  void *stream_user_data) -> nghttp3_ssize {
            auto *self = static_cast<QuicConnection *>(user_data);
            (void)conn;
            (void)stream_user_data;

            auto it = self->pendingResponses_.find(stream_id);
            if (it == self->pendingResponses_.end())
            {
                *pflags = NGHTTP3_DATA_FLAG_EOF;
                return 0;
            }

            auto &resp = it->second;
            if (resp.bodySent >= resp.body.size())
            {
                *pflags = NGHTTP3_DATA_FLAG_EOF;
                return 0;
            }

            size_t remaining = resp.body.size() - resp.bodySent;
            vec[0].base = reinterpret_cast<uint8_t *>(
                const_cast<char *>(resp.body.data() + resp.bodySent));
            vec[0].len = remaining;
            resp.bodySent += remaining;

            *pflags = NGHTTP3_DATA_FLAG_EOF;
            return 1;
        };
        dataReaderPtr = &dataReader;
    }

    // Submit response headers
    int rv = nghttp3_conn_submit_response(
        h3conn_,
        streamId,
        serialized.nva.data(),
        serialized.nva.size(),
        dataReaderPtr);

    if (rv != 0)
    {
        LOG_ERROR << "nghttp3_conn_submit_response failed: "
                  << nghttp3_strerror(rv);
        return;
    }

    LOG_TRACE << "HTTP/3 response submitted for stream " << streamId
              << " (status " << resp->statusCode()
              << ", body " << bodyLen << " bytes)";

    // Trigger write
    onWrite();
}

int QuicConnection::onRead(const ngtcp2_pkt_info *pi,
                           const uint8_t *data,
                           size_t datalen,
                           const struct sockaddr *remoteAddr,
                           socklen_t remoteAddrLen)
{
    ngtcp2_path path;
    path.local.addr = reinterpret_cast<ngtcp2_sockaddr *>(&localAddr_);
    path.local.addrlen = localAddrLen_;

    ngtcp2_sockaddr_union tmpRemote;
    memcpy(&tmpRemote, remoteAddr, remoteAddrLen);
    path.remote.addr = reinterpret_cast<ngtcp2_sockaddr *>(&tmpRemote);
    path.remote.addrlen = remoteAddrLen;

    int rv = ngtcp2_conn_read_pkt(
        conn_, &path, pi, data, datalen, quicTimestamp());

    if (rv != 0)
    {
        LOG_ERROR << "ngtcp2_conn_read_pkt failed: "
                  << ngtcp2_strerror(rv);
        if (!ngtcp2_err_is_fatal(rv))
        {
            return 0;
        }
        return -1;
    }

    updateTimer();
    return 0;
}

int QuicConnection::onWrite()
{
    return writePackets();
}

int QuicConnection::writePackets()
{
    if (draining_)
    {
        return 0;
    }

    ngtcp2_path_storage ps;
    ngtcp2_path_storage_zero(&ps);

    ngtcp2_pkt_info pi;

    for (;;)
    {
        int64_t streamId = -1;
        nghttp3_vec vec[16];
        nghttp3_ssize sveccnt = 0;
        int fin = 0;

        // Ask nghttp3 if it has data to send
        if (h3conn_)
        {
            sveccnt = nghttp3_conn_writev_stream(
                h3conn_,
                &streamId,
                &fin,
                vec,
                16);

            if (sveccnt < 0)
            {
                LOG_ERROR << "nghttp3_conn_writev_stream failed: "
                          << nghttp3_strerror(static_cast<int>(sveccnt));
                return -1;
            }
        }

        uint32_t flags = NGTCP2_WRITE_STREAM_FLAG_MORE;
        if (fin)
        {
            flags |= NGTCP2_WRITE_STREAM_FLAG_FIN;
        }

        ngtcp2_vec ntVec[16];
        for (nghttp3_ssize i = 0; i < sveccnt; ++i)
        {
            ntVec[i].base = vec[i].base;
            ntVec[i].len = vec[i].len;
        }

        ngtcp2_ssize ndatalen = 0;
        auto nwrite = ngtcp2_conn_writev_stream(
            conn_,
            &ps.path,
            &pi,
            sendBuf_.data(),
            sendBuf_.size(),
            &ndatalen,
            flags,
            streamId,
            reinterpret_cast<const ngtcp2_vec *>(ntVec),
            static_cast<size_t>(sveccnt),
            quicTimestamp());

        if (nwrite < 0)
        {
            if (nwrite == NGTCP2_ERR_WRITE_MORE)
            {
                // ngtcp2 consumed ndatalen bytes of stream data but needs
                // more before it can produce a packet. Tell nghttp3.
                if (h3conn_ && streamId >= 0 && ndatalen >= 0)
                {
                    nghttp3_conn_add_write_offset(
                        h3conn_, streamId, ndatalen);
                }
                continue;
            }
            LOG_ERROR << "ngtcp2_conn_writev_stream failed: "
                      << ngtcp2_strerror(static_cast<int>(nwrite));
            return -1;
        }

        if (nwrite == 0)
        {
            // No more data to send
            break;
        }

        // Send the packet
        server_->sendPacket(
            reinterpret_cast<struct sockaddr *>(&remoteAddr_),
            remoteAddrLen_,
            sendBuf_.data(),
            static_cast<size_t>(nwrite));

        // Tell nghttp3 how much stream data was consumed by ngtcp2
        if (h3conn_ && streamId >= 0 && ndatalen >= 0)
        {
            nghttp3_conn_add_write_offset(
                h3conn_, streamId, ndatalen);
        }
    }

    updateTimer();
    return 0;
}

int QuicConnection::handleExpiry()
{
    int rv = ngtcp2_conn_handle_expiry(conn_, quicTimestamp());
    if (rv != 0)
    {
        LOG_ERROR << "ngtcp2_conn_handle_expiry failed: "
                  << ngtcp2_strerror(rv);
        return -1;
    }
    return onWrite();
}

void QuicConnection::updateTimer()
{
    if (retransTimerId_ != trantor::InvalidTimerId)
    {
        loop_->invalidateTimer(retransTimerId_);
        retransTimerId_ = trantor::InvalidTimerId;
    }

    auto expiry = ngtcp2_conn_get_expiry(conn_);
    auto now = quicTimestamp();

    if (expiry <= now)
    {
        // Already expired, handle immediately
        loop_->queueInLoop([weak = weak_from_this()] {
            auto self = weak.lock();
            if (self)
            {
                if (self->handleExpiry() != 0)
                {
                    self->server_->removeConnection(self->scid());
                }
            }
        });
        return;
    }

    // Schedule timer
    double delaySec =
        static_cast<double>(expiry - now) / NGTCP2_SECONDS;
    if (delaySec < 0.001)
    {
        delaySec = 0.001;
    }

    retransTimerId_ = loop_->runAfter(
        delaySec,
        [weak = weak_from_this()] {
            auto self = weak.lock();
            if (self)
            {
                if (self->handleExpiry() != 0)
                {
                    self->server_->removeConnection(self->scid());
                }
            }
        });
}

}  // namespace drogon

#endif  // DROGON_HAS_HTTP3
