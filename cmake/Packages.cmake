include(GNUInstallDirs)

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_CONTACT "https://github.com/drogonframework/drogon")

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${DROGON_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A C++14/17 based HTTP web application framework running on Linux/macOS/Unix/Windows")

# DEB
# Figure out dependencies automatically.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# Should be set automatically, but it is not.
execute_process(COMMAND dpkg --print-architecture
  OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
  OUTPUT_STRIP_TRAILING_WHITESPACE)

# The default does not produce valid Debian package names.
set(CPACK_DEBIAN_FILE_NAME
  "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}-0_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")

# RPM
set(CPACK_RPM_PACKAGE_LICENSE "MIT")

# Figure out dependencies automatically.
set(CPACK_RPM_PACKAGE_AUTOREQ ON)

# Should be set automatically, but it is not.
execute_process(COMMAND uname -m
  OUTPUT_VARIABLE CPACK_RPM_PACKAGE_ARCHITECTURE
  OUTPUT_STRIP_TRAILING_WHITESPACE)

set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-0.${CPACK_RPM_PACKAGE_ARCHITECTURE}")

include(CPack)
