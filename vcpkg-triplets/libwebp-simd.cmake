# libwebp's CMake build enables its SSE2 / SSE4.1 code only if __SSE2__ / __SSE4_1__
# are defined (src/dsp/cpu.h ignores its MSVC detection when HAVE_CONFIG_H is set),
# but MSVC never defines these macros. Without them every SIMD function compiles to
# a stub and WebP encoding is about 2x slower. Google's own MSVC builds (Makefile.vc,
# official cwebp.exe) enable the same code paths. SSE4.1 functions are still selected
# at runtime based on CPU features.
if(PORT STREQUAL "libwebp")
    set(VCPKG_C_FLAGS "/D__SSE2__ /D__SSE4_1__")
    set(VCPKG_CXX_FLAGS "/D__SSE2__ /D__SSE4_1__")
endif()
