#include <trantor/net/TcpServer.h>
#include <trantor/utils/Logger.h>
#include <trantor/net/EventLoopThread.h>
#include <string>
#include <iostream>
using namespace trantor;
#define USE_IPV6 0
int main()
{
	LOG_DEBUG<<"test start";
	Logger::setLogLevel(Logger::TRACE);
	//EventLoopThread loopThread;
	EventLoop loop;
	//loopThread.run();
#if USE_IPV6
	InetAddress addr(7676,true,true);
#else
	InetAddress addr(7676);
#endif
	TcpServer server(&loop,addr,"test");
	server.setRecvMessageCallback([](const TcpConnectionPtr &connectionPtr,MsgBuffer *buffer){
		//LOG_DEBUG<<"recv callback!";
		std::cout<<std::string(buffer->peek(),buffer->readableBytes());
		connectionPtr->send(buffer->peek(),buffer->readableBytes());
		buffer->retrieveAll();
		connectionPtr->forceClose();
	});
	server.setConnectionCallback([](const TcpConnectionPtr& connPtr){
		if(connPtr->connected())
		{
			LOG_DEBUG<<"New connection";
		}
		else if(connPtr->disconnected())
		{
			LOG_DEBUG<<"connection disconnected";
		}
	});
	server.setIoLoopNum(3);
	server.start();
	loop.loop();
}