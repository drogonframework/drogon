/**
 *
 *  AccessLogger.h
 *
 */

#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/plugins/Plugin.h>
#include <trantor/utils/AsyncFileLogger.h>
#include <vector>

namespace drogon
{
namespace plugin
{
/**
 * @brief This plugin is used to print all requests to the log.
 *
 * The json configuration is as follows:
 *
 * @code
   {
      "name": "drogon::plugin::AccessLogger",
      "dependencies": [],
      "config": {
            "log_path": "./",
            "log_format": "",
            "log_file": "access.log",
            "log_size_limit": 0,
            "use_local_time": true,
            "log_index": 0
      }
   }
   @endcode
 *
 * log_format: a format string for logging requests, there are several
 * placeholders that represent particular data.
 *     $date: the time when the log was printed.
 *     $request_date: the time when the request was created.
 *     $request_path: the path of the request.
 *     $request_query: the query string of the request.
 *     $request_url: the URL of the request, equals to
 *                   $request_path+"?"+$request_query.
 *     $remote_addr: the remote address
 *     $local_addr: the local address
 *     $request_len: the content length of the request.
 *     $method: the HTTP method of the request.
 *     $thread: the current thread number.
 *
 * log_path: Log file path, empty by default,in which case,logs are output to
 * the regular log file (or stdout based on the log configuration).
 *
 * log_file: The access log file name, 'access.log' by default. if the file name
 * does not contain a extension, the .log extension is used.
 *
 * log_size_limit: 0 bytes by default, when the log file size reaches
 * "log_size_limit", the log file is switched. Zero value means never switch
 *
 * log_index: The index of log output, 0 by default.
 *
 * Enable the plugin by adding the configuration to the list of plugins in the
 * configuration file.
 *
 */
class DROGON_EXPORT AccessLogger : public drogon::Plugin<AccessLogger>
{
  public:
    AccessLogger()
    {
    }
    void initAndStart(const Json::Value &config) override;
    void shutdown() override;

  private:
    trantor::AsyncFileLogger asyncFileLogger_;
    int logIndex_{0};
    bool useLocalTime_{true};
    using LogFunction = std::function<void(trantor::LogStream &,
                                           const drogon::HttpRequestPtr &,
                                           const drogon::HttpResponsePtr &)>;
    std::vector<LogFunction> logFunctions_;
    void logging(trantor::LogStream &stream,
                 const drogon::HttpRequestPtr &req,
                 const drogon::HttpResponsePtr &resp);
    void createLogFunctions(std::string format);
    LogFunction newLogFunction(const std::string &placeholder);
    std::map<std::string, LogFunction> logFunctionMap_;
    //$request_path
    static void outputReqPath(trantor::LogStream &,
                              const drogon::HttpRequestPtr &,
                              const drogon::HttpResponsePtr &);
    //$request_query
    static void outputReqQuery(trantor::LogStream &,
                               const drogon::HttpRequestPtr &,
                               const drogon::HttpResponsePtr &);
    //$request_url
    static void outputReqURL(trantor::LogStream &,
                             const drogon::HttpRequestPtr &,
                             const drogon::HttpResponsePtr &);
    //$date
    void outputDate(trantor::LogStream &,
                    const drogon::HttpRequestPtr &,
                    const drogon::HttpResponsePtr &);
    //$request_date
    void outputReqDate(trantor::LogStream &,
                       const drogon::HttpRequestPtr &,
                       const drogon::HttpResponsePtr &);
    //$remote_addr
    static void outputRemoteAddr(trantor::LogStream &,
                                 const drogon::HttpRequestPtr &,
                                 const drogon::HttpResponsePtr &);
    //$local_addr
    static void outputLocalAddr(trantor::LogStream &,
                                const drogon::HttpRequestPtr &,
                                const drogon::HttpResponsePtr &);
    //$request_len
    static void outputReqLength(trantor::LogStream &,
                                const drogon::HttpRequestPtr &,
                                const drogon::HttpResponsePtr &);
    static void outputMethod(trantor::LogStream &,
                             const drogon::HttpRequestPtr &,
                             const drogon::HttpResponsePtr &);
    static void outputThreadNumber(trantor::LogStream &,
                                   const drogon::HttpRequestPtr &,
                                   const drogon::HttpResponsePtr &);
};
}  // namespace plugin
}  // namespace drogon
