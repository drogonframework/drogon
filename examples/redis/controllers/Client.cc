#include "Client.h"

using namespace drogon;

void Client::get(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback, std::string key)
{
  nosql::RedisClientPtr redisClient = app().getRedisClient();

  redisClient->execCommandAsync(
    [callback](const nosql::RedisResult &r) {
      if (r.type() == nosql::RedisResultType::kNil) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k404NotFound);
        callback(resp);
        return;
      }

      std::string redisResponse = r.asString();
      Json::Value response;

      Json::CharReaderBuilder builder;
      Json::CharReader* reader = builder.newCharReader();

      Json::Value json;
      std::string errors;

      bool parsingSuccessful = reader->parse(
        redisResponse.c_str(),
        redisResponse.c_str() + redisResponse.size(),
        &json,
        &errors
      );

      response["response"] = redisResponse;
      if (parsingSuccessful) {
        response["response"] = json;
      }

      auto resp = HttpResponse::newHttpJsonResponse(response);

      callback(resp);
    },
    [](const std::exception &err) {
      LOG_ERROR << "something failed!!! " << err.what();
    },
    "get %s", key.c_str());
}
