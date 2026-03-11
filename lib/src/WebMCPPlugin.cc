/**
 *
 *  WebMCPPlugin.cc
 *
 */

#include <drogon/plugins/WebMCPPlugin.h>
#include <drogon/HttpAppFramework.h>
#include <trantor/utils/Logger.h>

#include <json/json.h>

#include <mutex>
#include <sstream>
#include <string>

namespace drogon
{
namespace plugin
{

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Json::Value parseBody(const HttpRequestPtr &req, std::string &errs)
{
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::istringstream ss(req->getBody());
    Json::parseFromStream(builder, ss, &root, &errs);
    return root;
}

static std::string jsonStr(const Json::Value &v)
{
    Json::StreamWriterBuilder w;
    w["indentation"] = "";
    return Json::writeString(w, v);
}

// ---------------------------------------------------------------------------
// WebMCPPlugin — lifecycle
// ---------------------------------------------------------------------------

void WebMCPPlugin::initAndStart(const Json::Value &config)
{
    if (config.isMember("path") && config["path"].isString())
        path_ = config["path"].asString();
    if (path_.empty() || path_[0] != '/')
        path_ = "/" + path_;

    if (config.isMember("server_name") && config["server_name"].isString())
        serverName_ = config["server_name"].asString();
    if (config.isMember("server_version") &&
        config["server_version"].isString())
        serverVersion_ = config["server_version"].asString();

    registerRoutes();

    LOG_INFO << "WebMCPPlugin: MCP endpoint ready at POST " << path_;
}

void WebMCPPlugin::shutdown()
{
    std::lock_guard<std::mutex> lock(toolsMutex_);
    tools_.clear();
}

// ---------------------------------------------------------------------------
// Public API — registerTool
// ---------------------------------------------------------------------------

void WebMCPPlugin::registerTool(const std::string &name,
                                const std::string &description,
                                std::vector<ToolParam> params,
                                ToolHandler handler)
{
    ToolEntry entry;
    entry.name = name;
    entry.description = description;
    entry.params = std::move(params);
    entry.handler = std::move(handler);
    entry.inputSchema = buildInputSchema(entry.params);

    std::lock_guard<std::mutex> lock(toolsMutex_);
    // Replace if a tool with the same name already exists
    for (auto &e : tools_)
    {
        if (e.name == name)
        {
            e = std::move(entry);
            LOG_INFO << "WebMCPPlugin: updated tool '" << name << "'";
            return;
        }
    }
    tools_.push_back(std::move(entry));
    LOG_INFO << "WebMCPPlugin: registered tool '" << name << "'";
}

// ---------------------------------------------------------------------------
// Route registration
// ---------------------------------------------------------------------------

void WebMCPPlugin::registerRoutes()
{
    drogon::app().registerHandler(
        path_,
        [this](const HttpRequestPtr &req,
               std::function<void(const HttpResponsePtr &)> &&cb) {
            handlePost(req, std::move(cb));
        },
        {Post});

    // GET — minimal SSE endpoint so clients that open a stream don't get 404
    drogon::app().registerHandler(
        path_,
        [](const HttpRequestPtr & /*req*/,
           std::function<void(const HttpResponsePtr &)> &&cb) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k200OK);
            resp->addHeader("Content-Type", "text/event-stream");
            resp->addHeader("Cache-Control", "no-cache");
            resp->setBody("");
            cb(resp);
        },
        {Get});
}

// ---------------------------------------------------------------------------
// JSON-RPC dispatcher
// ---------------------------------------------------------------------------

void WebMCPPlugin::handlePost(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&cb) const
{
    std::string errs;
    Json::Value body = parseBody(req, errs);

    if (!errs.empty() || body.isNull())
    {
        cb(rpcError(Json::Value::null, -32700, "Parse error: " + errs));
        return;
    }

    // Support both single request and batch (array)
    if (body.isArray())
    {
        // Batch: not commonly used by MCP clients but handle gracefully
        cb(rpcError(Json::Value::null, -32600, "Batch requests not supported"));
        return;
    }

    const Json::Value &id =
        body.isMember("id") ? body["id"] : Json::Value::null;
    const std::string method = body.get("method", "").asString();
    const Json::Value &params = body.isMember("params")
                                    ? body["params"]
                                    : Json::Value(Json::objectValue);

    if (method == "initialize")
    {
        cb(rpcOk(id, handleInitialize(params)));
    }
    else if (method == "ping")
    {
        cb(rpcOk(id, Json::Value(Json::objectValue)));
    }
    else if (method == "tools/list")
    {
        cb(rpcOk(id, handleToolsList(params)));
    }
    else if (method == "tools/call")
    {
        // tools/call is async — the handler calls back when ready
        handleToolsCall(id, params, std::move(cb));
    }
    else if (method.empty() || body.get("jsonrpc", "").asString() != "2.0")
    {
        cb(rpcError(id, -32600, "Invalid Request"));
    }
    else
    {
        cb(rpcError(id, -32601, "Method not found: " + method));
    }
}

