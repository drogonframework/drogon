module;

#include <drogon/drogon.h>
#include <drogon/HttpSimpleController.h>
#include <drogon/WebSocketClient.h>
#include <drogon/WebSocketController.h>
#include <drogon/PubSubService.h>
#include <drogon/orm/Mapper.h>
#include <drogon/HttpTypes.h>
#include <drogon/HttpMiddleware.h>
#include <drogon/HttpController.h>
#include <drogon/IOThreadStorage.h>
#include <drogon/drogon_callbacks.h>

#include <drogon/utils/coroutine.h>
#include <drogon/utils/Utilities.h>
#include <drogon/plugins/Plugin.h>
#include <drogon/nosql/RedisResult.h>
#include <drogon/orm/DbListener.h>

export module drogon;

export namespace drogon
{

using drogon::HttpClient;
using drogon::HttpClientPtr;
using drogon::HttpRequest;
using drogon::HttpRequestPtr;
using drogon::HttpResponse;
using drogon::HttpResponsePtr;
using drogon::MultiPartParser;
using drogon::RequestStream;
using drogon::RequestStreamPtr;
using drogon::RequestStreamReader;
using drogon::ResponseStream;
using drogon::ResponseStreamPtr;

using drogon::AdviceCallback;
using drogon::AdviceChainCallback;
using drogon::AdviceDestroySessionCallback;
using drogon::AdviceStartSessionCallback;
using drogon::FilterCallback;
using drogon::FilterChainCallback;
using drogon::HttpReqCallback;
using drogon::MiddlewareCallback;
using drogon::MiddlewareNextCallback;

using drogon::ContentType;
using drogon::HttpMethod;
using drogon::HttpStatusCode;
using drogon::ReqResult;
using drogon::to_string;
using drogon::to_string_view;
using drogon::operator<<;
using drogon::FileType;
using drogon::Version;
using drogon::WebSocketMessageType;

using drogon::HttpController;
using drogon::HttpSimpleController;
using drogon::HttpViewData;

using drogon::PubSubService;
using drogon::SubscriberID;

using drogon::WebSocketClient;
using drogon::WebSocketClientPtr;
using drogon::WebSocketConnectionPtr;
using drogon::WebSocketController;
using drogon::WebSocketRequestCallback;

using drogon::app;

using drogon::HttpCoroMiddleware;
using drogon::HttpFilter;
using drogon::HttpMiddleware;
using drogon::IOThreadStorage;
using drogon::Plugin;

using drogon::async_run;
using drogon::AsyncTask;
using drogon::CallbackAwaiter;
using drogon::co_future;
using drogon::Mutex;
using drogon::queueInLoopCoro;
using drogon::sleepCoro;
using drogon::switchThreadCoro;
using drogon::Task;
using drogon::untilQuit;

using drogon::statusCodeToString;

}  // namespace drogon

export namespace drogon::utils
{

using drogon::utils::base64Decode;
using drogon::utils::base64DecodedLength;
using drogon::utils::base64DecodeToVector;
using drogon::utils::base64Encode;
using drogon::utils::base64EncodedLength;
using drogon::utils::base64EncodeUnpadded;
using drogon::utils::binaryStringToHex;
using drogon::utils::brotliCompress;
using drogon::utils::brotliDecompress;
using drogon::utils::createPath;
using drogon::utils::formattedString;
using drogon::utils::genRandomString;
using drogon::utils::getMd5;
using drogon::utils::getSha1;
using drogon::utils::getSha256;
using drogon::utils::getSha3;
using drogon::utils::getUuid;
using drogon::utils::gzipCompress;
using drogon::utils::gzipDecompress;
using drogon::utils::hexToBinaryString;
using drogon::utils::hexToBinaryVector;
using drogon::utils::isBase64;
using drogon::utils::isInteger;
using drogon::utils::needUrlDecoding;
using drogon::utils::replaceAll;
using drogon::utils::splitString;
using drogon::utils::splitStringToSet;
using drogon::utils::urlDecode;
using drogon::utils::urlEncode;
using drogon::utils::urlEncodeComponent;

}  // namespace drogon::utils

export namespace drogon::nosql
{

using drogon::nosql::RedisClient;
using drogon::nosql::RedisClientPtr;
using drogon::nosql::RedisErrorCode;
using drogon::nosql::RedisException;
using drogon::nosql::RedisExceptionCallback;
using drogon::nosql::RedisMessageCallback;
using drogon::nosql::RedisResult;
using drogon::nosql::RedisResultCallback;
using drogon::nosql::RedisResultType;
using drogon::nosql::RedisSubscriber;
using drogon::nosql::RedisTransactionPtr;

}  // namespace drogon::nosql

export namespace drogon::orm
{

using drogon::orm::ConstRowIterator;
using drogon::orm::DbClient;
using drogon::orm::DbListener;
using drogon::orm::DrogonDbException;
using drogon::orm::Field;
using drogon::orm::Mapper;
using drogon::orm::Result;

}  // namespace drogon::orm
