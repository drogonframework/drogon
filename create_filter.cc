/**
 *
 *  create_filter.cc
 * 
 *  drogon_ctl
 *
 */

#include "create_filter.h"

using namespace drogon;
void create_filter::doFilter(const HttpRequestPtr &req,
                         const FilterCallback &fcb,
                         const FilterChainCallback &fccb)
{
    //Edit your logic here
    if (1)
    {
        //Passed
        fccb();
        return;
    }
    //Check failed
    auto res = drogon::HttpResponse::newHttpResponse();
    res->setStatusCode(HttpResponse::k500InternalServerError);
    fcb(res);
}
