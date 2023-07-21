/**
 *
 *  drogon.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <trantor/net/EventLoop.h>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/Date.h>
#include <trantor/utils/Logger.h>

#include <drogon/CacheMap.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/HttpController.h>
#include <drogon/HttpSimpleController.h>
#include <drogon/utils/Utilities.h>
#include <drogon/MultiPart.h>
#include <drogon/plugins/Plugin.h>
#include <drogon/plugins/SecureSSLRedirector.h>
#include <drogon/plugins/AccessLogger.h>
#include <drogon/plugins/RealIpResolver.h>
#include <drogon/plugins/Hodor.h>
#include <drogon/plugins/SlashRemover.h>
#include <drogon/plugins/GlobalFilters.h>
#include <drogon/IntranetIpFilter.h>
#include <drogon/LocalHostFilter.h>
#include <drogon/Cookie.h>
#include <drogon/Session.h>
#include <drogon/IOThreadStorage.h>
#include <drogon/UploadFile.h>
#include <drogon/orm/DbClient.h>

/**
 * @mainpage
 * ### Overview
 * Drogon is a C++14/17-based HTTP application framework. Drogon can be used to
 * easily build various types of web application server programs using C++.
 */
