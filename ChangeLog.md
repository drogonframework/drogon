# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Added

- dg_ctl create models new command line option -o | --output

## [1.9.10] - 2025-02-20

### API changes list

- Add setConnectionCallback.

### Changed

- ORM:Avoid unnecessary copies when returning search results.

- Improve the zh-TW README translation.

- Make quit function thread safe.

- Added path_exempt in AccessLogger plugin config to exclude desired paths.

### Fixed

- Fix the issue in view generation by including the missing header file.

- Fix ci: codespell.

## [1.9.9] - 2025-01-01

### API changes list

- Added Partitioned flag for cookies.

### Changed

- Update FindFilesystem.cmake to check for GNU instead of GCC for CMAKE_CXX_COMPILER_ID.

- Update README.

- Chore(workflow/cmake.yml): upgrade macos runner.

- Add emptiness check to the LogStream &operator<< with std::string_view.

### Fixed

- Fix a bug in plugin Redirector.

- Fix CMAKE issues mentioned in #2144 and a linking problem which manifest with gcc12.3 when building with shared libs.

- Fix: Remove dependency on locales being installed on the system.

## [1.9.8] - 2024-10-27

### API changes list

- Add in-place base64 encode and decode.

- Add check the client connection status.

### Changed

- Add Hodor whitelists.

- Include exception header for std::exception_ptr.

- Add support for escaped identifiers in Postgresql.

- Remove content-length header from 101 Switching Protocols response.

- Remove websocketResponseTest from windows shared library env.

- Optimize query params and allow for empty values.

- Replace rejection sampling and remove use of rand().

- Add sending customized http requests to drogon_ctl.

### Fixed

- Fix coroutine continuation handle.

- Fix some bugs in plugin PromExporter.

- Fix a bug after removing content-length header in some responses.

## [1.9.7] - 2024-09-10

### API changes list

- Add coroutine mutex.

- Add requestsBufferSize function.

- Refine SQLite3 error types with new exception handling.

- Add a new method to reload SSL files on the fly.

### Changed

- Allow MultiPartParser to be movable.

- Add quotes to the table name in the ORM generator.

- Change stoi to stoul in the Field class.

- Modernize cookies.

- Change a log level.

### Fixed

- Use correct libraries when compiling statically.

## [1.9.6] - 2024-07-20

### API changes list

- Add setsockopt to HttpServer.

- Support request stream.

### Changed

- Allow MultiPartParser to parse PATCH requests.

- Add an example of prometheus.

- Delay parsing json for HttpClient.

- Update README.md.

### Fixed

- Fix some compilation warnings.

- Fix typo in yaml config.

## [1.9.5] - 2024-06-08

### API changes list

- Fix an error in the yaml format config file.

- Support postgresql connection options.

- Add regex support for websocket controller.

- Add the registerMiddleware method.

### Changed

- Add the conan badge to readme files.

- Install gcc in ci.

- Intention to present an alternative to improve the performance of a method in models.

### Fixed

- Fix an error in the yaml format config file.

- Fix CI on Windows.

- Fix some spelling errors.

## [1.9.4] - 2024-05-04

### API changes list

- Add client cert support for websocket.

- Add JSON send overloads for WebSocket connections.

### Changed

- Minor enhancement: move some smart pointers around instead of copying them.

- Remove the request shared_ptr from the multipart parser.

- Fix typo in HttpAppFrameworkImpl.cc.

- Avoid string copy and lowercasing on every request.

- Implemented database reconnection loop.

### Fixed

- Bypass clang thread_local error.

## [1.9.3] - 2024-02-09

### API changes list

- Added getParameter() and getOptionalParameter().

- Change drogon::MultiPartParser's parameters data type.

- Use std::string_view for WebSockets.

### Changed

- Add support for gentoo linux, dev-db/mariadb contains mysql.

- Introduce cpplint to the CI.

- Enable readability/alt_tokens for cpplint.

- Use clang-format-17.

- Add newline at EOF.

- Enable readability/inheritance for cpplint.

- Enable build/explicit_make_pair for cpplint.

- Enable build/include_order for cpplint.

- Enable build/header_guard for cpplint.

- Enable build/storage_class for cpplint.

- Enable readability/multiline_string for cpplint.

- Alias the safe hashmap template.

- Simplify traits in utils.

- Enhancement: extend drogon::ContentType for file handling.

### Fixed

- Fix a wrong place of return.

- Fix drogon::util::fromString().

## [1.9.2] - 2024-01-18

### API changes list

- Feature: Integrate spdlog as logging backend.

- Support asynchronous sending of chunked responses.

### Changed

- Modify the configuration file templates in drogon_ctl.

- Use execute_process instead of exec_program in FindJsoncpp.cmake.

- GitHub Action to find typos in the codebase using codespell.

- add discord link to readme.

- Add -k option to the drogon_ctl when running the press command.

- Refine request routing process.

- Add CI tests with more compilers.

- Avoid a race condition in database listener tests.

- Remove macos-11 CI; not supported by Homebrew.

- Bump github/codeql-action from 2 to 3.

- Move the RealIpResolver plugin to the PreRouting join point.

### Fixed

- Fix: typo on Mapper method.

- Fix a error of coroutines on Windows.

- Fix ORM: The original way did not handle exceptions correctly.

- Remove the default ctor of the Row class in ORM.

- Set the url of trantor to the official repository.

