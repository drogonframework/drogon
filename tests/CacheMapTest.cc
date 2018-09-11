#include <drogon/CacheMap.h>
#include <trantor/net/EventLoopThread.h>

#include <unistd.h>
#include <string>
#include <thread>
#include <iostream>

int main()
{
    trantor::EventLoopThread loopThread;
    loopThread.run();
    drogon::CacheMap<std::string,std::string> cache(loopThread.getLoop(),1,120);
    cache.insert("1","first",20,[=]{
        std::cout<<"first item in cache timeout,erase!"<<std::endl;
    });
    cache.insert("2","second",5);
    cache.insert("3","third",5);
    std::thread thread1([&]{
        sleep(1);
        cache.erase("3");
        cache.insert("3","THIRD");
        sleep(10);
        if(cache.find("1"))
        {
            std::cout<<"sleep 10,1 item:"<<cache["1"]<<std::endl;
        }
        else
        {
            std::cout<<"can't find 1 item"<<std::endl;
        }
        sleep(15);
        if(cache.find("1"))
        {
            std::cout<<"sleep 15,1 item:"<<cache["1"]<<std::endl;
        }
        else
        {
            std::cout<<"can't find 1 item"<<std::endl;
        }
        sleep(20);
        if(cache.find("1"))
        {
            std::cout<<"sleep 20,1 item:"<<cache["1"]<<std::endl;
        }
        else
        {
            std::cout<<"can't find 1 item"<<std::endl;
        }
        sleep(22);
        if(cache.find("1"))
        {
            std::cout<<"sleep22,1 item:"<<cache["1"]<<std::endl;
        }
        else
        {
            std::cout<<"can't find 1 item"<<std::endl;
        }

        if(cache.find("2"))
        {
            std::cout<<"sleep22,2 item:"<<cache["2"]<<std::endl;
        }
        else
        {
            std::cout<<"can't find 2 item"<<std::endl;
        }

        if(cache.find("3"))
        {
            std::cout<<"sleep22,3 item:"<<cache["3"]<<std::endl;
        }
        else
        {
            std::cout<<"can't find 3 item"<<std::endl;
        }

    });
    thread1.join();

    loopThread.stop();

    getchar();
}