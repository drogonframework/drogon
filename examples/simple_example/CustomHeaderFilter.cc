/**
 *
 *  CustomHeaderFilter.cc
 *
 */

#include "CustomHeaderFilter.h"

using namespace drogon;

void CustomHeaderFilter::doFilter(const HttpRequestPtr &req,
                                  const FilterCallback &fcb,
                                  const FilterChainCallback &fccb)
{
    if (req->getHeader(_field) == _value)
    {
        //Passed
        fccb();
        return;
    }
    //Check failed
    auto res = drogon::HttpResponse::newHttpResponse();
    res->setStatusCode(k500InternalServerError);
    fcb(res);
}