- Fix htonll/ntohll redefinition.

- Fix building with MSYS2.

- Fix name issue when cross-compiling.

## [1.9.1] - 2023-11-27

### API changes list

- Pass HttpRequestPtr to custom error handlers.

- Provide some functions for incrementing the value of given columns.

- Return HttpAppFramework by setExceptionHandler.

### Changed

- Custom sessions.

- Use the constexpr if instead of std::enable_if.

- Make id generator consistent.

- Update test_cmake.csp.

- Simplify drogon test with c++17.

- Remove unused and undefined overloads of isBase64.

### Fixed

- Fix build due to trantor commit out of date and address warnings.

- Fix a bug of the GlobalFilters plugin.

- Fix: uuid formatting.

## [1.9.0] - 2023-10-29

### API changes list

- Added isTopicEmpty function;

### Changed

- Update the ubuntu Dockerfile;

- Add optional Criteria && || operator support;

- Bump actions/checkout from 3 to 4;

- Make & and * directly adjacent to variable names;

- Use wss://echo.websocket.events/.ws in WebSocket client example;

- Change logs in the AccessLogger plugin to TRACE level;

### Fixed

- Fix an error in the secureRandomString function;

- FIX int mapping to int64_t instead of uint64_t;


## [1.9.0-rc.1] - 2023-09-23

### API changes list

- Drop cpp14 build support.

- Add isHead() method to HttpRequest, to preserve information about the original method for use in the controller.

- Allow omitting template parameter in execCommandSync.

- Add a method to HttpRequest to access the matched routing parameters.

### Changed

- Update readme files.

- Allow sync advice to be callable on websocket requests.

- Set concurrency to prevent blocking CI queue.

- Validate clang-format version & Customize clang-format path.

- Extract format action into distinct job.

- Split macOS and Ubuntu CIs for readability.

- Set concurrency for CodeQL.

- Add dependabot.yml for GH actions.

- Replace sprintf with snprintf.

- Use ninja to build faster.

- Avoid using well-known ports for demoMain.

- Simplify coroutine implementation.

- Add a plugin for prometheus.

- Optimize plugins with redirection functions.

- Optimize regex generator.

- Add override keyword to setSockOptCallback.

- SlashRemover optimization.

### Fixed

- Fix race condition when setting the secure flag during test.

## [1.8.6] - 2023-08-23

### Changed

- Show outputs of try_compile for UUID libs.

- Update Trantor to fix a serious bug when sending files.

## [1.8.5] - 2023-08-19

### API changes list

- Add the forwardCoro method to HttpAppFramework.

- Possibility to add plugins without config file.

- Implement QueryBuilder.

- Add getOriginalPath function.

- Add setsockopt to HttpClient.

- Add overload function of newHttpResponse.

- Add getConnectionCount method.

### Changed

- Add synchronization interface to model's associated query.

- Use syncAdvices.empty() to check.

- Remove the deprecated Json::Reader.

- Adapt Drogon to take advitange of Trantor TLS refactor.

- Add avatars of all contributors to the readme file.

- Remove docsforge from readme.

- Adapt to gcc13.

- Modify the HttpMessageBody class.

- Add the GlobalFilters plugin.

- Add filters and plugins to the drogon.h header file.

- Add webp and svg to the default file_types.

- Update SqlBinder.h.

- Remove path from COMPILER_COMMAND.

- Hodor plugin - Use IP bytes for user IP identification.

- Add SlashRemover plugin.

- Support setting max_files in loggers.

- Use shared_ptr to store plugins.

- Throw custom exception in HttpClient.

- Base64 improvements.

- Add CT_TEXT_JAVASCRIPT.

- Fix typo in newHttpJsonResponse documentation.

- Add CodeQL workflow.

- constexpr base64 length calculators.

- Make isBase64() and isInteger() take string_view.

- Chore: add package.xml.

- Add an example of yaml config file.

- Remove some unused functions.

- Use unsigned char to call the std::isspace function.

- Change options BUILD_CTL and BUILD_EXAMPLES and update readme with Building options.

- Remove unused CI files and Jekyll config.

- Ensure that all filters, AOP advice, and handlers are executed within the IO threads.

- Update test.sh and build.sh by appending prefix "X" to string variable comparisons.

- Recognize URI in request lines.

- Address warnings on macOS CI.

- Perform insensitive string compare of cookie SameSite attribute.

### Fixed

- Temporary fix dead HttpClient.

- Fix a configuration error when building drogon_ctl.

- Fix generation of JSON field validation for short type.

- Fix bug on Mac M1 with redis chat example code.

- Fix error: conversion from ‘long int’ to ‘Json::Value’ is ambiguous.

- Bugfix: PgBatchConnection did not report error message.

- Fix typo in config file.

- Fix a hanging bug when writing large string to text field.

- Fixed logic error of limiterExpireTime_.

- Fix Missing Header BaseBuilder.h in CMakeLists.

- Fix: Coroutine handle should be captured by value.

- Fix issues in the ListParaView and content types.

- Fix target link libraries of yaml-cpp to comaptiable with vcpkg.

- Fix build error on win32/mingw.

- Fix CI in MacOS.

- Fix broken link in CONTRIBUTING.md.

## [1.8.4] - 2023-03-19

### API Changes list

- Add a configuration option to display local time in logs.

- Add the stackLimit option for jsoncpp.

