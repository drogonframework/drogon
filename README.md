![](https://github.com/an-tao/drogon/wiki/images/drogon-white.jpg)

[![Build Status](https://travis-ci.com/an-tao/drogon.svg?branch=master)](https://travis-ci.com/an-tao/drogon)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/45f8a65ca1844788b9109c0044a618f8)](https://app.codacy.com/app/an-tao/drogon?utm_source=github.com&utm_medium=referral&utm_content=an-tao/drogon&utm_campaign=Badge_Grade_Dashboard)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/an-tao/drogon.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/an-tao/drogon/alerts/)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/an-tao/drogon.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/an-tao/drogon/context:cpp)

**Drogon** is a C++11-based HTTP application framework. Drogon can be used to easily build various types of web application server programs using C++. **Drogon** is the name of a dragon in the American TV series "Game of Thrones" that I really like. 

Drogon's main application platform is Linux. It also supports Mac OS and FreeBSD. Currently it does not support windows. Its main features are as follows:

* The network layer uses a NIO framework based on epoll (kqueue under MacOS/FreeBSD) to provide high-concurrency, high-performance network IO;
* Full asynchronous programming mode;
* Support Http1.0/1.1 (server side and client side);
* Based on template, a simple reflection mechanism is implemented to completely decouple the main program framework, controller and view.
* Support for cookies and built-in sessions;
* Support back-end rendering, the controller generates the data to the view to generate the Html page, the view is described by a "JSP-like" CSP file, the C++ code is embedded into the Html page by the CSP tag, and the drogon command-line tool automatically generates the C++ code file for compilation;
* Support view page dynamic loading (dynamic compilation and loading at runtime);
* Very convenient and flexible path to controller handler mapping scheme;
* Support filter chain to facilitate the execution of unified logic (such as login verification, Http Method constraint verification, etc.) before the controller;
* Support https (based on OpenSSL);
* Support websocket (server side);
* Support Json format request and response, very friendly to the Restful API application development;
* Support file download and upload;
* Support gzip compression transmission;
* Provides a lightweight command line tool, drogon_ctl, to simplify the creation of various classes in Drogon and the generation of view code;
* Asynchronous database read and write based on NIO, currently supports PostgreSQL and MySQL (MariaDB) database;
* Asynchronously read and write sqlite3 database based on thread pool, providing the same interfaces as above databases;
* Convenient lightweight ORM implementation that supports regular object-to-database bidirectional mapping;


### For more information, please visit the [wiki](https://github.com/an-tao/drogon/wiki)
