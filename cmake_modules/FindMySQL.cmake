# --------------------------------------------------------
# Copyright (C) 1995-2007 MySQL AB
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of version 2 of the GNU General Public License as published by the
# Free Software Foundation.
#
# There are special exceptions to the terms and conditions of the GPL as it is
# applied to this software. View the full text of the exception in file
# LICENSE.exceptions in the top-level directory of this software distribution.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA  02110-1301, USA
#
# The MySQL Connector/ODBC is licensed under the terms of the GPL, like most
# MySQL Connectors. There are special exceptions to the terms and conditions of
# the GPL as it is applied to this software, see the FLOSS License Exception
# available on mysql.com.

# ##############################################################################

# -------------- FIND MYSQL_INCLUDE_DIR ------------------
find_path(MYSQL_INCLUDE_DIR
          NAMES mysql.h
          PATH_SUFFIXES mysql
          PATHS /usr/include/mysql
                /usr/local/include/mysql
                /opt/mysql/mysql/include
                /opt/mysql/mysql/include/mysql
                /opt/mysql/include
                /opt/local/include/mysql5
                /usr/local/mysql/include
                /usr/local/mysql/include/mysql
                $ENV{ProgramFiles}/MySQL/*/include
                $ENV{SystemDrive}/MySQL/*/include)

if(EXISTS "${MYSQL_INCLUDE_DIR}/mysql.h")

elseif(EXISTS "${MYSQL_INCLUDE_DIR}/mysql/mysql.h")
  set(MYSQL_INCLUDE_DIR ${MYSQL_INCLUDE_DIR}/mysql)
endif()

# ----------------- FIND MYSQL_LIB_DIR -------------------
if(WIN32)
  # Set lib path suffixes dist = for mysql binary distributions build = for
  # custom built tree
  if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(libsuffixDist debug)
    set(libsuffixBuild Debug)
  else(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(libsuffixDist opt)
    set(libsuffixBuild Release)
    add_definitions(-DDBUG_OFF)
  endif(CMAKE_BUILD_TYPE STREQUAL Debug)

  find_library(MYSQL_LIB
               NAMES mariadbclient
               PATHS $ENV{MYSQL_DIR}/lib/${libsuffixDist}
                     $ENV{MYSQL_DIR}/libmysql
                     $ENV{MYSQL_DIR}/libmysql/${libsuffixBuild}
                     $ENV{MYSQL_DIR}/client/${libsuffixBuild}
                     $ENV{MYSQL_DIR}/libmysql/${libsuffixBuild}
                     $ENV{ProgramFiles}/MySQL/*/lib/${libsuffixDist}
                     $ENV{SystemDrive}/MySQL/*/lib/${libsuffixDist})
else(WIN32)
  find_library(MYSQL_LIB
               NAMES mysqlclient_r mariadbclient
               PATHS /usr/lib/mysql
                     /usr/local/lib/mysql
                     /usr/local/mysql/lib
                     /usr/local/mysql/lib/mysql
                     /opt/local/mysql5/lib
                     /opt/local/lib/mysql5/mysql
                     /opt/mysql/mysql/lib/mysql
                     /opt/mysql/lib/mysql)
endif(WIN32)

if(MYSQL_LIB)
  get_filename_component(MYSQL_LIB_DIR ${MYSQL_LIB} PATH)
endif(MYSQL_LIB)

if(MYSQL_INCLUDE_DIR AND MYSQL_LIB_DIR)
  set(MYSQL_FOUND TRUE)

  # FIND_LIBRARY(MYSQL_ZLIB zlib PATHS ${MYSQL_LIB_DIR})
  # FIND_LIBRARY(MYSQL_TAOCRYPT taocrypt PATHS ${MYSQL_LIB_DIR})
  set(MYSQL_CLIENT_LIBS ${MYSQL_LIB})
  # IF (MYSQL_ZLIB) SET(MYSQL_CLIENT_LIBS ${MYSQL_CLIENT_LIBS} zlib) ENDIF
  # (MYSQL_ZLIB) IF (MYSQL_TAOCRYPT) SET(MYSQL_CLIENT_LIBS ${MYSQL_CLIENT_LIBS}
  # taocrypt) ENDIF (MYSQL_TAOCRYPT) Added needed mysqlclient dependencies on
  # Windows
  if(MSVC)
    set(MYSQL_CLIENT_LIBS ${MYSQL_CLIENT_LIBS} shlwapi)
  endif(MSVC)

  message(
    STATUS
      "MySQL Include dir: ${MYSQL_INCLUDE_DIR}  library dir: ${MYSQL_LIB_DIR}")
  message(STATUS "MySQL client libraries: ${MYSQL_CLIENT_LIBS}")
elseif(MySQL_FIND_REQUIRED)
  message(
    FATAL_ERROR
      "Cannot find MySQL. Include dir: ${MYSQL_INCLUDE_DIR}  library dir: ${MYSQL_LIB_DIR}"
    )
endif(MYSQL_INCLUDE_DIR AND MYSQL_LIB_DIR)
