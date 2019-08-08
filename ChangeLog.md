# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

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

[Unreleased]: https://github.com/an-tao/drogon/compare/v1.0.0-beta6...HEAD

[1.0.0-beta6]: https://github.com/an-tao/drogon/compare/v1.0.0-beta5...v1.0.0-beta6

[1.0.0-beta5]: https://github.com/an-tao/drogon/compare/v1.0.0-beta4...v1.0.0-beta5

[1.0.0-beta4]: https://github.com/an-tao/drogon/compare/v1.0.0-beta3...v1.0.0-beta4

[1.0.0-beta3]: https://github.com/an-tao/drogon/compare/v1.0.0-beta2...v1.0.0-beta3

[1.0.0-beta2]: https://github.com/an-tao/drogon/compare/v1.0.0-beta1...v1.0.0-beta2

[1.0.0-beta1]: https://github.com/an-tao/drogon/releases/tag/v1.0.0-beta1
