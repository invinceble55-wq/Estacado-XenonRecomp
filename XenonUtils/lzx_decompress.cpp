// Referenced from: https://github.com/xenia-canary/xenia-canary/blob/canary_experimental/src/xenia/cpu/xex_module.cc

/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2023 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

// LZX decompression on libmspack's lzxd (LGPL-2.1): the memory "files" the
// decoder reads and writes, and the call. Built into mspack_lzx.dll with
// XENONUTILS_LZX_SHARED, so an application's users can replace that library.

#include "lzx_decompress.h"

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#include <intrin.h>
#endif

#include <lzx.h>
#include <mspack.h>

struct mspack_memory_file
{
    mspack_system sys;
    void *buffer;
    size_t bufferSize;
    size_t offset;
};

static mspack_memory_file *mspack_memory_open(mspack_system *sys, void *buffer, size_t bufferSize)
{
    assert(bufferSize < INT_MAX);

    if (bufferSize >= INT_MAX)
    {
        return nullptr;
    }

    mspack_memory_file *memoryFile = (mspack_memory_file *)(std::calloc(1, sizeof(mspack_memory_file)));
    if (memoryFile == nullptr)
    {
        return memoryFile;
    }

    memoryFile->buffer = buffer;
    memoryFile->bufferSize = bufferSize;
    memoryFile->offset = 0;
    return memoryFile;
}

static void mspack_memory_close(mspack_memory_file *file)
{
    std::free(file);
}

static int mspack_memory_read(mspack_file *file, void *buffer, int chars)
{
    mspack_memory_file *memoryFile = (mspack_memory_file *)(file);
    const size_t remaining = memoryFile->bufferSize - memoryFile->offset;
    const size_t total = std::min(size_t(chars), remaining);
    std::memcpy(buffer, (uint8_t *)(memoryFile->buffer) + memoryFile->offset, total);
    memoryFile->offset += total;
    return int(total);
}

static int mspack_memory_write(mspack_file *file, void *buffer, int chars)
{
    mspack_memory_file *memoryFile = (mspack_memory_file *)(file);
    const size_t remaining = memoryFile->bufferSize - memoryFile->offset;
    const size_t total = std::min(size_t(chars), remaining);
    std::memcpy((uint8_t *)(memoryFile->buffer) + memoryFile->offset, buffer, total);
    memoryFile->offset += total;
    return int(total);
}

static void *mspack_memory_alloc(mspack_system *sys, size_t chars)
{
    return std::calloc(chars, 1);
}

static void mspack_memory_free(void *ptr)
{
    std::free(ptr);
}

static void mspack_memory_copy(void *src, void *dest, size_t chars)
{
    std::memcpy(dest, src, chars);
}

static mspack_system *mspack_memory_sys_create()
{
    auto sys = (mspack_system *)(std::calloc(1, sizeof(mspack_system)));
    if (!sys)
    {
        return nullptr;
    }

    sys->read = mspack_memory_read;
    sys->write = mspack_memory_write;
    sys->alloc = mspack_memory_alloc;
    sys->free = mspack_memory_free;
    sys->copy = mspack_memory_copy;
    return sys;
}

static void mspack_memory_sys_destroy(struct mspack_system *sys)
{
    free(sys);
}

#if defined(_WIN32)
inline bool bitScanForward(uint32_t v, uint32_t *outFirstSetIndex)
{
    return _BitScanForward((unsigned long *)(outFirstSetIndex), v) != 0;
}

inline bool bitScanForward(uint64_t v, uint32_t *outFirstSetIndex)
{
    return _BitScanForward64((unsigned long *)(outFirstSetIndex), v) != 0;
}

#else
inline bool bitScanForward(uint32_t v, uint32_t *outFirstSetIndex)
{
    int i = ffs(v);
    *outFirstSetIndex = i - 1;
    return i != 0;
}

inline bool bitScanForward(uint64_t v, uint32_t *outFirstSetIndex)
{
    int i = __builtin_ffsll(v);
    *outFirstSetIndex = i - 1;
    return i != 0;
}
#endif

int lzxDecompress(const void *lzxData, size_t lzxLength, void *dst, size_t dstLength, uint32_t windowSize, void *windowData, size_t windowDataLength)
{
    int resultCode = 1;
    uint32_t windowBits;
    if (!bitScanForward(windowSize, &windowBits)) {
        return resultCode;
    }

    mspack_system *sys = mspack_memory_sys_create();
    mspack_memory_file *lzxSrc = mspack_memory_open(sys, (void *)(lzxData), lzxLength);
    mspack_memory_file *lzxDst = mspack_memory_open(sys, dst, dstLength);
    lzxd_stream *lzxd = lzxd_init(sys, (mspack_file *)(lzxSrc), (mspack_file *)(lzxDst), windowBits, 0, 0x8000, dstLength, 0);
    if (lzxd != nullptr) {
        if (windowData != nullptr) {
            size_t paddingLength = windowSize - windowDataLength;
            std::memset(&lzxd->window[0], 0, paddingLength);
            std::memcpy(&lzxd->window[paddingLength], windowData, windowDataLength);
            lzxd->ref_data_size = windowSize;
        }

        resultCode = lzxd_decompress(lzxd, dstLength);
        lzxd_free(lzxd);
    }

    if (lzxSrc) {
        mspack_memory_close(lzxSrc);
    }

    if (lzxDst) {
        mspack_memory_close(lzxDst);
    }

    if (sys) {
        mspack_memory_sys_destroy(sys);
    }

    return resultCode;
}
