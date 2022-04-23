#include <drogon/drogon.h>

using namespace drogon;

HttpResponsePtr makeFailedResponse()
{
    Json::Value json;
    json["ok"] = false;
    auto resp = HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(k500InternalServerError);
    return resp;
}

HttpResponsePtr makeSuccessResponse()
{
    Json::Value json;
    json["ok"] = true;
    auto resp = HttpResponse::newHttpJsonResponse(json);
    return resp;
}

std::string getRandomString(size_t n)
{
    std::vector<unsigned char> random(n);
    utils::secureRandomBytes(random.data(), random.size());

    // This is cryptographically safe as 256 mod 16 == 0
    static const std::string alphabets = "0123456789abcdef";
    assert(256 % alphabets.size() == 0);
    std::string randomString(n, '\0');
    for (size_t i = 0; i < n; i++)
        randomString[i] = alphabets[random[i] % alphabets.size()];
    return randomString;
}

struct DataItem
{
    Json::Value item;
    std::mutex mtx;
};

class JsonStore : public HttpController<JsonStore>
{
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(JsonStore::getToken, "/get-token", Get);
    ADD_METHOD_VIA_REGEX(JsonStore::getItem, "/([a-f0-9]{64})/(.*)", Get);
    ADD_METHOD_VIA_REGEX(JsonStore::createItem, "/([a-f0-9]{64})", Post);
    ADD_METHOD_VIA_REGEX(JsonStore::deleteItem, "/([a-f0-9]{64})", Delete);
    ADD_METHOD_VIA_REGEX(JsonStore::updateItem, "/([a-f0-9]{64})/(.*)", Put);
    METHOD_LIST_END

    void getToken(const HttpRequestPtr&,
                  std::function<void(const HttpResponsePtr&)>&& callback)
    {
        std::string randomString = getRandomString(64);
        Json::Value res;
        res["token"] = randomString;

        callback(HttpResponse::newHttpJsonResponse(std::move(res)));
    }

    void getItem(const HttpRequestPtr&,
                 std::function<void(const HttpResponsePtr&)>&& callback,
                 const std::string& token,
                 const std::string& path)
    {
        auto itemPtr = [this, &token]() -> std::shared_ptr<DataItem> {
            // It is possible that the item is being removed while another
            // thread tries to look it up. The mutex here prevents that from
            // happening.
            std::lock_guard<std::mutex> lock(storageMtx_);
            auto it = dataStore_.find(token);
            if (it == dataStore_.end())
                return nullptr;
            return it->second;
        }();
        if (itemPtr == nullptr)
        {
            callback(makeFailedResponse());
            return;
        }

        auto& item = *itemPtr;
        // Prevents another thread from writing to the same item while this
        // thread reads. Could cause blockage if multiple clients are asking to
        // read the same object. But that should be rare.
        std::lock_guard<std::mutex> lock(item.mtx);
        Json::Value* valuePtr = walkJson(item.item, path);

        if (valuePtr == nullptr)
        {
            callback(makeFailedResponse());
            return;
        }

        auto resp = HttpResponse::newHttpJsonResponse(*valuePtr);
        callback(resp);
    }

    void updateItem(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback,
                    const std::string& token,
                    const std::string& path)
    {
        auto jsonPtr = req->jsonObject();
        auto itemPtr = [this, &token]() -> std::shared_ptr<DataItem> {
            // It is possible that the item is being removed while another
            // thread tries to look it up. The mutex here prevents that from
            // happening.
            std::lock_guard<std::mutex> lock(storageMtx_);
            auto it = dataStore_.find(token);
            if (it == dataStore_.end())
                return nullptr;
            return it->second;
        }();

        if (itemPtr == nullptr || jsonPtr == nullptr)
        {
            callback(makeFailedResponse());
            return;
        }

        auto& item = *itemPtr;
        std::lock_guard<std::mutex> lock(item.mtx);
        Json::Value* valuePtr = walkJson(item.item, path, 1);

        if (valuePtr == nullptr)
        {
            callback(makeFailedResponse());
            return;
        }

        std::string key = utils::splitString(path, "/").back();
        (*valuePtr)[key] = *jsonPtr;

        callback(makeSuccessResponse());
    }

    void createItem(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback,
                    const std::string& token)
    {
        auto jsonPtr = req->jsonObject();
        if (jsonPtr == nullptr)
        {
            callback(makeFailedResponse());
            return;
        }

        std::lock_guard<std::mutex> lock(storageMtx_);
        if (dataStore_.find(token) == dataStore_.end())
        {
            auto item = std::make_shared<DataItem>();
            item->item = std::move(*jsonPtr);
            dataStore_.insert({token, std::move(item)});

            callback(makeSuccessResponse());
        }
        else
        {
            callback(makeFailedResponse());
        }
    }

    void deleteItem(const HttpRequestPtr&,
                    std::function<void(const HttpResponsePtr&)>&& callback,
                    const std::string& token)
    {
        std::lock_guard<std::mutex> lock(storageMtx_);
        dataStore_.erase(token);

        callback(makeSuccessResponse());
    }

  protected:
    static Json::Value* walkJson(Json::Value& json,
                                 const std::string& path,
                                 size_t ignore_back = 0)
    {
        auto pathElem = utils::splitString(path, "/", false);
        if (pathElem.size() >= ignore_back)
            pathElem.resize(pathElem.size() - ignore_back);
        Json::Value* valuePtr = &json;
        for (const auto& elem : pathElem)
        {
            if (valuePtr->isArray())
            {
                Json::Value& value = (*valuePtr)[std::stoi(elem)];
                if (value.isNull())
                    return nullptr;

                valuePtr = &value;
            }
            else
            {
                Json::Value& value = (*valuePtr)[elem];
                if (value.isNull())
                    return nullptr;

                valuePtr = &value;
            }
        }
        return valuePtr;
    }

    std::unordered_map<std::string, std::shared_ptr<DataItem>> dataStore_;
    std::mutex storageMtx_;
};

int main()
{
    app().addListener("127.0.0.1", 8848).run();
}