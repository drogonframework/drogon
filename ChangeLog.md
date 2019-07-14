# Changelog
All notable changes to this project will be documented in this file.

## [Unreleased]

## [1.0.0-beta2] - 2019-07-10

### Added

- Add setBody methods to the HttpRequest class.
- Add stress testing command to drogon_ctl.
- Add -v, -h parameters to drogon_ctl.
- Add the setContentTypeCodeAndCustomString method to the HttpResponse class.

### Changed

- Update the submodule - trantor.
- Modify the handling of CORS.
- Optimize the htmlTranslate method and the Field class.
- Make all listeners share IO threads in the MacOS/Unix system.

### Fixed

- Fix a bug of the IsPlugin class.
- Use default constructor of string_view to reset _statusMessage to fix a warning on GCC 9.1 on Arch Linux.

## [1.0.0-beta1] - 2019-06-11

[Unreleased]: https://github.com/an-tao/drogon/compare/v1.0.0-beta2...HEAD

[1.0.0-beta2]: https://github.com/an-tao/drogon/compare/v1.0.0-beta1...v1.0.0-beta2

[1.0.0-beta1]: https://github.com/an-tao/drogon/releases/tag/v1.0.0-beta1
