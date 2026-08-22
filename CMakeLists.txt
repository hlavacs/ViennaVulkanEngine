cmake_minimum_required(VERSION 3.31.8)

# CMake discovers the compiler-provided `import std` target while enabling CXX.
if(CMAKE_VERSION VERSION_LESS "4.0")
   set(VVE_CMAKE_IMPORT_STD_UUID "d0edc3af-4c50-42ea-a356-e2862fe7a444")
elseif(CMAKE_VERSION VERSION_LESS "4.0.3")
   set(VVE_CMAKE_IMPORT_STD_UUID "a9e1cf81-9932-4810-974b-6eccaf14e457")
elseif(CMAKE_VERSION VERSION_LESS "4.3")
   set(VVE_CMAKE_IMPORT_STD_UUID "d0edc3af-4c50-42ea-a356-e2862fe7a444")
elseif(CMAKE_VERSION VERSION_LESS "4.4")
   set(VVE_CMAKE_IMPORT_STD_UUID "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
else()
   set(VVE_CMAKE_IMPORT_STD_UUID "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
endif()
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "${VVE_CMAKE_IMPORT_STD_UUID}" CACHE STRING "CMake experimental import std gate" FORCE)
set(CMAKE_CXX_MODULE_STD ON CACHE BOOL "Enable C++ standard library module support" FORCE)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED VVE_VCPKG_TRIPLET OR VVE_VCPKG_TRIPLET STREQUAL "")
   if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
      set(VVE_VCPKG_TRIPLET_DEFAULT "x64-windows")
   elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
      set(VVE_VCPKG_TRIPLET_DEFAULT "x64-linux")
   elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
      set(VVE_VCPKG_TRIPLET_DEFAULT "arm64-osx")
   else()
      message(FATAL_ERROR "Unsupported host platform '${CMAKE_HOST_SYSTEM_NAME}' for default vcpkg triplet selection.")
   endif()

   set(VVE_VCPKG_TRIPLET
      "${VVE_VCPKG_TRIPLET_DEFAULT}"
      CACHE STRING "Vcpkg triplet used for local manifest installs")
endif()

set(VVE_VCPKG_INSTALLED_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VVE_VCPKG_TRIPLET}")
if(EXISTS "${VVE_VCPKG_INSTALLED_DIR}")
   list(PREPEND CMAKE_PREFIX_PATH "${VVE_VCPKG_INSTALLED_DIR}")
   if(EXISTS "${VVE_VCPKG_INSTALLED_DIR}/share/stb/FindStb.cmake")
      list(PREPEND CMAKE_MODULE_PATH "${VVE_VCPKG_INSTALLED_DIR}/share/stb")
   endif()
endif()

if(CMAKE_GENERATOR MATCHES "^Visual Studio")
   if(DEFINED CMAKE_VS_GLOBALS AND NOT CMAKE_VS_GLOBALS STREQUAL "")
      set(CMAKE_VS_GLOBALS "${CMAKE_VS_GLOBALS};VcpkgApplocalDeps=false")
   else()
      set(CMAKE_VS_GLOBALS "VcpkgApplocalDeps=false")
   endif()
endif()

project(ViennaVulkanEngine VERSION 4.0.0 LANGUAGES CXX)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
   include(CTest)
endif()

add_compile_options(
   "$<$<CXX_COMPILER_ID:MSVC>:/MP>"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
   add_compile_options(-Wno-reserved-module-identifier)
endif()

find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
   message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE}")
else()
   message(STATUS "Doxygen not found. The 'docs' target will be unavailable.")
endif()