- Add coroutine to wait until event loop ends.

- Add switchThreadCoro().

- Add queueInLoopCoro function.

### Changed

- Reduce overhead of constructing string from request status.

- Add coroutine parameter binding test and make tests not blocking.

- Use weakptr instead of shared_ptr in HttpClient.

- Refactor HttpServer codes.

- Add an option in CMakeLists.txt to set how to use trantor.

- Add support to yaml config file.

- Changes to free http client if no request is pending.

- Trigger error message in drogon test when faced with bad parameter.

### Fixed

- Make the token bucket full when it is initialized

- Fix file r/w race condition in integration test.

- Postgresql: don't handle events anymore after connection closing.

- Fix multipart boundary with charset.

- Fix a conan issue in github actions.

- Hold shared pointer from the very beginning in WebSocketConnectionImpl.

## [1.8.3] - 2023-01-23

### API changes list

- Add onSessionStart() and onSessionDestroy() events

- Support postgresql asynchronous notification (LISTEN/NOTIFY).

### Changed

- Update Utilities.cc

- Drogon test refactor

- Enable json entry as string

- Mark awaiters as non-copyable

### Fixed

- Fix HttpController regex path matching

- Fix a memory leak in the redis example

- Partial fix for MSYS2 compat

## [1.8.2] - 2022-11-11

### API changes list

- Add the queueInLoopCoro function.

- Avoid HashDoS attacks via random per-session hash initial state.

### Changed

- Support the mediumint column when generate the mysql model.

- Set Hiredis_FOUND to true when finding Hiredis library.

- Add rate limiter.

- Add some test cases for the sqlite datetime type.

### Fixed

- Fix typo in drogon_test.h.

- Fix a date race in drogon_test.

- Fix a deadlock bug when closing all database connections.

## [1.8.1] - 2022-09-25

### API changes list

- Support redis subscription.

### Changed

- Remove redundant member functions of drogon::Task.

- Small patches on orm_lib.

- Add support for the string_view type to SqlBinder in orm.

### Fixed

- Fix a conflict of ssize_t type with hiredis.

- Fix a test bug when clients start before servers.

- Fix model template file Unreachable code.

- Use the mysql_library_end() function to avoid memory leaks.

## [1.8.0] - 2022-09-01

### API changes list

- Add ‘not like‘ criteria.

- Add HttpResponse::newStreamResponse().

- Add the same site option for session cookie.

- Add support for custom SQL query.

### Changed

- Update issue templates.

- Enable automatic reconnect in mysql.

- Add typename for clang-14.

- A workaround for redis sync exec.

- Resolve redis server hostname from config file.

- Add username option to redis databases.

- Return nullptr if a plugin is not loaded when getting it.

- Support controller registration in plugin.

- Check mysql-optionsv support in cmake.

- Check if host header is set before setting.

- Clear all database connections before quitting.

- Add namespace to views when using drogon_ctl.

- Support pipeline mode on PostgreSQL 14+.

- Add content type to multipart file upload.

- Make orm::Result compatible with C++ STL container concepts.

- Throw exceptions instead of exiting when loading configuration fails.

- Rename BUILD_TRANTOR_SHARED to BUILD_SHARED_LIBS.

- Support compressed request.

- Prevent sending multiple responses for a single request.

- Remove the virtual specifier from functions marked with override.

- Remove redundancies from the CMake action.

- Ensure requiring a semi-colon after macros.

- Omit redundant virtual specifiers.

- Refactor orm::SqlBinder.

- Implement toJson to convert std::vector to Json::Value.

- Resolve real ip from HttpRequest.

- Delete the unmaintained test script.

- Change the listener port of the cookie test.

- Use a raw string literal for the drogon banner.

- Change timeout of pipeline test for the CI environment.

- Accept "postgres" for DbClient type as well.

- Log remote real address in AccessLogger.

- Support coroutine filter.

- Refactor db_test.cc.

- Use nullopt instead of the no-argument constructor.

- Set the running flag to false after calling the quit() method.

- Fix doc link in README files.

### Fixed

- Fix XXXControllers created on MSVC even if specified not to do so.

- To avoid accessing a null point, make sure `result == OK` before accessing the response ptr.

- Fix a bug when stopping redis service.

- Fix mutex lock missing.

- Fix tolower with cfi sanitizer.

- Add move constructor to fix clang-14 compile error.

- Fix HttpClient dns cache.

- Fix bug when resolving redis server hostname.

- Reset timer afters relaunching on Linux.

- Fix some configuration file issues.

- Fix HttpFile unittest error on Windows.

- Fix core dump causing by logging in destructor.

- Fixing link error when linking with static c-ares.

- Remove redundant resource release.

- Install missing header file apply.h.

- Fix deleteFutureByPrimaryKey compile fail.

- Fix compilation failure without database support.

- Fix Mapper::updateBy() async api.

- Fix no BUILD_CTL with tests.

- Fix some bugs in RedisClient.

- Fix a misuse of std::move.

- Fix a bug when creating models with composite keys in sqlite3.

- Fix a bug when converting the content-length string to size_t.

- Fix a bug when parsing multipart/form-data.

- Export the getVersion method for Windows.

- Add a pre-compilation macro in the pg pipeline test code.

## [1.7.5] - 2022-02-19

### API changes list

- Add toString for drogon::ReqResult.

- Add max-age, samesite options to Cookie.

