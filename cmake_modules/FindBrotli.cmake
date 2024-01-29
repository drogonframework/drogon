# ***************************************************************************
#                                  _   _ ____  _
#  Project                     ___| | | |  _ \| |
#                             / __| | | | |_) | |
#                            | (__| |_| |  _ <| |___
#                             \___|\___/|_| \_\_____|
#
# Copyright (C) 1998 - 2020, Daniel Stenberg, <daniel@haxx.se>, et al.
#
# This software is licensed as described in the file COPYING, which you should
# have received as part of this distribution. The terms are also available at
# https://curl.haxx.se/docs/copyright.html.
#
# You may opt to use, copy, modify, merge, publish, distribute and/or sell
# copies of the Software, and permit persons to whom the Software is furnished
# to do so, under the terms of the COPYING file.
#
# This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# KIND, either express or implied.
#
# ##############################################################################
include(FindPackageHandleStandardArgs)

find_path(BROTLI_INCLUDE_DIR "brotli/decode.h")

find_library(BROTLICOMMON_LIBRARY NAMES brotlicommon brotlicommon-static)
find_library(BROTLIDEC_LIBRARY NAMES brotlidec brotlidec-static)
find_library(BROTLIENC_LIBRARY NAMES brotlienc brotlienc-static)

find_package_handle_standard_args(Brotli
                                  REQUIRED_VARS
                                  BROTLIDEC_LIBRARY
                                  BROTLIENC_LIBRARY
                                  BROTLICOMMON_LIBRARY
                                  BROTLI_INCLUDE_DIR
                                  FAIL_MESSAGE
                                  "Could NOT find BROTLI")

set(BROTLI_INCLUDE_DIRS ${BROTLI_INCLUDE_DIR})
set(BROTLI_LIBRARIES ${BROTLIDEC_LIBRARY}
                     ${BROTLIENC_LIBRARY} ${BROTLICOMMON_LIBRARY})

if(Brotli_FOUND)
  add_library(Brotli_lib INTERFACE IMPORTED)
  set_target_properties(Brotli_lib
                        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                   "${BROTLI_INCLUDE_DIRS}"
                                   INTERFACE_LINK_LIBRARIES
                                   "${BROTLI_LIBRARIES}")
endif(Brotli_FOUND)