set(VVE_VULKAN_SDK_ROOT "" CACHE PATH "Root directory of the Vulkan SDK used for Vulkan and Slang packages")
if(VVE_VULKAN_SDK_ROOT STREQUAL "")
   if(DEFINED ENV{VULKAN_SDK} AND NOT "$ENV{VULKAN_SDK}" STREQUAL "")
      set(VVE_VULKAN_SDK_ROOT "$ENV{VULKAN_SDK}"
         CACHE PATH "Root directory of the Vulkan SDK used for Vulkan and Slang packages" FORCE)
   elseif(APPLE AND DEFINED ENV{HOME})
      file(GLOB VVE_VULKAN_SDK_CANDIDATES LIST_DIRECTORIES true "$ENV{HOME}/VulkanSDK/*/macOS")
      if(VVE_VULKAN_SDK_CANDIDATES)
         list(SORT VVE_VULKAN_SDK_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
         list(GET VVE_VULKAN_SDK_CANDIDATES 0 VVE_VULKAN_SDK_DETECTED_ROOT)
         set(VVE_VULKAN_SDK_ROOT "${VVE_VULKAN_SDK_DETECTED_ROOT}"
            CACHE PATH "Root directory of the Vulkan SDK used for Vulkan and Slang packages" FORCE)
      endif()
   endif()
endif()

if(NOT VVE_VULKAN_SDK_ROOT STREQUAL "")
   list(PREPEND CMAKE_PREFIX_PATH "${VVE_VULKAN_SDK_ROOT}" "${VVE_VULKAN_SDK_ROOT}/cmake")
   list(PREPEND CMAKE_INCLUDE_PATH "${VVE_VULKAN_SDK_ROOT}/include")
   list(PREPEND CMAKE_LIBRARY_PATH "${VVE_VULKAN_SDK_ROOT}/lib" "${VVE_VULKAN_SDK_ROOT}/Lib")

   if(EXISTS "${VVE_VULKAN_SDK_ROOT}/include/vulkan/vulkan.h")
      set(Vulkan_INCLUDE_DIR "${VVE_VULKAN_SDK_ROOT}/include" CACHE PATH "Vulkan SDK include directory" FORCE)
   endif()

   if(WIN32 AND EXISTS "${VVE_VULKAN_SDK_ROOT}/Lib/vulkan-1.lib")
      set(Vulkan_LIBRARY "${VVE_VULKAN_SDK_ROOT}/Lib/vulkan-1.lib" CACHE FILEPATH "Vulkan SDK loader library" FORCE)
   elseif(APPLE AND EXISTS "${VVE_VULKAN_SDK_ROOT}/lib/libvulkan.dylib")
      set(Vulkan_LIBRARY "${VVE_VULKAN_SDK_ROOT}/lib/libvulkan.dylib" CACHE FILEPATH "Vulkan SDK loader library" FORCE)
   elseif(UNIX AND EXISTS "${VVE_VULKAN_SDK_ROOT}/lib/libvulkan.so")
      set(Vulkan_LIBRARY "${VVE_VULKAN_SDK_ROOT}/lib/libvulkan.so" CACHE FILEPATH "Vulkan SDK loader library" FORCE)
   endif()
endif()

set(VVE_GENERATED_MOLTENVK_ICD_MANIFEST "")
set(VVE_GENERATED_KOSMICKRISP_ICD_MANIFEST "")
if(APPLE AND NOT VVE_VULKAN_SDK_ROOT STREQUAL "")
   set(VVE_GENERATED_VULKAN_ICD_DIR "${CMAKE_BINARY_DIR}/vulkan/icd.d")
   file(MAKE_DIRECTORY "${VVE_GENERATED_VULKAN_ICD_DIR}")

   set(VVE_MOLTENVK_LIBRARY "${VVE_VULKAN_SDK_ROOT}/lib/libMoltenVK.dylib")
   if(EXISTS "${VVE_MOLTENVK_LIBRARY}")
      set(VVE_GENERATED_MOLTENVK_ICD_MANIFEST "${VVE_GENERATED_VULKAN_ICD_DIR}/MoltenVK_icd.json")
      file(WRITE "${VVE_GENERATED_MOLTENVK_ICD_MANIFEST}"
         "{\n"
         "  \"file_format_version\": \"1.0.0\",\n"
         "  \"ICD\": {\n"
         "    \"library_path\": \"${VVE_MOLTENVK_LIBRARY}\",\n"
         "    \"api_version\": \"1.4.0\",\n"
         "    \"is_portability_driver\": true\n"
         "  }\n"
         "}\n"
      )
   endif()

   set(VVE_KOSMICKRISP_LIBRARY "${VVE_VULKAN_SDK_ROOT}/lib/libvulkan_kosmickrisp.dylib")
   if(EXISTS "${VVE_KOSMICKRISP_LIBRARY}")
      set(VVE_GENERATED_KOSMICKRISP_ICD_MANIFEST
         "${VVE_GENERATED_VULKAN_ICD_DIR}/libkosmickrisp_icd.json")
      file(WRITE "${VVE_GENERATED_KOSMICKRISP_ICD_MANIFEST}"
         "{\n"
         "  \"file_format_version\": \"1.0.0\",\n"
         "  \"ICD\": {\n"
         "    \"library_path\": \"${VVE_KOSMICKRISP_LIBRARY}\",\n"
         "    \"api_version\": \"1.3.0\"\n"
         "  }\n"
         "}\n"
      )
   endif()
endif()

if(EXISTS "${VVE_VCPKG_INSTALLED_DIR}/share/sdl3/SDL3Config.cmake")
   set(SDL3_DIR "${VVE_VCPKG_INSTALLED_DIR}/share/sdl3"
      CACHE PATH "SDL3 package configuration directory from vcpkg_installed" FORCE)
elseif(EXISTS "${VVE_VCPKG_INSTALLED_DIR}/share/SDL3/SDL3Config.cmake")
   set(SDL3_DIR "${VVE_VCPKG_INSTALLED_DIR}/share/SDL3"
      CACHE PATH "SDL3 package configuration directory from vcpkg_installed" FORCE)
endif()

find_package(Vulkan REQUIRED)
find_package(slang CONFIG QUIET)
if(TARGET slang::slang AND NOT VVE_VULKAN_SDK_ROOT STREQUAL "" AND EXISTS "${VVE_VULKAN_SDK_ROOT}/include/slang/slang.h")
   get_target_property(VVE_SLANG_IMPORTED_INCLUDES slang::slang INTERFACE_INCLUDE_DIRECTORIES)
   set(VVE_SLANG_FIXED_INCLUDES)
   foreach(VVE_SLANG_INCLUDE IN LISTS VVE_SLANG_IMPORTED_INCLUDES)
      if(EXISTS "${VVE_SLANG_INCLUDE}")
         list(APPEND VVE_SLANG_FIXED_INCLUDES "${VVE_SLANG_INCLUDE}")
      endif()
   endforeach()
   list(APPEND VVE_SLANG_FIXED_INCLUDES "${VVE_VULKAN_SDK_ROOT}/include")
   list(REMOVE_DUPLICATES VVE_SLANG_FIXED_INCLUDES)
   set_target_properties(slang::slang PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${VVE_SLANG_FIXED_INCLUDES}")
endif()

set(VVE_SLANG_RUNTIME_DLLS_RELEASE)
set(VVE_SLANG_RUNTIME_DLLS_DEBUG)
set(VVE_SLANG_RUNTIME_DLLS)

if(NOT TARGET slang::slang)
   find_path(VVE_SLANG_INCLUDE_DIR
      NAMES slang/slang.h
      HINTS
         "${VVE_VULKAN_SDK_ROOT}/include"
         "${VVE_VULKAN_SDK_ROOT}/Include"
   )

   if(WIN32)
      find_library(VVE_SLANG_LIBRARY_RELEASE
         NAMES slang
         HINTS
            "${VVE_VULKAN_SDK_ROOT}/Lib"
            "${VVE_VULKAN_SDK_ROOT}/lib"
      )
      find_library(VVE_SLANG_LIBRARY_DEBUG
         NAMES slangd slang
         HINTS
            "${VVE_VULKAN_SDK_ROOT}/Lib"
            "${VVE_VULKAN_SDK_ROOT}/lib"
      )
      find_file(VVE_SLANG_RUNTIME_RELEASE
         NAMES slang.dll
         HINTS
            "${VVE_VULKAN_SDK_ROOT}/Bin"
            "${VVE_VULKAN_SDK_ROOT}/bin"
      )
      find_file(VVE_SLANG_RUNTIME_DEBUG
         NAMES slangd.dll slang.dll
         HINTS
            "${VVE_VULKAN_SDK_ROOT}/Bin"
            "${VVE_VULKAN_SDK_ROOT}/bin"
      )
   else()
      find_library(VVE_SLANG_LIBRARY_RELEASE
         NAMES slang
         HINTS
            "${VVE_VULKAN_SDK_ROOT}/lib"
            "${VVE_VULKAN_SDK_ROOT}/Lib"
      )
      set(VVE_SLANG_LIBRARY_DEBUG "${VVE_SLANG_LIBRARY_RELEASE}")
      set(VVE_SLANG_RUNTIME_RELEASE "${VVE_SLANG_LIBRARY_RELEASE}")
      set(VVE_SLANG_RUNTIME_DEBUG "${VVE_SLANG_LIBRARY_RELEASE}")
   endif()

   if(VVE_SLANG_INCLUDE_DIR AND VVE_SLANG_LIBRARY_RELEASE AND VVE_SLANG_RUNTIME_RELEASE)
      add_library(slang::slang SHARED IMPORTED GLOBAL)
      set_target_properties(slang::slang PROPERTIES
         INTERFACE_INCLUDE_DIRECTORIES "${VVE_SLANG_INCLUDE_DIR}"
         IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo;MinSizeRel"
      )

      if(WIN32)
         set_target_properties(slang::slang PROPERTIES
            IMPORTED_IMPLIB_DEBUG "${VVE_SLANG_LIBRARY_DEBUG}"
            IMPORTED_IMPLIB_RELEASE "${VVE_SLANG_LIBRARY_RELEASE}"
            IMPORTED_IMPLIB_RELWITHDEBINFO "${VVE_SLANG_LIBRARY_RELEASE}"
            IMPORTED_IMPLIB_MINSIZEREL "${VVE_SLANG_LIBRARY_RELEASE}"
            IMPORTED_LOCATION_DEBUG "${VVE_SLANG_RUNTIME_DEBUG}"
            IMPORTED_LOCATION_RELEASE "${VVE_SLANG_RUNTIME_RELEASE}"
            IMPORTED_LOCATION_RELWITHDEBINFO "${VVE_SLANG_RUNTIME_RELEASE}"
            IMPORTED_LOCATION_MINSIZEREL "${VVE_SLANG_RUNTIME_RELEASE}"
         )

         if(EXISTS "${VVE_VULKAN_SDK_ROOT}/Bin")
            set(VVE_SLANG_RUNTIME_BIN_DIR "${VVE_VULKAN_SDK_ROOT}/Bin")
         else()
            set(VVE_SLANG_RUNTIME_BIN_DIR "${VVE_VULKAN_SDK_ROOT}/bin")
         endif()

         file(GLOB VVE_SLANG_RUNTIME_DLLS CONFIGURE_DEPENDS
            "${VVE_SLANG_RUNTIME_BIN_DIR}/slang*.dll"
         )
         list(APPEND VVE_SLANG_RUNTIME_DLLS_DEBUG ${VVE_SLANG_RUNTIME_DLLS})
         list(APPEND VVE_SLANG_RUNTIME_DLLS_RELEASE ${VVE_SLANG_RUNTIME_DLLS})
      else()
         set_target_properties(slang::slang PROPERTIES
            IMPORTED_LOCATION_DEBUG "${VVE_SLANG_RUNTIME_DEBUG}"
            IMPORTED_LOCATION_RELEASE "${VVE_SLANG_RUNTIME_RELEASE}"
            IMPORTED_LOCATION_RELWITHDEBINFO "${VVE_SLANG_RUNTIME_RELEASE}"
            IMPORTED_LOCATION_MINSIZEREL "${VVE_SLANG_RUNTIME_RELEASE}"
         )
      endif()

      message(STATUS "Using Slang from Vulkan SDK fallback: ${VVE_VULKAN_SDK_ROOT}")
   else()
      message(FATAL_ERROR
         "Could not find Slang via CMake package config or Vulkan SDK fallback. "
         "Set slang_DIR to a directory containing slangConfig.cmake, or set VVE_VULKAN_SDK_ROOT/VULKAN_SDK "
         "to a Vulkan SDK installation that provides the Slang headers and libraries."
      )
   endif()
endif()

find_package(SDL3 CONFIG REQUIRED)

set(VVE_VCPKG_RUNTIME_DLLS_RELEASE)
if(WIN32 AND EXISTS "${VVE_VCPKG_INSTALLED_DIR}/bin")
   file(GLOB VVE_VCPKG_RUNTIME_DLLS_RELEASE CONFIGURE_DEPENDS
      "${VVE_VCPKG_INSTALLED_DIR}/bin/*.dll"
   )
endif()
list(APPEND VVE_VCPKG_RUNTIME_DLLS_RELEASE ${VVE_SLANG_RUNTIME_DLLS_RELEASE})

set(VVE_VCPKG_RUNTIME_DLLS_DEBUG)
if(WIN32 AND EXISTS "${VVE_VCPKG_INSTALLED_DIR}/debug/bin")
   file(GLOB VVE_VCPKG_RUNTIME_DLLS_DEBUG CONFIGURE_DEPENDS
      "${VVE_VCPKG_INSTALLED_DIR}/debug/bin/*.dll"
   )
endif()
list(APPEND VVE_VCPKG_RUNTIME_DLLS_DEBUG ${VVE_SLANG_RUNTIME_DLLS_DEBUG})

set(VVE_WINDOWS_RUNTIME_DLLS)
if(WIN32)
   list(APPEND VVE_WINDOWS_RUNTIME_DLLS
      ${VVE_VCPKG_RUNTIME_DLLS_RELEASE}
      ${VVE_VCPKG_RUNTIME_DLLS_DEBUG}
      ${VVE_SLANG_RUNTIME_DLLS}
   )
   list(REMOVE_DUPLICATES VVE_WINDOWS_RUNTIME_DLLS)
endif()

set(VVE_OUTPUT_CONFIG_DIR "$<LOWER_CASE:$<CONFIG>>")
set(VVE_ARCHIVE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/${VVE_OUTPUT_CONFIG_DIR}/lib")
set(VVE_LIBRARY_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/${VVE_OUTPUT_CONFIG_DIR}/lib")
set(VVE_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/${VVE_OUTPUT_CONFIG_DIR}/dll")
set(VVE_PROJECT_CONFIG_DIR "${VVE_OUTPUT_CONFIG_DIR}")
set(VVE_PROJECT_ARCHIVE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${VVE_PROJECT_CONFIG_DIR}/lib")
set(VVE_PROJECT_LIBRARY_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${VVE_PROJECT_CONFIG_DIR}/lib")
set(VVE_PROJECT_RUNTIME_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${VVE_PROJECT_CONFIG_DIR}/exe")
set(VVE_PROJECT_MODULE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${VVE_PROJECT_CONFIG_DIR}/dll")
set(VVE_RUNTIME_OUTPUT_DIR "${VVE_PROJECT_RUNTIME_OUTPUT_DIR}")

if(WIN32 AND VVE_WINDOWS_RUNTIME_DLLS)
   add_custom_target(vve_copy_runtime_dlls
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${VVE_RUNTIME_OUTPUT_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${VVE_PROJECT_RUNTIME_OUTPUT_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
         ${VVE_WINDOWS_RUNTIME_DLLS}
         "${VVE_RUNTIME_OUTPUT_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
         ${VVE_WINDOWS_RUNTIME_DLLS}
         "${VVE_PROJECT_RUNTIME_OUTPUT_DIR}"
      COMMAND_EXPAND_LISTS
   )
endif()

function(vve_mirror_target_file target_name destination_dir)
   if(NOT VVE_RUNTIME_OUTPUT_DIR STREQUAL destination_dir)
      add_custom_command(TARGET "${target_name}" POST_BUILD
         COMMAND "${CMAKE_COMMAND}" -E make_directory "${destination_dir}"
         COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${target_name}>"
            "${destination_dir}"
         COMMAND_EXPAND_LISTS
      )
   endif()
endfunction()

function(vve_set_output_dirs target_name)
   get_target_property(target_type "${target_name}" TYPE)

   set_target_properties("${target_name}" PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY "${VVE_ARCHIVE_OUTPUT_DIR}"
   )

   if(target_type STREQUAL "EXECUTABLE")
      set_target_properties("${target_name}" PROPERTIES
         RUNTIME_OUTPUT_DIRECTORY "${VVE_RUNTIME_OUTPUT_DIR}"
      )
      if(APPLE)
         set(vve_executable_rpath "@loader_path/../lib")
         if(NOT VVE_VULKAN_SDK_ROOT STREQUAL "")
            list(APPEND vve_executable_rpath "${VVE_VULKAN_SDK_ROOT}/lib")
         endif()
         set_target_properties("${target_name}" PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "${vve_executable_rpath}"
         )
      elseif(UNIX)
         set_target_properties("${target_name}" PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "$ORIGIN/../lib"
         )
      endif()
      vve_mirror_target_file("${target_name}" "${VVE_PROJECT_RUNTIME_OUTPUT_DIR}")

      if(WIN32)
         if(TARGET vve_copy_runtime_dlls)
            add_dependencies("${target_name}" vve_copy_runtime_dlls)
         endif()
         add_custom_command(TARGET "${target_name}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
               $<TARGET_RUNTIME_DLLS:${target_name}>
               $<TARGET_FILE_DIR:${target_name}>
            COMMAND_EXPAND_LISTS
         )
      endif()
   elseif(target_type STREQUAL "SHARED_LIBRARY")
      if(WIN32)
         set_target_properties("${target_name}" PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${VVE_RUNTIME_OUTPUT_DIR}"
         )
         vve_mirror_target_file("${target_name}" "${VVE_PROJECT_RUNTIME_OUTPUT_DIR}")
         if(TARGET vve_copy_runtime_dlls)
            add_dependencies("${target_name}" vve_copy_runtime_dlls)
         endif()
         add_custom_command(TARGET "${target_name}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
               $<TARGET_RUNTIME_DLLS:${target_name}>
               $<TARGET_FILE_DIR:${target_name}>
            COMMAND_EXPAND_LISTS
         )
      else()
         set_target_properties("${target_name}" PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${VVE_LIBRARY_OUTPUT_DIR}"
         )
         vve_mirror_target_file("${target_name}" "${VVE_PROJECT_LIBRARY_OUTPUT_DIR}")
      endif()
   elseif(target_type STREQUAL "MODULE_LIBRARY")
      set_target_properties("${target_name}" PROPERTIES
         LIBRARY_OUTPUT_DIRECTORY "${VVE_MODULE_OUTPUT_DIR}"
      )
      vve_mirror_target_file("${target_name}" "${VVE_PROJECT_MODULE_OUTPUT_DIR}")
   endif()
endfunction()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
   add_subdirectory(src)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
   add_subdirectory(examples)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
   add_subdirectory(tests)
   file(WRITE "${CMAKE_BINARY_DIR}/CTestTestfile.cmake" "subdirs(\"tests\")\n")
endif()

if(DOXYGEN_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in")
   set(VVE_DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/docs/build")
   set(VVE_DOXYGEN_INPUT
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
      "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
   )
   string(REPLACE ";" " " VVE_DOXYGEN_INPUT "${VVE_DOXYGEN_INPUT}")

   configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in"
      "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
      @ONLY
   )

   add_custom_target(docs
      COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      COMMENT "Generating API documentation with Doxygen"
      VERBATIM
   )

   message(STATUS "Added CMake target: docs")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in")
   message(STATUS "Doxygen template found, but 'docs' target was not added because Doxygen is unavailable.")
endif()
