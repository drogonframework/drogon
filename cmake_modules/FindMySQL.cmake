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
# MySQL_lib - The imported target library.

# ##############################################################################

# -------------- FIND MYSQL_INCLUDE_DIRS ------------------
find_path(MARIADB_INCLUDE_DIRS
          NAMES mysql.h
          PATH_SUFFIXES mariadb
          PATHS /usr/include/mysql
                /usr/local/include/mysql
                /usr/include/mariadb
                /usr/local/include/mariadb
                /opt/mysql/mysql/include
                /opt/mysql/mysql/include/mysql
                /opt/mysql/include
                /opt/local/include/mysql5
                /usr/local/mysql/include
                /usr/local/mysql/include/mysql
                /usr/local/mariadb/include
                /usr/local/mariadb/include/mariadb
                /opt/rh/rh-mariadb105/root/usr/include
                /opt/rh/rh-mariadb105/root/usr/include/mysql
                $ENV{ProgramFiles}/MySQL/*/include
                $ENV{SystemDrive}/MySQL/*/include)

find_path(MYSQL_INCLUDE_DIRS
          NAMES mysql.h
          PATH_SUFFIXES mysql
          PATHS /usr/include/mysql
                /usr/local/include/mysql
                /usr/include/mariadb
                /usr/local/include/mariadb
                /opt/mysql/mysql/include
                /opt/mysql/mysql/include/mysql
                /opt/mysql/include
                /opt/local/include/mysql5
                /usr/local/mysql/include
                /usr/local/mysql/include/mysql
                /usr/local/mariadb/include
                /usr/local/mariadb/include/mariadb
                /opt/rh/rh-mariadb105/root/usr/include
                /opt/rh/rh-mariadb105/root/usr/include/mysql
                $ENV{ProgramFiles}/MySQL/*/include
                $ENV{SystemDrive}/MySQL/*/include)

if(EXISTS "${MARIADB_INCLUDE_DIRS}/mysql.h")
  set(MYSQL_INCLUDE_DIRS ${MARIADB_INCLUDE_DIRS})
elseif(EXISTS "${MYSQL_INCLUDE_DIRS}/mysql.h")

elseif(EXISTS "${MYSQL_INCLUDE_DIRS}/mysql/mysql.h")
  set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIRS}/mysql)
endif()

# ----------------- FIND MYSQL_LIBRARIES_DIR -------------------
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

  find_library(MYSQL_LIBRARIES
               NAMES mariadbclient
               PATHS $ENV{MYSQL_DIR}/lib/${libsuffixDist}
                     $ENV{MYSQL_DIR}/libmysql
                     $ENV{MYSQL_DIR}/libmysql/${libsuffixBuild}
                     $ENV{MYSQL_DIR}/client/${libsuffixBuild}
                     $ENV{MYSQL_DIR}/libmysql/${libsuffixBuild}
                     $ENV{ProgramFiles}/MySQL/*/lib/${libsuffixDist}
                     $ENV{SystemDrive}/MySQL/*/lib/${libsuffixDist})
else(WIN32)
  find_library(MYSQL_LIBRARIES
               NAMES mysqlclient_r mariadbclient mariadb
               PATHS /usr/lib/mysql
                     /usr/lib/mariadb
                     /usr/local/lib/mysql
                     /usr/local/lib/mariadb
                     /usr/local/mysql/lib
                     /usr/local/mysql/lib/mysql
                     /opt/local/mysql5/lib
                     /opt/local/lib/mysql5/mysql
                     /opt/mysql/mysql/lib/mysql
                     /opt/mysql/lib/mysql
                     /opt/rh/rh-mariadb105/root/usr/lib64)
endif(WIN32)

if(MYSQL_INCLUDE_DIRS AND MYSQL_LIBRARIES)
  message(STATUS "MySQL Include dir: ${MYSQL_INCLUDE_DIRS}")
  message(STATUS "MySQL client libraries: ${MYSQL_LIBRARIES}")
elseif(MySQL_FIND_REQUIRED)
  message(
    FATAL_ERROR
      "Cannot find MySQL. Include dir: ${MYSQL_INCLUDE_DIRS}  library dir: ${MYSQL_LIBRARIES_DIR}"
    )
endif(MYSQL_INCLUDE_DIRS AND MYSQL_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MySQL
                                  DEFAULT_MSG
                                  MYSQL_LIBRARIES
                                  MYSQL_INCLUDE_DIRS)
# Copy the results to the output variables.
if(MySQL_FOUND)
  add_library(MySQL_lib INTERFACE IMPORTED)
  set_target_properties(MySQL_lib
                        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                   "${MYSQL_INCLUDE_DIRS}"
                                   INTERFACE_LINK_LIBRARIES
                                   "${MYSQL_LIBRARIES}")
else(MySQL_FOUND)
  set(MYSQL_LIBRARIES)
  set(MYSQL_INCLUDE_DIRS)
endif(MySQL_FOUND)

mark_as_advanced(MYSQL_INCLUDE_DIRS MYSQL_LIBRARIES)
