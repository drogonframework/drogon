add_executable(ssl_server_test SSLServerTest.cc)
add_executable(ssl_client_test SSLClientTest.cc)
add_executable(serial_task_queue_test1 SerialTaskQueueTest1.cc)
add_executable(serial_task_queue_test2 SerialTaskQueueTest2.cc)
add_executable(timer_test TimerTest.cc)
add_executable(timer_test1 TimerTest1.cc)
add_executable(run_in_loop_test1 RunInLoopTest1.cc)
add_executable(run_in_loop_test2 RunInLoopTest2.cc)
add_executable(logger_test LoggerTest.cc)
add_executable(async_file_logger_test AsyncFileLoggerTest.cc)
add_executable(tcp_server_test TcpServerTest.cc)
add_executable(concurrent_task_queue_test ConcurrentTaskQueueTest.cc)
add_executable(tcp_client_test TcpClientTest.cc)
add_executable(async_file_logger_test1 AsyncFileLoggerTest1.cc)
add_executable(sendfile_test SendfileTest.cc)
add_executable(timing_wheel_test TimingWheelTest.cc)
add_executable(kickoff_test KickoffTest.cc)
add_executable(dns_test DnsTest.cc)
add_executable(delayed_ssl_server_test DelayedSSLServerTest.cc)
add_executable(delayed_ssl_client_test DelayedSSLClientTest.cc)
set(targets_list
    ssl_server_test
    ssl_client_test
    serial_task_queue_test1
    serial_task_queue_test2
    timer_test
    timer_test1
    run_in_loop_test1
    run_in_loop_test2
    logger_test
    async_file_logger_test
    tcp_server_test
    concurrent_task_queue_test
    tcp_client_test
    async_file_logger_test1
    sendfile_test
    timing_wheel_test
    kickoff_test
    dns_test
    delayed_ssl_server_test
    delayed_ssl_client_test)

set_property(TARGET ${targets_list} PROPERTY CXX_STANDARD 14)
set_property(TARGET ${targets_list} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${targets_list} PROPERTY CXX_EXTENSIONS OFF)

foreach(T ${targets_list})
  target_link_libraries(${T} PRIVATE trantor)
endforeach()
