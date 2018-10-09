#include <drogon/CacheMap.h>
#include <trantor/utils/Logger.h>
#include <trantor/net/EventLoopThread.h>
#include <drogon/utils/Utilities.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <iostream>
#include <memory>

int main()
{

        trantor::Logger::setLogLevel(trantor::Logger::TRACE);
        trantor::EventLoopThread loopThread;
        loopThread.run();



        auto loop=loopThread.getLoop();
        std::shared_ptr<drogon::CacheMap<std::string,std::string>> main_cachePtr;
        auto now=trantor::Date::date();
        loop->runAt(now.after(1).roundSecond(),[=,&main_cachePtr]()
        {
            std::shared_ptr<drogon::CacheMap<std::string,std::string>> cachePtr
                    =std::make_shared<drogon::CacheMap<std::string,std::string>>
                            (loop,0.1,3,50);
            main_cachePtr=cachePtr;
            LOG_DEBUG<<"insert :usecount="<<main_cachePtr.use_count();
            cachePtr->insert("1","1",3,[=](){
                LOG_DEBUG<<"timeout!erase 1!";
            });
            cachePtr->insert("2","2",10,[](){
                LOG_DEBUG<<"2 timeout";
            });
        });
        trantor::EventLoop mainLoop;
        mainLoop.runAt(now.after(3).roundSecond().after(0.0013),[&](){
            if(main_cachePtr->find("1"))
            {
                LOG_DEBUG<<"find item 1";
            }
            else
            {
                LOG_DEBUG<<"can't find item 1";
            }
        });
        mainLoop.runAfter(8,[&](){
            mainLoop.quit();
        });
        mainLoop.loop();
}