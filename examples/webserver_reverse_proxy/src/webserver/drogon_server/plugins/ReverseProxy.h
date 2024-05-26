#ifndef EXAMPLES_WEBSERVER_REVERSE_PROXY_SRC_WEBSERVER_DROGON_SERVER_PLUGINS_REVERSEPROXY_H_
#define EXAMPLES_WEBSERVER_REVERSE_PROXY_SRC_WEBSERVER_DROGON_SERVER_PLUGINS_REVERSEPROXY_H_
#include <drogon/drogon.h>

using namespace drogon;

/**
 * @brief type for backend ssl storage
 *
 */
struct TBackendSSL
{
    std::string key;
    std::string cert;
    bool use_old_tls;
    std::vector<std::pair<std::string, std::string>> configurations;
};

/**
 * @brief type for backend storage
 *
 */
struct TBackendStorage
{
    bool ssl;
    bool redirect;
    bool header_add;
    bool check_origin_whitelist;
    std::string host;
    TBackendSSL ssl_conf;
    std::string redirect_to;
    std::string document_root;
    std::string check_origin_whitelist_data;
    std::vector<std::string> internal_proxies;
    std::vector<std::pair<std::string, std::string>> header_add_list_pair;
};

/**
 * @brief reverse proxy plugin for drogon_server project
 *
 */
class ReverseProxy final : public Plugin<ReverseProxy>
{
  private:
    bool m_ready = false;
    bool m_useSSL = false;
    bool m_sameClientToSameBackend = false;

    size_t m_pipelining = 16;
    size_t m_connectionFactor = 1;

    std::vector<TBackendStorage> m_backends;

    std::string m_defaultDocumentRoot = "";

    IOThreadStorage<size_t> m_clientIndex = 0;
    IOThreadStorage<std::vector<HttpClientPtr>> m_clients;

  protected:
    /**
     * @brief pre routing request to array backend
     *
     * @param pReq
     * @param aCallback
     * @param acCallback
     */
    void PreRouting(const HttpRequestPtr &pReq,
                    AdviceCallback &&aCallback,
                    AdviceChainCallback &&acCallback);

  public:
    ReverseProxy(/* args */);
    ~ReverseProxy();

    void initAndStart(const Json::Value &config) override;
    void shutdown() override;
};

#endif  // EXAMPLES_WEBSERVER_REVERSE_PROXY_SRC_WEBSERVER_DROGON_SERVER_PLUGINS_REVERSEPROXY_H_
