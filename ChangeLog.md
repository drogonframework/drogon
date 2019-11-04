# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

## [1.0.0-beta10] - 2019-11-04

### API change list

- None

### Changed

- Add the headers configuration option for static files

### Fixed

- Fix(compilation on alpine): Replace u_short alias.


## [1.0.0-beta9] - 2019-10-28

### API change list

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

- Fix a busy loop bug when connections to mysql server are timeout.


## [1.0.0-beta8] - 2019-10-03

### API change list

- Add length() method to the Field class.

- Add `as<bool>()` function template specialization to the Field class.

- Add add attribute store methods to the HttpRequest class.

- Add the setCustomContentTypeString() method to the HttpRequest class.

- Add thread storage.


### Changed

- Use .find('x') instead of .find("x") in a string search.

- Add the ability to create restful API controllers.

### Fixed

- Fix a bug of creating models for mysql.

- Fix a bug when HTTP method is PUT.

- Fix a bug when using 'is null' substatement in ORM.

- Fix a sqlite3 bug when some SQL errors occur.

- Fix bug with parsing json.

- Fix url decode.

- Fix a error in HttpClient.

- Fix a error in setThreadNum method.

- Fix some race conditions.

## [1.0.0-beta7] - 2019-08-31

### API change list

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

### API change list

- None

### Changed

- Modify the 'create view' sub-command of drogon_ctl

- Optimize the transmission of pipelining responses.

- Add the DrogonConfig.cmake file so that users can use drogon with the `find_package(Drogon)` command.

## [1.0.0-beta5] - 2019-08-01

### API change list

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

### API change list

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

### API change list

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

### API change list

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

[Unreleased]: https://github.com/an-tao/drogon/compare/v1.0.0-beta10...HEAD

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
