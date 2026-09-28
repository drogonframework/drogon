/**
 *
 *  extension.h
 *  Ahmed Sabri
 *  https://github.com/STCodePro
 *
 */

#pragma once

#include <string>

// Helper macros
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifndef DEFAULT_SOURCE_EXTENSION
#define DEFAULT_SOURCE_EXTENSION cc
#endif

#ifndef DEFAULT_HEADER_EXTENSION
#define DEFAULT_HEADER_EXTENSION h
#endif

inline std::string sourceExtension = TOSTRING(DEFAULT_SOURCE_EXTENSION);
inline std::string headerExtension = TOSTRING(DEFAULT_HEADER_EXTENSION);