- Enable setup output of logs to files at any time.

### Changed

- Use operator<< to convert ReqResult to string.

- Remove sudo from build.sh.

- Remove sudo from dependencies in Dockerfile.

- Avoid attempt linking to std::fs when target does not exist.

- Destroy fastdb client on quit.

- Check HTTP client is not sending requests in sync mode on the same event loop.

- Start listening after beginning advice.

- Allow using json_cpp in other sublibraries.

- Accept system lib for uuid on macOS.

- Add `Not In` to ORM Criteria.

### Fixed

- Fix WS test potentially can get stuck.

- Fix a bug in model generation.

- Prevent malformed upload path causing arbitrary write.

- Fix missing "using namespace drogon::orm;" at autogenerated restful controllers.

## [1.7.4] - 2021-12-11

### API Change List

- Support setting client certificate and SSL options on HTTP client

- Add more method for mapper

- Add overloads for SqlBinder::operator<< with non-const ref parameter

### Changes

- Use decay_t instead of remove_cvref_t

- Prevent `drogon_ctl create_view` appending empty new lines to resulting

- Add an example for using coroutines of redis clients

- Export some symbols for Windows

- Mark all awaiters as nodiscard

- Handle SIGINT too

- Support CoroMapper method chaining

- Remove setting c++17 in FindFilesystem

### Fixed

- Fix Drogon not building caused by FindFilesystem

- Fix deprecated warning when using openssl 3

- Fix coroutine object destructing before coroutine ends in async_run

- Fix build fail on CentOS8

- Fix some compiler warnings

- Fix the error with multiple results when calling a procedure in mysql

- Fix an error when binding a function pointer to SqlBinder

- Fix orm tests

- Fix CI to actually build in C++14

- Fix a race condition when resetting ws

- Fix an error of std::bad_function_call

- Update Trantor (fix sending partial files)

## [1.7.3] - 2021-10-17

### API Change List

- Support sending files by range

- Allow outside access to the file path of an HTTP response

- Support custom MIME types and extensions

- Add the getOptionalParameter method

- Add async_run

### Changes

- Experimental HaikuOS Support

- Improve AccessLogger

- Add Alpine Dockerfile

- Add option to disable brotli if desired by the builder

### Fixed

- Fix a bug in the getIOLoop method

- Return on redis connection errors

- Fix(MultiPart): Does not respect quotes in Content-Disposition header

- Fix(cmake): error in FindFilesystem

- Fix(style): Change the NotFound page text color

- Fix a race condition in testing

## [1.7.2] - 2021-08-24

### API Change List

- Add port() and host() to HttpClient

- Add stop() method to the WebSocketClient class

### Changes

- Enables higher level of warnings when building on UNIX with GCC

- Generic optimizations

- Add redis example

- Added support for paths containing unicode characters on Windows

- Load ParseAndAddDrogonTests in DrogonConfig

- Add BUILD_DOC to cmake options

- Add websocket server example

- CMake: Add CPack for .deb and .rpm package generation

- cmake: Use GNUInstallDirs to figure out install dirs.

### Fixed

- Fix WS client example not working with integration_test

- Fix WS client example error when encountering bad IP addresses

- CacheFile supports >2GB files on 64-bit Windows

- `drogon_ctl` now emits error on failing to create view from CSP

- Added the `make` program to Ubuntu docker environment

- Correctly check the case-insensitive value of the upgrade header of responses in websocket connections

- Fix incorrect MD5 hash when using internal MD5 implementation when input size == block size+1

- Fix test success message incorrectly shown for a failed test when -s is flag present

- Force using boost::filesystem when building for Android

- Escape connection string in drogon_ctl create model

- Fix some memory leak and race conditions in WebSocketClient

## [1.7.1] - 2021-06-24

### Changes

- Updated Dockerfile to Ubuntu 20.04 & Fixed Timezone Hangup.

- Add jsonstore example.

- Fix some typos.

### Fixed

- Fix single layer directory traversal in StaticFileRouter.

## [1.7.0] - 2021-06-18

### API changes list

- Add the PreSendingAdvice to AOP.

- Make Json::Value as a SQL parameters type.

- Add the int type for the Row index parameter.

- Add SSL_CONF_cmd support.

- Add the setCustomStatusCode method.

### Changes

- Fix sync_wait/co_future use-after-free.

- Add the AccessLogger plugin.

- Make AsyncTask only destruct when the coroutine reaches end of executions.

- Add Drogon test framework.

- Improve WebSocket mask handling.

- Add minimal server side examples.

- Optimize HttpControllersRouter for cases where regex is not needed.

- Create controller instances after running instead of after being called.

### Fixed

- Move resolverPtr when destroying an HttpClientImpl object.

- Modify the way to create sqlite3 client.

- Fix a bug when a network failure occurs on Redis connections.

- Fix a bug of string_view for MSVC.

- Fix 'build.sh -tshared'.

- Fix compiler warnings.

- Fix CacheMap crash in CI tests.

## [1.6.0] - 2021-05-15

### API changes list

- Add option to set default handler.

- Add the setTimeout() method to the DbClient class and the RedisClient class.

- Add the validateCert parameter to the newWebSocketClient method.

### Changed

- A few mini changes to drogon_ctl command.

- Improve the MultiPartParser class.

- Add GNU -Werror & fix warnings.

- Enhancements on files part.

- Add version/soversion to shared library.

