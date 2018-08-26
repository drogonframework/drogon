#include "api_v1_ApiTest.h"
using namespace api::v1;
//add definition of your processing function here
void ApiTest::get(const HttpRequestPtr& req,const std::function<void (const HttpResponsePtr &)>&callback,int p1,std::string p2)
{
    HttpViewData data;
    data.insert("title",std::string("ApiTest::get"));
    std::map<std::string,std::string> para;
    para["p1"]=std::to_string(p1);
    para["p2"]=p2;
    data.insert("parameters",para);
    auto res=HttpResponse::newHttpViewResponse("DynamicListParaView.csp",data);
    callback(res);
}
void ApiTest::your_method_name(const HttpRequestPtr& req,const std::function<void (const HttpResponsePtr &)>&callback,double p1,int p2) const
{
    HttpViewData data;
    data.insert("title",std::string("ApiTest::get"));
    std::map<std::string,std::string> para;
    para["p1"]=std::to_string(p1);
    para["p2"]=std::to_string(p2);
    data.insert("parameters",para);
    auto res=HttpResponse::newHttpViewResponse("ListParaView",data);
    callback(res);
}

