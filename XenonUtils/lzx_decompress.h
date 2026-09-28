#pragma once

// LZX decompression (libmspack's lzxd, LGPL-2.1). With XENONUTILS_LZX_SHARED
// it is built into mspack_lzx.dll (an application can then ship the decoder as
// a separate, replaceable library); otherwise it is linked statically.

#include <cstddef>
#include <cstdint>

#if defined(XENONUTILS_LZX_SHARED)
#if defined(XENONUTILS_LZX_BUILD)
#define XENONUTILS_LZX_API __declspec(dllexport)
#else
#define XENONUTILS_LZX_API __declspec(dllimport)
#endif
#else
#define XENONUTILS_LZX_API
#endif

XENONUTILS_LZX_API int lzxDecompress(const void* lzxData, size_t lzxLength, void* dst,
                                     size_t dstLength, uint32_t windowSize, void* windowData,
                                     size_t windowDataLength);
