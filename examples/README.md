# Drogon Examples

The following examples can help you understand how to use Drogon:

1. [helloworld](https://github.com/drogonframework/drogon/tree/master/examples/helloworld) - The multiple ways of "Hello, World!"
2. [client_example](https://github.com/drogonframework/drogon/tree/master/examples/client_example/main.cc) - A client example
3. [websocket_client](https://github.com/drogonframework/drogon/tree/master/examples/websocket_client/WebSocketClient.cc) - An example on how to use the WebSocket client
4. [login_session](https://github.com/drogonframework/drogon/tree/master/examples/login_session) - How to use the built-in session system to handle login and out
5. [file_upload](https://github.com/drogonframework/drogon/tree/master/examples/file_upload) - How to handle file uploads in Drogon
6. [simple_reverse_proxy](https://github.com/drogonframework/drogon/tree/master/examples/simple_reverse_proxy) - An example showing how to use Drogon as a HTTP reverse 
proxy with a simple round robin
7. [benchmark](https://github.com/drogonframework/drogon/tree/master/examples/benchmark) - Basic benchmark(https://github.com/drogonframework/drogon/wiki/13-Benchmarks) example
8. [jsonstore](https://github.com/drogonframework/drogon/tree/master/examples/jsonstore) - Implementation of a [jsonstore](https://github.com/bluzi/jsonstore)-like storage service that is concurrent and stores in memory. Serving as a showcase on how to build a minimally useful RESTful APIs in Drogon
9. [redis](https://github.com/drogonframework/drogon/tree/master/examples/redis) - A simple example of Redis
10. [websocket_server](https://github.com/drogonframework/drogon/tree/master/examples/websocket_server) - A example websocket chat room server
11. [redis_cache](https://github.com/drogonframework/drogon/tree/master/examples/redis_cache) - An example for using coroutines of Redis clients
12. [redis_chat](https://github.com/drogonframework/drogon/tree/master/examples/redis_chat) - A chatroom server built with websocket and Redis pub/sub service
13. [prometheus_example](https://github.com/drogonframework/drogon/tree/master/examples/prometheus_example) - An example of how to use the Prometheus exporter in Drogon
14. [cors](https://github.com/drogonframework/drogon/tree/master/examples/cors) - An example demonstrating how to implement CORS (Cross-Origin Resource Sharing) support in Drogon

### [TechEmpower Framework Benchmarks](https://github.com/TechEmpower/FrameworkBenchmarks) test suite

I created a benchmark suite for the `tfb`, see [here](https://github.com/TechEmpower/FrameworkBenchmarks/tree/master/frameworks/C%2B%2B/drogon) for details.

### Another test suite

I also created a test suite for another web frameworks benchmark repository, see [here](https://github.com/the-benchmarker/web-frameworks/tree/master/cpp/drogon).  In this project, Drogon is used as a sub-module (locally include in the project).
