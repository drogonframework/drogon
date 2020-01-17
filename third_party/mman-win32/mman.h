/*
 * sys/mman.h
 * mman-win32
 */

#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

/* All the headers include this file. */
#ifndef _MSC_VER
#include <_mingw.h>
#endif

#if defined(MMAN_LIBRARY_DLL)
/* Windows shared libraries (DLL) must be declared export when building the lib and import when building the 
application which links against the library. */
#if defined(MMAN_LIBRARY)
#define MMANSHARED_EXPORT __declspec(dllexport)
#else
#define MMANSHARED_EXPORT __declspec(dllimport)
#endif /* MMAN_LIBRARY */
#else
/* Static libraries do not require a __declspec attribute.*/
#define MMANSHARED_EXPORT
#endif /* MMAN_LIBRARY_DLL */

/* Determine offset type */
#include <stdint.h>
#if defined(_WIN64)
typedef int64_t OffsetType;
#else
typedef uint32_t OffsetType;
#endif

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROT_NONE       0
#define PROT_READ       1
#define PROT_WRITE      2
#define PROT_EXEC       4

#define MAP_FILE        0
#define MAP_SHARED      1
#define MAP_PRIVATE     2
#define MAP_TYPE        0xf
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_ANON        MAP_ANONYMOUS

#define MAP_FAILED      ((void *)-1)

/* Flags for msync. */
#define MS_ASYNC        1
#define MS_SYNC         2
#define MS_INVALIDATE   4

MMANSHARED_EXPORT void*   mmap(void *addr, size_t len, int prot, int flags, int fildes, OffsetType off);
MMANSHARED_EXPORT int     munmap(void *addr, size_t len);
MMANSHARED_EXPORT int     _mprotect(void *addr, size_t len, int prot);
MMANSHARED_EXPORT int     msync(void *addr, size_t len, int flags);
MMANSHARED_EXPORT int     mlock(const void *addr, size_t len);
MMANSHARED_EXPORT int     munlock(const void *addr, size_t len);

#ifdef __cplusplus
}
#endif

#endif /*  _SYS_MMAN_H_ */
