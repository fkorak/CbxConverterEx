# Static libraries, dynamic CRT (matches /MD of the application projects)
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

include("${CMAKE_CURRENT_LIST_DIR}/libwebp-simd.cmake")