- Disallow coroutines to be resolved as plain subroutine handlers.

- Send the content-length header even if the body(POST,PUT,OPTIONS,PATCH) is empty.

- Use make_exception_ptr instead of throw/catch when possible.

- Remove duplicated inclusion.

- Print error before terminating in AsyncTask.

- Allow users to override drogon Find modules.

- Use two-phase construction for the DbClientImpl and the RedisClientImpl.

- Add support 'select &lt;db&gt;' for redis.

### Fixed

- Fix a bug of the Transaction class.

- Copy CoroMapper.h to installation location.

- Remove the related request from the buffer if it's not sent after the timeout.

- Fix ORM with SQLite3 not compiling on Arch Linux.

- Fix an error when constructing RedisClientImpl objects.

- Fix coroutine frame leak upon assigning to awaitable.

- Set running flag to true before installing plugins.

- Fix double free in coroutine exception handling.

## [1.5.1] - 2021-04-10

### Fixed

- Fix a bug of reflection failure.

## [1.5.0] - 2021-04-10

### API changes list

- Add option to disable signal handling.

- Added newFileResponse Support for buffers in memory.

- Add a method to HttpRequest to set the user_agent header.

- Catch exceptions thrown by handlers.

### Changed

- Add convert method to models.

- Add Arch Dockerfile.

- Add Redis support.

- Print error and exit when IP parsing failed in server startup.

- Use a canonical way of calling max() function on Windows.

- Remove an assertion statement in the HttpClientImpl class.

- Send ping messages by default for WebSockets.

- Use canonical cmake logic for cross-compilation.

- set make job count to the number of threads in GitHub Actions workflow.

- Use lambda instead of std::bind in HttpServer.

- Add exports macro to allow Shared Library with hidden symbols by default.

- Remove repeated class names on relationships from the model generator.

### Fixed

- Fix compile warnings in SQL client.

- Fix compilation errors for the TimeFilter example.

- Fix build.sh missing nproc error in build for macOS.

- Fix a bug when creating sqlite3 models.

- Fix two building corner cases, CMake quality of life improvements.

- Add CoroMapper to models' friends.

## [1.4.1] - 2021-03-07

### Fixed

- Fix a bug of DbClientImpl class that can lead to a crash when database connections are breaking.

## [1.4.0] - 2021-03-05

### API changes list

- Add coroutine support.

- Add default value interface to SqlBinder for MySQL and PostgreSQL.

- Support SNI in the HttpClient class.

- Validate certificate in HttpClient.

- HttpRequest: add a feature to avoid URL encoding of the path.

### Changed

- Handle cross-compiling properly.

- Lowercase all HTTP headers, add webp and avif types.

- Modify FindMySQL.cmake

### Fixed

- Fix an error in the HttpClient class when a response has no content-length.

- Return 404 or 405 responses correctly.

- Fix compilation errors on vs2019.

- Fix stack use after scope error in client_example.

- Fix the error when the SSL handshake fails.

## [1.3.0] - 2021-01-16

### API changes list

- Add an option for setting float precision in Json string.

### Fixed

- Fix brotli link order.

- Fix cmake with drogonctl cross-compilation.

- sqlite3: Insert into stmtsMap_ as string_view.

- Fix some bugs when creating models via drogon_ctl.

- Fix an error in sqlite3 ORM generator.

- Fix an error with missing composite key to sqlite3 ORM generator.

### Changed

- Remove the use of std::filesystem to adapt to old compilers.

- Add github actions.

- Serve wasm files with the correct MIME type.

## [1.2.0] - 2020-12-12

### Fixed

- Fix error when receiving response without content-length header.

- Fix a stack-overflow error when high concurrency happening on sqlite3.

- Fix MinGW ORM building by enabling htonll and ntohll.

### Changed

- Modify the WebSocketTest controller to create a simple chat room.

- Add support for OpenBSD.

- Return 400 if the content-length is invalid.

- Don't send content type in a 304 response.

- Add the reuse_port option to app() interface.

- Add the 'std::optional' support in the SqlBinder class and the Session class.

- Add implicit page resolving capability.

## [1.1.0] - 2020-10-31

### Fixed

- Fix failing to connect to DB if parameters contains spaces.

- Fix a CMAKE bug when SHARED and EXAMPLES are on.

- Fix the HttpServer::isWebSocket method.

- Find MariaDB client library correctly on Ubuntu 20.04.

- Fix a bug when creating sqlite3 database models.

- Fix a bug in the Mapper::insertFuture method.

### Changed

- Disable TLS1.0/1.1 on HTTPS by default.

- Use explicit lambda capture lists.

- Modify the procedure of the app().run() method.

- Support namespaces when creating view source files.

- Add --path-to-namespace option to drogon_ctl for creating views.

- Add the Host and Sec-WebSocket-Version headers when connecting to a websocket server.

## [1.0.0] - 2020-09-27

### Fixed

- Fix an issue of simple_reverse_proxy when handling chunked transfer-encoding.

- Fix a bug when losting connection to MySQL server during query.

- Remove the expired std::iterator template.

- Fix a bug when creating models in some special cases.

### API changes list

- Modify methods related to headers.

- Remove the expired std::iterator template.

- Add getListeners() method to the HttpAppFramework class.

- Remove the useless method stat() from the PluginBase class.

- Add ConfigLoader::ConfigLoader(const Json::Value &data).

