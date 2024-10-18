#include "ReverseProxy.h"

void ReverseProxy::PreRouting(const HttpRequestPtr &pReq,
                              AdviceCallback &&aCallback,
                              AdviceChainCallback &&acCallback)
{
    size_t index = 0, index_be = 0;
    size_t count = -1;
    auto host = pReq->getHeader("host");

    auto &clientsVector = *m_clients;

    bool redirect = false;
    std::string redirectUrl = "";

    bool requestFound = false;

    // check for correct request
    for (const auto &backend : m_backends)
    {
        count++;

        if (backend.host.rfind(host, 0) == 0)
        {
            index_be = count;
            requestFound = true;
            break;
        }
    }

    // ssl assignment config
    if (requestFound && m_backends[index_be].ssl)
    {
        const auto &sslConf = m_backends[index_be].ssl_conf;
        app().setSSLFiles(sslConf.cert, sslConf.key);
        app().setSSLConfigCommands(sslConf.configurations);
    }

    // same client same backend
    if (requestFound && m_sameClientToSameBackend)
    {
        auto ipHash = std::hash<uint32_t>{}(pReq->getPeerAddr().ipNetEndian()) %
                      clientsVector.size();
        index = (ipHash + (++(*m_clientIndex))) % clientsVector.size();
    }
    else if (requestFound && !m_sameClientToSameBackend)
    {
        index = ++(*m_clientIndex) % clientsVector.size();
    }

    // set another document root
    if (requestFound)
    {
        app().setDocumentRoot(m_backends[index_be].document_root);
    }

    // http client pointer
    auto &pClient = clientsVector[index];

    if (!pClient && requestFound)
    {
        const auto &backend = m_backends[index_be];
        const auto &address =
            backend.internal_proxies[index % backend.internal_proxies.size()];

        pClient = HttpClient::newHttpClient(
            address,
            trantor::EventLoop::getEventLoopOfCurrentThread(),
            backend.ssl_conf.use_old_tls,
            backend.ssl);
    }
    else
    {
        // NOTE:
        // this section is to
        // expecting something not right
        // to prevent subdomain and same path conflict
        // if this else section remove, there's something wrong with the request

        const auto &backend = m_backends[index_be];
        const auto &address =
            backend.internal_proxies[index % backend.internal_proxies.size()];

        pClient = HttpClient::newHttpClient(
            address,
            trantor::EventLoop::getEventLoopOfCurrentThread(),
            backend.ssl_conf.use_old_tls,
            backend.ssl);
    }

    pReq->setPassThrough(true);

    if (requestFound)
    {
        pClient->setPipeliningDepth(m_pipelining);

        redirect = m_backends[index_be].redirect;
        redirectUrl = m_backends[index_be].redirect_to;

        if (redirect)
        {
            auto pRequestRedirect = HttpResponse::newHttpResponse();

            aCallback(pRequestRedirect->newRedirectionResponse(
                redirectUrl, k307TemporaryRedirect));
            return;
        }

        // valid backend object helper for response
        auto validBackend = m_backends[index_be];

        pClient->sendRequest(
            pReq,
            [aCallback = std::move(aCallback),
             validBackend =
                 std::move(validBackend)](ReqResult result,
                                          const HttpResponsePtr &pResp) {
                pResp->setPassThrough(true);

                switch (result)
                {
                    case ReqResult::Ok:
                    {
                        // add header if supplied
                        if (validBackend.header_add &&
                            validBackend.header_add_list_pair.size() >= 1)
                        {
                            for (auto &pair : validBackend.header_add_list_pair)
                            {
                                pResp->addHeader(pair.first, pair.second);
                            }
                        }
                        // give warn or error?

                        // add server check origin if supplied
                        // can be add manually from each backend before their
                        // response callback
                        if (validBackend.check_origin_whitelist)
                        {
                            pResp->addHeader(
                                "access-control-allow-origin",
                                validBackend.check_origin_whitelist_data);
                        }

                        aCallback(pResp);
                    }
                    break;

                    default:
                    {
                        // just to make it sure and show the message when happen
                        pResp->setBody("Internal Server Error By Default");
                        pResp->setStatusCode(k500InternalServerError);

                        aCallback(pResp);
                    }
                    break;
                }
            });
    }
    else
    {
        app().setDocumentRoot(m_defaultDocumentRoot);

        auto errorResp = HttpResponse::newHttpResponse();

        errorResp->setBody("Internal Server Error");
        errorResp->setStatusCode(k500InternalServerError);

        aCallback(errorResp);
    }
}

ReverseProxy::ReverseProxy()
{
}

ReverseProxy::~ReverseProxy()
{
}

