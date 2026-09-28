project("XenonUtils")

add_library(XenonUtils
    "disasm.cpp" 
    "xex.cpp" 
    "image.cpp" 
    "xdbf_wrapper.cpp"
    "xex_patcher.cpp"
    "memory_mapped_file.cpp"
    "${THIRDPARTY_ROOT}/tiny-AES-c/aes.c"
)

# LZX decompression (libmspack's lzxd, LGPL-2.1). XENONUTILS_LZX_SHARED builds
# it as mspack_lzx.dll so an application can ship it as a separate, replaceable
# library; by default it is linked into XenonUtils.
option(XENONUTILS_LZX_SHARED "Build the LZX decoder as mspack_lzx.dll" OFF)
set(XENONUTILS_LZX_SOURCES
    "lzx_decompress.cpp"
    "${THIRDPARTY_ROOT}/libmspack/libmspack/mspack/lzxd.c"
)
if(XENONUTILS_LZX_SHARED)
    add_library(mspack_lzx SHARED ${XENONUTILS_LZX_SOURCES})
    target_compile_definitions(mspack_lzx
        PRIVATE XENONUTILS_LZX_BUILD NOMINMAX
        PUBLIC XENONUTILS_LZX_SHARED)
    target_include_directories(mspack_lzx
        PUBLIC .
        PRIVATE "${THIRDPARTY_ROOT}/libmspack/libmspack/mspack")
    target_link_libraries(XenonUtils PUBLIC mspack_lzx)
else()
    target_sources(XenonUtils PRIVATE ${XENONUTILS_LZX_SOURCES})
endif()

target_compile_definitions(XenonUtils
    PRIVATE
        NOMINMAX
)

target_include_directories(XenonUtils 
    PUBLIC
        .
        "${THIRDPARTY_ROOT}/simde"
    PRIVATE
        "${THIRDPARTY_ROOT}/libmspack/libmspack/mspack"
        "${THIRDPARTY_ROOT}/tiny-AES-c"
        "${THIRDPARTY_ROOT}/TinySHA1"
)

target_link_libraries(XenonUtils 
    PUBLIC
        disasm
)