### Changed

- Add support for status code 418.

- Modify session handling.

- Modify the FileUpload.csp in simple_example to avoid CORS.

- remove execution permission on /tmp/drogon.lock.

## [1.0.0-beta21] - 2020-08-19

### Changed

- Modify the Result class in ORM.

### Fixed

- Fix zlib link error on Windows for the latest vcpkg.

## [1.0.0-beta20] - 2020-08-15

### API changes list

- Provide users with a method to change the session ID of a session.

### Changed

- Modify parseContentType function.

- Modify the docker file to build release version in docker.

- Set session to requests for websockets.

- Modify parseContentType function.

- Change the return value type of the mktime() function in models.

- Fix compilation warning of sprintf function.

### Fixed

- Fix a bug when saving uploaded files on Windows.

- Fix a MySQL issue when connections are lost.

- Resolve an issue when sending big files (>=2GB) on Windows.

- Fix boost::string_view compilation error of MysqlConnection class.

- Set the response Access-Control-Allow-Headers header correctly for CORS.

- Fix a bug in drogon_ctl when creating a model, that causes to write source files multiple times.

## [1.0.0-beta19] - 2020-07-16

### API changes list

- Add a method to disable unicode escaping in json string.

- Add a timeout parameter when sending HTTP requests.

- Add the getJsonError method.

### Changed

- Remove the restriction on the location of layout tags in views.

- Add a way to set the character set when creating DbClient objects.

- Make `GET` as the only method for accessing static files.

- Modify the 404 pages generator.

- Modify the DbClient class.

- Optimize the HttpResponse class.

### Fixed

- Properly handle chunked encoding requests.

- Destroy DNS resolver of HttpClient in the correct thread.

- Add the header &lt;cctype&gt; to resolve build errors in VS2017.

## [1.0.0-beta18] - 2020-06-14

### API changes list

- Add a new joinpoint of AOP for modification on each HTTP response.

- Add a method for the TERM signal handling.

- Add getContextRef method to the WebSocketConnection class.

### Changed

- Create a class template for publish subscribe pattern.

- Add contribution recommendations.

- Send a close message when closing a web socket connection.

- Add additional formats for getHttpDate function.

- Make app().run() method callable on a non-main thread.

- Add digest filter in examples.

- Use string_view to parse multipart/form-data requests.

### Fixed

- Fix building of ORM on FreeBSD.

- Fix a Mysql connection error on Windows.

- Fix a bug in ListenerManager::getIOLoop().

- Fix the count() method of Mysql ORM.

- Fix a compilation issue on windows.

- Fix model generation for PostgreSQL primary keys.

- Fix a bug with quoted column names in sqlite3 databases.

## [1.0.0-beta17] - 2020-05-22

### API changes list

- Add methods to get DbClient connection status

### Changed

- Add causal profiling with coz

- Add filters on static file locations

- Pass data from view to its layout container

- Add additional HttpStatusCodes and implement a custom error handler

- Modify drogon_ctl to show more compilation information

### Fixed

- Fix a bug in drogon_ctl (when size of a line is larger than buffer size)

- Fix a connection bug of MariaDB clients

## [1.0.0-beta16] - 2020-04-27

### API changes list

- Standardize Row and Result api in ORM

### Changed

- Add support for brotli compression

- Parse content-type of HTTP requests

- Remove non standard macros

- Support url safe base64 codec

## [1.0.0-beta15] - 2020-03-28

### API changes list

- Modify the Attributes interface of the HttpRequest class

- Add the getHomePage() method to HttpAppFramework

### Changed

- Support br compression files

- Update Content-Type support for PDF

- Add support for MSVC 2015

- Optimize the rendering of HTTP responses

- Update the Dynamic Views Loading, add the `layout` tag

- Graceful shutdown

### Fixed

- Fix error when finding the jsoncpp library

- Fix the 'many to many' relationship in ORM

- Fix a bug when creating json responses

- Fix a bug on filters with WebSocketControllers

- Fix a fatal bug in the MysqlConnection class

- Fix crash with partial matched url

- Fix null jsonObject from newHttpJsonRequest

## [1.0.0-beta14] - 2020-02-17

### API changes list

- None

### Added

- Add IOLoop access function

### Changed

- Add support for regular expressions when routing

- Add location configuration for static resources

- Port drogon to Windows

- Support 'password' keyword in configuration files

- Remove get_version.sh

- Modify dynamic view loading algorithm, add 'layout' tag for view generation.

### Fixed

