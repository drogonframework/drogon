#include "NormalResolver.h"
#include <trantor/utils/Logger.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <strings.h>  // memset
#endif

using namespace trantor;

std::shared_ptr<Resolver> Resolver::newResolver(trantor::EventLoop *loop,
                                                size_t timeout)
{
    return std::make_shared<NormalResolver>(timeout);
}
bool Resolver::isCAresUsed()
{
    return false;
}
void NormalResolver::resolve(const std::string &hostname,
                             const Callback &callback)
{
    {
        std::lock_guard<std::mutex> guard(globalMutex());
        auto iter = globalCache().find(hostname);
        if (iter != globalCache().end())
        {
            auto &cachedAddr = iter->second;
            if (timeout_ == 0 || cachedAddr.second.after(static_cast<double>(
                                     timeout_)) > trantor::Date::date())
            {
                callback(cachedAddr.first);
                return;
            }
        }
    }

    concurrentTaskQueue().runTaskInQueue(
        [thisPtr = shared_from_this(), callback, hostname]() {
            {
                std::lock_guard<std::mutex> guard(thisPtr->globalMutex());
                auto iter = thisPtr->globalCache().find(hostname);
                if (iter != thisPtr->globalCache().end())
                {
                    auto &cachedAddr = iter->second;
                    if (thisPtr->timeout_ == 0 ||
                        cachedAddr.second.after(static_cast<double>(
                            thisPtr->timeout_)) > trantor::Date::date())
                    {
                        callback(cachedAddr.first);
                        return;
                    }
                }
            }
            struct addrinfo hints, *res;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = PF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_PASSIVE;
            auto error = getaddrinfo(hostname.data(), nullptr, &hints, &res);
            if (error == -1 || res == nullptr)
            {
                LOG_SYSERR << "InetAddress::resolve";
                callback(InetAddress{});
                return;
            }
            InetAddress inet;
            if (res->ai_family == AF_INET)
            {
                struct sockaddr_in addr;
                memset(&addr, 0, sizeof addr);
                addr = *reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
                inet = InetAddress(addr);
            }
            else if (res->ai_family == AF_INET6)
            {
                struct sockaddr_in6 addr;
                memset(&addr, 0, sizeof addr);
                addr = *reinterpret_cast<struct sockaddr_in6 *>(res->ai_addr);
                inet = InetAddress(addr);
            }
            callback(inet);
            {
                std::lock_guard<std::mutex> guard(thisPtr->globalMutex());
                auto &addrItem = thisPtr->globalCache()[hostname];
                addrItem.first = inet;
                addrItem.second = trantor::Date::date();
            }
            return;
        });
}