// ---------------------------------------------------------------------------
// MCP method implementations
// ---------------------------------------------------------------------------

Json::Value WebMCPPlugin::handleInitialize(const Json::Value & /*params*/) const
{
    Json::Value result;
    result["protocolVersion"] = "2025-03-26";
    result["serverInfo"]["name"] = serverName_;
    result["serverInfo"]["version"] = serverVersion_;
    result["capabilities"]["tools"]["listChanged"] = false;
    return result;
}

Json::Value WebMCPPlugin::handleToolsList(const Json::Value & /*params*/) const
{
    Json::Value toolsArray(Json::arrayValue);

    std::lock_guard<std::mutex> lock(toolsMutex_);
    for (const auto &entry : tools_)
    {
        Json::Value tool;
        tool["name"] = entry.name;
        tool["description"] = entry.description;
        tool["inputSchema"] = entry.inputSchema;
        toolsArray.append(tool);
    }

    Json::Value result;
    result["tools"] = toolsArray;
    return result;
}

void WebMCPPlugin::handleToolsCall(
    const Json::Value &id,
    const Json::Value &params,
    std::function<void(const HttpResponsePtr &)> cb) const
{
    const std::string toolName = params.get("name", "").asString();
    const Json::Value &arguments = params.isMember("arguments")
                                       ? params["arguments"]
                                       : Json::Value(Json::objectValue);

    // Find the tool
    ToolHandler handler;
    {
        std::lock_guard<std::mutex> lock(toolsMutex_);
        for (const auto &entry : tools_)
        {
            if (entry.name == toolName)
            {
                handler = entry.handler;
                break;
            }
        }
    }

    if (!handler)
    {
        cb(rpcError(id, -32602, "Tool not found: " + toolName));
        return;
    }

    // Invoke the handler; it calls back (possibly on another thread) with
    // result
    handler(arguments, [id, cb = std::move(cb)](ToolResult &&result) {
        Json::Value mcpResult;
        mcpResult["content"] = result.content;
        if (result.isError)
            mcpResult["isError"] = true;
        cb(rpcOk(id, mcpResult));
    });
}

// ---------------------------------------------------------------------------
// JSON-RPC response builders
// ---------------------------------------------------------------------------

HttpResponsePtr WebMCPPlugin::rpcOk(const Json::Value &id,
                                    const Json::Value &result)
{
    Json::Value body;
    body["jsonrpc"] = "2.0";
    body["id"] = id;
    body["result"] = result;

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_APPLICATION_JSON);
    resp->setBody(jsonStr(body));
    return resp;
}

HttpResponsePtr WebMCPPlugin::rpcError(const Json::Value &id,
                                       int code,
                                       const std::string &message)
{
    Json::Value err;
    err["code"] = code;
    err["message"] = message;

    Json::Value body;
    body["jsonrpc"] = "2.0";
    body["id"] = id;
    body["error"] = err;

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);  // JSON-RPC errors use HTTP 200
    resp->setContentTypeCode(CT_APPLICATION_JSON);
    resp->setBody(jsonStr(body));
    return resp;
}

// ---------------------------------------------------------------------------
// Schema builder
// ---------------------------------------------------------------------------

Json::Value WebMCPPlugin::buildInputSchema(const std::vector<ToolParam> &params)
{
    Json::Value schema;
    schema["type"] = "object";
    schema["properties"] = Json::Value(Json::objectValue);
    Json::Value required(Json::arrayValue);

    for (const auto &p : params)
    {
        Json::Value prop;
        prop["type"] = p.type;
        prop["description"] = p.description;
        schema["properties"][p.name] = prop;
        if (p.required)
            required.append(p.name);
    }

    if (!required.empty())
        schema["required"] = required;

    return schema;
}

}  // namespace plugin
}  // namespace drogon
