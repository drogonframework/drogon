![](https://github.com/an-tao/drogon/wiki/images/drogon-white.jpg)

[![Build Status](https://travis-ci.com/an-tao/drogon.svg?branch=master)](https://travis-ci.com/an-tao/drogon)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/45f8a65ca1844788b9109c0044a618f8)](https://app.codacy.com/app/an-tao/drogon?utm_source=github.com&utm_medium=referral&utm_content=an-tao/drogon&utm_campaign=Badge_Grade_Dashboard)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/an-tao/drogon.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/an-tao/drogon/alerts/)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/an-tao/drogon.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/an-tao/drogon/context:cpp) 
[![Join the chat at https://gitter.im/drogon-web/community](https://badges.gitter.im/drogon-web/community.svg)](https://gitter.im/drogon-web/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

**Drogon** is a C++14/17-based HTTP application framework. Drogon can be used to easily build various types of web application server programs using C++. **Drogon** is the name of a dragon in the American TV series "Game of Thrones" that I really like. 

Drogon's main application platform is Linux. It also supports Mac OS and FreeBSD. Currently, it does not support windows. Its main features are as follows:

* Use a NIO network lib based on epoll (kqueue under MacOS/FreeBSD) to provide high-concurrency, high-performance network IO, please visit the [benchmarks](https://github.com/an-tao/drogon/wiki/benchmarks) page for more details;
* Provide a completely asynchronous programming mode;
* Support Http1.0/1.1 (server side and client side);
* Based on template, a simple reflection mechanism is implemented to completely decouple the main program framework, controllers and views.
* Support cookies and built-in sessions;
* Support back-end rendering, the controller generates the data to the view to generate the Html page, the view is described by a "JSP-like" CSP file, the C++ code is embedded into the Html page by the CSP tag, and the drogon command-line tool automatically generates the C++ code file for compilation;
* Support view page dynamic loading (dynamic compilation and loading at runtime);
* Provide a convenient and flexible routing solution from the path to the controller handler;
* Support filter chains to facilitate the execution of unified logic (such as login verification, Http Method constraint verification, etc.) before controllers;
* Support https (based on OpenSSL);
* Support WebSocket (server side);
* Support JSON format request and response, very friendly to the Restful API application development;
* Support file download and upload;
* Support gzip compression transmission;
* Support pipelining;
* Provide a lightweight command line tool, drogon_ctl, to simplify the creation of various classes in Drogon and the generation of view code;
* Support NIO-based asynchronously reading and writing database (PostgreSQL and MySQL(MariaDB) database);
* Support asynchronously reading and writing sqlite3 database based on thread pool;
* Provide a convenient lightweight ORM implementation that supports for regular object-to-database bidirectional mapping;

### For more information, please visit the [wiki](https://github.com/an-tao/drogon/wiki) site
