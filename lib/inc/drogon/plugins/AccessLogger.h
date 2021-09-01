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
            // "show_microseconds": true,
            // "custom_time_format": "",
            "log_index": 0
      }
   }
   @endcode
 *
 * log_format: a format string for access logging, there are several
 * placeholders that represent particular data.
 *     $date: the time when the log was printed.
 *     $request_date: the time when the request was created.
 *     $request_path|$path: the path of the request.
 *     $request_query|$query: the query string of the request.
 *     $request_url|$url: the URL of the request, equals to
 *                   $request_path+"?"+$request_query.
 *     $request_version|$version: the http version string.
 *     $request: the full request line.
 *     $remote_addr: the remote address
 *     $local_addr: the local address
 *     $request_len|$body_bytes_received: the content length of the request.
 *     $method: the HTTP method of the request.
 *     $thread: the current thread number.
 *     $response_len|$body_bytes_sent: the content length of the response.
 *     $http_[header_name]: the header of the request.
 *     $cookie_[cookie_name]: the cookie of the request.
 *     $upstream_http_[header_name]: the header of the response sent to the
 *                                   client.
 *     $status_code: the status code of the response.
 *     $status: the status code and string of the response.
 *     $processing_time: request processing time in seconds with a microseconds
 *                       resolution; time elapsed between the request object was
 *                       created and response object was created.
 * @note If the format string is empty or not configured, a default value of
 * "$request_date $method $url [$body_bytes_received] ($remote_addr -
 * $local_addr) $status $body_bytes_sent $processing_time" is applied.
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
 * show_microseconds: Whether print microsecond in time. True by default.
 *
 * custom_time_format: Provide a custom format for time. If not provided or
 * empty, the default format is "%Y%m%d %H:%M:%S", with microseonds followed if
 * show_microseconds is true. For detailed information about formats, please
 * refer to cpp reference about strftime().
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
    bool showMicroseconds_{true};
    bool useCustomTimeFormat_{false};
    std::string timeFormat_;

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
    //$version
    static void outputVersion(trantor::LogStream &,
                              const drogon::HttpRequestPtr &,
                              const drogon::HttpResponsePtr &);
    //$request
    static void outputReqLine(trantor::LogStream &,
                              const drogon::HttpRequestPtr &,
                              const drogon::HttpResponsePtr &);
    //$date
    void outputDate(trantor::LogStream &,
                    const drogon::HttpRequestPtr &,
                    const drogon::HttpResponsePtr &) const;
    //$request_date
    void outputReqDate(trantor::LogStream &,
                       const drogon::HttpRequestPtr &,
                       const drogon::HttpResponsePtr &) const;
    //$remote_addr
    static void outputRemoteAddr(trantor::LogStream &,
                                 const drogon::HttpRequestPtr &,
                                 const drogon::HttpResponsePtr &);
    //$local_addr
    static void outputLocalAddr(trantor::LogStream &,
                                const drogon::HttpRequestPtr &,
                                const drogon::HttpResponsePtr &);
    //$request_len $body_bytes_received
    static void outputReqLength(trantor::LogStream &,
                                const drogon::HttpRequestPtr &,
                                const drogon::HttpResponsePtr &);
    //$response_len $body_bytes_sent
    static void outputRespLength(trantor::LogStream &,
                                 const drogon::HttpRequestPtr &,
                                 const drogon::HttpResponsePtr &);
    //$method
    static void outputMethod(trantor::LogStream &,
                             const drogon::HttpRequestPtr &,
                             const drogon::HttpResponsePtr &);
    //$thread
    static void outputThreadNumber(trantor::LogStream &,
                                   const drogon::HttpRequestPtr &,
                                   const drogon::HttpResponsePtr &);
    //$http_[header_name]
    static void outputReqHeader(trantor::LogStream &stream,
                                const drogon::HttpRequestPtr &req,
                                const std::string &headerName);
    //$cookie_[cookie_name]
    static void outputReqCookie(trantor::LogStream &stream,
                                const drogon::HttpRequestPtr &req,
                                const std::string &cookie);
    //$upstream_http_[header_name]
    static void outputRespHeader(trantor::LogStream &stream,
                                 const drogon::HttpResponsePtr &resp,
                                 const std::string &headerName);
    //$status
    static void outputStatusString(trantor::LogStream &,
                                   const drogon::HttpRequestPtr &,
                                   const drogon::HttpResponsePtr &);
    //$status_code
    static void outputStatusCode(trantor::LogStream &,
                                 const drogon::HttpRequestPtr &,
                                 const drogon::HttpResponsePtr &);
    //$processing_time
    static void outputProcessingTime(trantor::LogStream &,
                                     const drogon::HttpRequestPtr &,
                                     const drogon::HttpResponsePtr &);
    //$upstream_http_content-type $upstream_http_content_type
    static void outputRespContentType(trantor::LogStream &,
                                      const drogon::HttpRequestPtr &,
                                      const drogon::HttpResponsePtr &);
};
}  // namespace plugin
}  // namespace drogon
