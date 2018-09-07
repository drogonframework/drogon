## Overview
```
 ____
|  _ \ _ __ ___   __ _  ___  _ __
| | | | '__/ _ \ / _` |/ _ \| '_ \
| |_| | | | (_) | (_| | (_) | | | |
|____/|_|  \___/ \__, |\___/|_| |_|
                 |___/
```

**Drogon** is a C++11-based HTTP application framework. Drogon can be used to easily build various types of web application server programs using C++. Drogon's main application platform is Linux. For debugging purposes, it also supports Mac OS. There is no plan to support Windows. Its main features are as follows:

* The network layer uses a NIO framework based on epoll (poll under MacOS) to provide high-concurrency, high-performance network IO;
* Full asynchronous programming mode;
* Support Http1.0/1.1 (server side and client side);
* Based on template, a simple reflection mechanism is implemented to completely decouple the main program framework, controller and view.
* Support for cookies and built-in sessions;
* Support back-end rendering, the controller generates the data to the view to generate the Html page, the view is described by a "JSP-like" CSP file, the C++ code is embedded into the Html page by the CSP tag, and the drogon command-line tool automatically generates the C++ code file for compilation;
* Support view page dynamic loading (dynamic compilation and loading at runtime);
* Very convenient and flexible path to controller handler mapping scheme;
* Support filter chain to facilitate the execution of unified logic (such as login verification, Http Method constraint verification, etc.) before the controller;
* Support https (based on OpenSSL implementation);
* Support websocket (server side);
* Support Json format request and response;
* Support file download and upload;
* Support gzip compression transmission;
* Provides a lightweight command line tool, drogon_ctl, to simplify the creation of various classes in Drogon and the generation of view code;


### For more information, please visit the [wiki](https://github.com/an-tao/drogon/wiki)


