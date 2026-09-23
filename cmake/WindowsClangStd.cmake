# CMake cannot yet discover import std for Clang with Microsoft's STL. Compile the
# installed module sources with Clang so both the engine and libclang can read them.
set(CMAKE_CXX_MODULE_STD OFF)
find_path(VVE_MSVC_MODULE_DIR std.ixx
   HINTS "$ENV{VCToolsInstallDir}/modules" ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES}
   PATH_SUFFIXES "" ../modules
   REQUIRED)
add_library(vve_windows_std STATIC)
target_compile_features(vve_windows_std PUBLIC cxx_std_23)
target_sources(vve_windows_std PUBLIC FILE_SET CXX_MODULES
   BASE_DIRS "${VVE_MSVC_MODULE_DIR}"
   FILES "${VVE_MSVC_MODULE_DIR}/std.ixx" "${VVE_MSVC_MODULE_DIR}/std.compat.ixx")
target_compile_options(vve_windows_std PRIVATE
   -Wno-reserved-module-identifier -Wno-include-angled-in-module-purview)
