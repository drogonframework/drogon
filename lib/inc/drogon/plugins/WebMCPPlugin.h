/**
 *
 *  WebMCPPlugin.h
 *
 *  Drogon plugin that turns the Drogon application itself into an MCP server,
 *  letting C++ code register MCP tools that AI agents can call over HTTP.
 *
 *  --------------------------------------------------------------------------
 *  Quick start
 *  --------------------------------------------------------------------------
 *
 *  1. Add the plugin to your config.json:
 *
 *  @code
 *  {
 *    "name": "drogon::plugin::WebMCPPlugin",
 *    "config": {
 *      "path": "/mcp"
 *    }
 *  }
 *  @endcode
 *
 *  2. Register tools anywhere in your application code (e.g. main.cc or a
 *     controller constructor) *before* app().run():
 *
 *  @code
 *  #include <drogon/plugins/WebMCPPlugin.h>
 *
 *  auto *mcp = app().getPlugin<drogon::plugin::WebMCPPlugin>();
 *
 *  mcp->registerTool(
 *      "recognize_image",                       // tool name
 *      "Detect objects in a JPEG/PNG image",    // description shown to agent
 *      {                                        // input schema parameters
 *          {"image_base64", "string",  "Base64-encoded image data", true},
 *          {"min_confidence", "number", "Minimum confidence 0-1",  false},
 *      },
 *      [](const Json::Value &args,
 *         drogon::plugin::ToolResultCallback &&cb) {
 *          auto b64    = args["image_base64"].asString();
 *          double conf = args.get("min_confidence", 0.5).asDouble();
 *          std::string label = myRecognizer.run(b64, conf);
 *          cb(drogon::plugin::ToolResult::text(label));
 *      });
 *  @endcode
 *
 *  --------------------------------------------------------------------------
 *  HTTP endpoints (MCP Streamable-HTTP transport, 2025-03-26 spec)
 *  --------------------------------------------------------------------------
 *
 *    POST {path}   — JSON-RPC 2.0 endpoint (initialize / tools/list /
 *                    tools/call / ping …)
 *    GET  {path}   — SSE stream for server-initiated messages (optional)
 *    DELETE {path} — terminate a session
 *
 *  --------------------------------------------------------------------------
 *  Supported MCP methods
 *  --------------------------------------------------------------------------
 *    initialize        — capability negotiation
 *    ping              — keep-alive
 *    tools/list        — enumerate registered tools
 *    tools/call        — invoke a tool by name
 *
 *  --------------------------------------------------------------------------
 *  Configuration fields
 *  --------------------------------------------------------------------------
 *    path          (string,  default "/mcp")   — HTTP endpoint path
 *    server_name   (string,  default "drogon") — MCP serverInfo.name
 *    server_version(string,  default "1.0.0")  — MCP serverInfo.version
 */

#pragma once

#include <drogon/plugins/Plugin.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace drogon
{
namespace plugin
{

// ---------------------------------------------------------------------------
// ToolResult — what a tool handler returns to the agent
// ---------------------------------------------------------------------------
struct ToolResult
{
    // content array as defined by the MCP spec
    Json::Value content;
    bool isError{false};

    // Convenience factories
    static ToolResult text(const std::string &text)
    {
        ToolResult r;
        Json::Value item;
        item["type"] = "text";
        item["text"] = text;
        r.content.append(item);
        return r;
    }

    static ToolResult error(const std::string &message)
    {
        ToolResult r;
        r.isError = true;
        Json::Value item;
        item["type"] = "text";
        item["text"] = message;
        r.content.append(item);
        return r;
    }

    static ToolResult json(const Json::Value &value)
    {
        ToolResult r;
        Json::StreamWriterBuilder w;
        Json::Value item;
        item["type"] = "text";
        item["text"] = Json::writeString(w, value);
        r.content.append(item);
        return r;
    }
};

// Callback the tool handler must invoke (exactly once) with its result.
using ToolResultCallback = std::function<void(ToolResult &&)>;

// Tool handler signature.
using ToolHandler = std::function<void(const Json::Value &arguments,
                                       ToolResultCallback &&callback)>;

// ---------------------------------------------------------------------------
// ToolParam — describes one parameter in the JSON Schema input object
// ---------------------------------------------------------------------------
struct ToolParam
{
    std::string name;
    // JSON Schema type: "string" | "number" | "integer" | "boolean" | "object"
    std::string type{"string"};
    std::string description;
    bool required{false};
};

// ---------------------------------------------------------------------------
// WebMCPPlugin
// ---------------------------------------------------------------------------
class DROGON_EXPORT WebMCPPlugin : public drogon::Plugin<WebMCPPlugin>
{
  public:
    WebMCPPlugin() = default;

    void initAndStart(const Json::Value &config) override;
    void shutdown() override;

    /**
     * @brief Register a tool that AI agents can discover and call.
     *
     * Thread-safe; may be called before or after app().run().
     *
     * @param name        Unique tool name (snake_case recommended).
     * @param description Human-readable description sent to the agent.
     * @param params      Input parameter descriptors.
     * @param handler     Function called when the agent invokes the tool.
     *                    Must call the supplied ToolResultCallback exactly
     * once.
     */
    void registerTool(const std::string &name,
                      const std::string &description,
                      std::vector<ToolParam> params,
                      ToolHandler handler);

  private:
    struct ToolEntry
    {
        std::string name;
        std::string description;
        std::vector<ToolParam> params;
        ToolHandler handler;

        // Pre-built JSON Schema object for tools/list
        Json::Value inputSchema;
    };

    std::string path_{"/mcp"};
    std::string serverName_{"drogon"};
    std::string serverVersion_{"1.0.0"};

    mutable std::mutex toolsMutex_;
    std::vector<ToolEntry> tools_;

    void registerRoutes();

    // JSON-RPC dispatcher
    void handlePost(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&cb) const;

    // Individual MCP method handlers (return JSON-RPC result or error object)
    Json::Value handleInitialize(const Json::Value &params) const;
    Json::Value handleToolsList(const Json::Value &params) const;
    void handleToolsCall(const Json::Value &id,
                         const Json::Value &params,
                         std::function<void(const HttpResponsePtr &)> cb) const;

    // Build a JSON-RPC 2.0 success response
    static HttpResponsePtr rpcOk(const Json::Value &id,
                                 const Json::Value &result);
    // Build a JSON-RPC 2.0 error response
    static HttpResponsePtr rpcError(const Json::Value &id,
                                    int code,
                                    const std::string &message);

    // Build inputSchema from ToolParam list
    static Json::Value buildInputSchema(const std::vector<ToolParam> &params);
};

}  // namespace plugin
}  // namespace drogon