void ReverseProxy::initAndStart(const Json::Value &config)
{
    if (m_ready)
    {
        return;
    }

#pragma region configuration
    // backends storage
    if (config.isMember("backends") && config["backends"].isArray())
    {
        for (auto &backends : config["backends"])
        {
            TBackendStorage backend;

            backend.host = backends["host"].asString();

            backend.redirect = backends["redirect"].asBool();

            backend.redirect_to = backends["redirect_to"].asString();

            backend.check_origin_whitelist =
                backends["check_origin_whitelist"].asBool();

            backend.check_origin_whitelist_data =
                backends["check_origin_whitelist_data"].asString();

            backend.header_add = backends["header_add"].asBool();

            if (backend.header_add)
            {
                if (backends["header_add_list"].isArray() &&
                    backends["header_add_list"].size() <= 0)
                {
                    app().getLoop()->runAfter(0, [&]() {
                        std::cerr << "ERROR: you are trying to add more header "
                                     "response, but header add list is not "
                                     "supplied or 0\n";
                        app().quit();
                    });
                }

                for (auto &header : backends["header_add_list"])
                {
                    if (header.isArray() && header.size() == 2 &&
                        header[0].isString() && header[1].isString())
                    {
                        std::string key = header[0].asString();
                        std::string value = header[1].asString();

                        backend.header_add_list_pair.emplace_back(key, value);
                    }
                    else
                    {
                        app().getLoop()->runAfter(0, [&]() {
                            std::cerr
                                << "ERROR: can't use header list, these array "
                                   "can't assign as pair 2 string\n";
                            app().quit();
                        });
                    }
                }
            }

            backend.document_root = backends["document_root"].asString();

            // check backend proxies
            if (backends["internal_proxies"].size() <= 0)
            {
                app().getLoop()->runAfter(0, [&]() {
                    std::cerr << "ERROR: internal backend proxies should be "
                                 "provide at least one\n";
                    app().quit();
                });
            }

            for (auto &proxy : backends["internal_proxies"])
            {
                backend.internal_proxies.emplace_back(proxy.asString());
            }

            backend.ssl = backends["ssl"].asBool();

            if (backend.ssl)
            {
                TBackendSSL backendSSL;

                backendSSL.key = backends["ssl_conf"]["key"].asString();
                backendSSL.cert = backends["ssl_conf"]["cert"].asString();
                backendSSL.use_old_tls =
                    backends["ssl_conf"]["use_old_tls"].asBool();

                if (backendSSL.key.length() <= 0)
                {
                    app().getLoop()->runAfter(0, [&]() {
                        std::cerr << "ERROR: you're configuring using ssl, but "
                                     "key is not supplied\n";
                        app().quit();
                    });
                }

                if (backendSSL.cert.length() <= 0)
                {
                    app().getLoop()->runAfter(0, [&]() {
                        std::cerr << "ERROR: you're configuring using ssl, but "
                                     "cert is not supplied\n";
                        app().quit();
                    });
                }

                const auto SSL_CONF_CONFIG =
                    backends["ssl_conf"]["configurations"];

                if (SSL_CONF_CONFIG.isArray() && SSL_CONF_CONFIG.size() >= 1)
                {
                    for (auto &ssl_cfg : SSL_CONF_CONFIG)
                    {
                        if (ssl_cfg.isArray() && ssl_cfg.size() == 2 &&
                            ssl_cfg[0].isString() && ssl_cfg[1].isString())
                        {
                            std::string key = ssl_cfg[0].asString();
                            std::string value = ssl_cfg[1].asString();

                            backendSSL.configurations.emplace_back(key, value);
                        }
                        else
                        {
                            app().getLoop()->runAfter(0, [&]() {
                                std::cerr
                                    << "ERROR: can't use config, these array "
                                       "can't assign as pair 2 string\n";
                                app().quit();
                            });
                        }
                    }

                    backend.ssl_conf = backendSSL;
                }
                else
                {
                    app().getLoop()->runAfter(0, [&]() {
                        std::cerr << "ERROR: ssl_conf configurations is not an "
                                     "array or empty\nREADMORE: "
                                     "https://www.openssl.org/docs/manmaster/"
                                     "man3/SSL_CONF_cmd.html\n";
                        app().quit();
                    });
                }
            }

            m_backends.emplace_back(backend);
        }

        // final check
        if (m_backends.empty())
        {
            app().getLoop()->runAfter(0, [&]() {
                std::cerr << "ERROR: backends configuration is empty\n";
                app().quit();
            });
        }
    }
    else
    {
        app().getLoop()->runAfter(0, [&]() {
            std::cerr << "ERROR: configuration\nconfig don't have 'backends' "
                         "member and/or 'backends' should be an array\n";
            app().quit();
        });
    }

    // pipelining
    m_pipelining = config.get("pipelining", 0).asInt();

    // connection factor
    m_connectionFactor = config.get("connection_factor", 0).asInt();

    if (m_connectionFactor == 0 || m_connectionFactor > 100)
    {
        app().getLoop()->runAfter(0, [&]() {
            std::cerr << "ERROR: invalid number of connection factor\n";
            app().quit();
        });
    }

    // same client to same backend
    m_sameClientToSameBackend =
        config.get("same_client_to_same_backend", false).asBool();

    // default document root
    m_defaultDocumentRoot = config["default_document_root"].asString();

    // clients config
    m_clients.init(
        [this](std::vector<HttpClientPtr> &clients, size_t ioLoopIndex) {
            clients.resize(m_backends.size() * m_connectionFactor);
        });

    // client index config
    m_clientIndex.init(
        [this](size_t &index, size_t ioLoopIndex) { index = ioLoopIndex; });

    // pre routing register
    app().registerPreRoutingAdvice([this](const HttpRequestPtr &pReq,
                                          AdviceCallback &&aCallback,
                                          AdviceChainCallback &&acCallback) {
        PreRouting(pReq, std::move(aCallback), std::move(acCallback));
    });
#pragma endregion

#if !NDEBUG
    std::cout << "drogon_server ReverseProxy plugin started\n";
#endif
}

void ReverseProxy::shutdown()
{
#if !NDEBUG
    std::cout << "drogon_server ReverseProxy plugin shutdown\n";
#endif
}