- Fix an issue of out-of-range (#334)

- Fix a bug in views generation (#341)

## [1.0.0-beta13] - 2020-01-04

### API changes list

- None

### Changed

- Add some unit tests (based on gtest)

- Add a reverse proxy example

- Make a patch to support the ossp UUID library

- Make shared linking possible

- Add the drogon::OStringStream class

- Optimize ORM

- Modify singleton logic of DrClassMap

### Fixed

- Fix an error in the batch mode of libpq

- Fix an error when clients use HTTP1.0

## [1.0.0-beta12] - 2019-11-30

### Changed

- Make dg_ctl a symlink

- Modify some code styles

- Explicitly set path to '/' for JSESSIONID cookie

- Handle gzip errors safely

- Add the SecureSSLRedirector plugin

### Fixed

- Fix a bug in dg_ctl for creating models of sqlite3

- Reset the flag used to parse json to false before recycling HttpRequest objects

## [1.0.0-beta11] - 2019-11-06

### Changed

- Delete useless log output

## [1.0.0-beta10] - 2019-11-04

### API changes list

- None

### Changed

- Add the headers configuration option for static files

### Fixed

- Fix(compilation on alpine): Replace u_short alias.

## [1.0.0-beta9] - 2019-10-28

### API changes list

- Add interfaces for accessing content of attachments.

- Add option to disable setting the 404 status code of the custom 404 page.

- Make user can use any string as a placeholder's name in routing patterns.

- Add type conversion methods to the HttpRequest and HttpResponse classes.

### Changed

- Modify cmake configuration.

- Modify the quit() method.

- Implement relationships in ORM.

### Fixed

- Fix size_t underflow of drogon_ctl.

- Fix some race conditions.

- Fix a busy loop bug when connections to MySQL server are timeout.

## [1.0.0-beta8] - 2019-10-03

### API changes list

- Add length() method to the Field class.

- Add `as<bool>()` function template specialization to the Field class.

- Add add attribute store methods to the HttpRequest class.

- Add the setCustomContentTypeString() method to the HttpRequest class.

- Add thread storage.

### Changed

- Use .find('x') instead of .find("x") in a string search.

- Add the ability to create restful API controllers.

### Fixed

- Fix a bug of creating models for MySQL.

- Fix a bug when HTTP method is PUT.

- Fix a bug when using 'is null' substatement in ORM.

- Fix a sqlite3 bug when some SQL errors occur.

- Fix bug with parsing json.

- Fix url decode.

- Fix a error in HttpClient.

- Fix a error in setThreadNum method.

- Fix some race conditions.

## [1.0.0-beta7] - 2019-08-31

### API changes list

- Remove the default value parameter of some methods (#220)

### Changed

- Optimize DNS in HttpClient and WebSocketClient (support c-ares library).

- Reduce dependencies between declarations.

- Add database tests in the travis CI and add test cases to database tests.

- Reduce size of docker image.

- Make the framework API support chained calls.

- Add a synchronous join point for AOP.

- Modify the CMakeLists to modern cmake style.

### Fixed

- Fix bugs in default return values of functions(#220),

- Fix a bug in the cmake configuration file when there's '+' in the building path.

- Fix a bug in drogon_ctl (when creating orm models)

## [1.0.0-beta6] - 2019-08-08

### API changes list

- None

### Changed

- Modify the 'create view' sub-command of drogon_ctl

- Optimize the transmission of pipelining responses.

- Add the DrogonConfig.cmake file so that users can use drogon with the `find_package(Drogon)` command.

## [1.0.0-beta5] - 2019-08-01

### API changes list

- None

### Added

- Add two methods to control if the Server header or the Date header is sent to clients with HTTP responses.
    * void HttpAppFramework::enableServerHeader(bool);
    * void HttpAppFramework::enableDateHeader(bool);

### Changed

- Support high performance batch mode of libpq.

### Fixed

- None

## [1.0.0-beta4] - 2019-07-30

### API changes list

- HttpRequest::query() returns a const reference of std::string instead of a string_view
- WebSocketConnection::setContext(), WebSocketConnection::getContext(), etc.
- Remove the config.h from public API.

### Added

- None

### Changed

- Modify the CMakeLists.txt
- Modify the get_version.sh

### Fixed

- None

## [1.0.0-beta3] - 2019-07-28

### API changes list

- None

### Added

- Add a README file for examples.
- Add some managers to reduce the size of the HttpAppFrameworkImpl code.
- Add missing wasm ContentType.

### Changed

- Update the submodule - trantor.
- Optimize processing of HTTP pipelining.

### Fixed

- Fix an error in the HttpClient class when sending a request using the HEAD method.

## [1.0.0-beta2] - 2019-07-10

### API changes list

- Add setBody methods to the HttpRequest class.
- Add the setContentTypeCodeAndCustomString method to the HttpResponse class.

### Added

- Add stress testing command to drogon_ctl.
- Add -v, -h parameters to drogon_ctl.

### Changed

- Update the submodule - trantor.
- Modify the handling of CORS.
- Optimize the htmlTranslate method and the Field class.
- Make all listeners share IO threads in the MacOS/Unix system.

### Fixed

- Fix a bug of the IsPlugin class.
- Use default constructor of string_view to reset _statusMessage to fix a warning on GCC 9.1 on Arch Linux.

## [1.0.0-beta1] - 2019-06-11

[Unreleased]: https://github.com/an-tao/drogon/compare/v1.9.10...HEAD

[1.9.10]: https://github.com/an-tao/drogon/compare/v1.9.9...v1.9.10

[1.9.9]: https://github.com/an-tao/drogon/compare/v1.9.8...v1.9.9

[1.9.8]: https://github.com/an-tao/drogon/compare/v1.9.7...v1.9.8

[1.9.7]: https://github.com/an-tao/drogon/compare/v1.9.6...v1.9.7

[1.9.6]: https://github.com/an-tao/drogon/compare/v1.9.5...v1.9.6

[1.9.5]: https://github.com/an-tao/drogon/compare/v1.9.4...v1.9.5

[1.9.4]: https://github.com/an-tao/drogon/compare/v1.9.3...v1.9.4

[1.9.3]: https://github.com/an-tao/drogon/compare/v1.9.2...v1.9.3

[1.9.2]: https://github.com/an-tao/drogon/compare/v1.9.1...v1.9.2

[1.9.1]: https://github.com/an-tao/drogon/compare/v1.9.0...v1.9.1

[1.9.0]: https://github.com/an-tao/drogon/compare/v1.9.0-rc.1...v1.9.0

[1.9.0-rc.1]: https://github.com/an-tao/drogon/compare/v1.8.6...v1.9.0-rc.1

[1.8.6]: https://github.com/an-tao/drogon/compare/v1.8.5...v1.8.6

[1.8.5]: https://github.com/an-tao/drogon/compare/v1.8.4...v1.8.5

[1.8.4]: https://github.com/an-tao/drogon/compare/v1.8.3...v1.8.4

[1.8.3]: https://github.com/an-tao/drogon/compare/v1.8.2...v1.8.3

[1.8.2]: https://github.com/an-tao/drogon/compare/v1.8.1...v1.8.2

[1.8.1]: https://github.com/an-tao/drogon/compare/v1.8.0...v1.8.1

[1.8.0]: https://github.com/an-tao/drogon/compare/v1.7.5...v1.8.0

[1.7.5]: https://github.com/an-tao/drogon/compare/v1.7.4...v1.7.5

[1.7.4]: https://github.com/an-tao/drogon/compare/v1.7.3...v1.7.4

[1.7.3]: https://github.com/an-tao/drogon/compare/v1.7.2...v1.7.3

[1.7.2]: https://github.com/an-tao/drogon/compare/v1.7.1...v1.7.2

[1.7.1]: https://github.com/an-tao/drogon/compare/v1.7.0...v1.7.1

[1.7.0]: https://github.com/an-tao/drogon/compare/v1.6.0...v1.7.0

[1.6.0]: https://github.com/an-tao/drogon/compare/v1.5.1...v1.6.0

[1.5.1]: https://github.com/an-tao/drogon/compare/v1.5.0...v1.5.1

[1.5.0]: https://github.com/an-tao/drogon/compare/v1.4.1...v1.5.0

[1.4.1]: https://github.com/an-tao/drogon/compare/v1.4.0...v1.4.1

[1.4.0]: https://github.com/an-tao/drogon/compare/v1.3.0...v1.4.0

[1.3.0]: https://github.com/an-tao/drogon/compare/v1.2.0...v1.3.0

[1.2.0]: https://github.com/an-tao/drogon/compare/v1.1.0...v1.2.0

[1.1.0]: https://github.com/an-tao/drogon/compare/v1.0.0...v1.1.0

[1.0.0]: https://github.com/an-tao/drogon/compare/v1.0.0-beta21...v1.0.0

[1.0.0-beta21]: https://github.com/an-tao/drogon/compare/v1.0.0-beta20...v1.0.0-beta21

[1.0.0-beta20]: https://github.com/an-tao/drogon/compare/v1.0.0-beta19...v1.0.0-beta20

[1.0.0-beta19]: https://github.com/an-tao/drogon/compare/v1.0.0-beta18...v1.0.0-beta19

[1.0.0-beta18]: https://github.com/an-tao/drogon/compare/v1.0.0-beta17...v1.0.0-beta18

[1.0.0-beta17]: https://github.com/an-tao/drogon/compare/v1.0.0-beta16...v1.0.0-beta17

[1.0.0-beta16]: https://github.com/an-tao/drogon/compare/v1.0.0-beta15...v1.0.0-beta16

[1.0.0-beta15]: https://github.com/an-tao/drogon/compare/v1.0.0-beta14...v1.0.0-beta15

[1.0.0-beta14]: https://github.com/an-tao/drogon/compare/v1.0.0-beta13...v1.0.0-beta14

[1.0.0-beta13]: https://github.com/an-tao/drogon/compare/v1.0.0-beta12...v1.0.0-beta13

[1.0.0-beta12]: https://github.com/an-tao/drogon/compare/v1.0.0-beta11...v1.0.0-beta12

[1.0.0-beta11]: https://github.com/an-tao/drogon/compare/v1.0.0-beta10...v1.0.0-beta11

[1.0.0-beta10]: https://github.com/an-tao/drogon/compare/v1.0.0-beta9...v1.0.0-beta10

[1.0.0-beta9]: https://github.com/an-tao/drogon/compare/v1.0.0-beta8...v1.0.0-beta9

[1.0.0-beta8]: https://github.com/an-tao/drogon/compare/v1.0.0-beta7...v1.0.0-beta8

[1.0.0-beta7]: https://github.com/an-tao/drogon/compare/v1.0.0-beta6...v1.0.0-beta7

[1.0.0-beta6]: https://github.com/an-tao/drogon/compare/v1.0.0-beta5...v1.0.0-beta6

[1.0.0-beta5]: https://github.com/an-tao/drogon/compare/v1.0.0-beta4...v1.0.0-beta5

[1.0.0-beta4]: https://github.com/an-tao/drogon/compare/v1.0.0-beta3...v1.0.0-beta4

[1.0.0-beta3]: https://github.com/an-tao/drogon/compare/v1.0.0-beta2...v1.0.0-beta3

[1.0.0-beta2]: https://github.com/an-tao/drogon/compare/v1.0.0-beta1...v1.0.0-beta2

[1.0.0-beta1]: https://github.com/an-tao/drogon/releases/tag/v1.0.0-beta1
