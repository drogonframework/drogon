# - Try to find UUID
# Once done this will define
#
# UUID_FOUND - system has UUID
# UUID_INCLUDE_DIRS - the UUID include directory
# UUID_LIBRARIES - Link these to use UUID
# UUID_DEFINITIONS - Compiler switches required for using UUID
#
# Copyright (c) 2006 Andreas Schneider <mail@cynapses.org>
#
# Redistribution and use is allowed according to the terms of the New
# BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

find_package(PkgConfig REQUIRED)

pkg_check_modules(libuuid
	REQUIRED
	IMPORTED_TARGET
	uuid
)

set(UUID_FOUND TRUE)
