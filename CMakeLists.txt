cmake_minimum_required(VERSION 3.28)

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

project(ViennaVulkanEngine VERSION 3.0.0 LANGUAGES CXX)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
   include(CTest)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

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
      set(VVE_VULKAN_SDK_ROOT "$ENV{VULKAN_SDK}" CACHE PATH "Root directory of the Vulkan SDK used for Vulkan and Slang packages" FORCE)
   elseif(APPLE AND DEFINED ENV{HOME})
      file(GLOB VVE_VULKAN_SDK_CANDIDATES LIST_DIRECTORIES true "$ENV{HOME}/VulkanSDK/*/macOS")
      if(VVE_VULKAN_SDK_CANDIDATES)
         list(SORT VVE_VULKAN_SDK_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
         list(GET VVE_VULKAN_SDK_CANDIDATES 0 VVE_VULKAN_SDK_DETECTED_ROOT)
         set(VVE_VULKAN_SDK_ROOT "${VVE_VULKAN_SDK_DETECTED_ROOT}" CACHE PATH "Root directory of the Vulkan SDK used for Vulkan and Slang packages" FORCE)
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

if(EXISTS "${VVE_VCPKG_INSTALLED_DIR}/share/sdl3/SDL3Config.cmake")
   set(SDL3_DIR "${VVE_VCPKG_INSTALLED_DIR}/share/sdl3" CACHE PATH "SDL3 package configuration directory from vcpkg_installed" FORCE)
elseif(EXISTS "${VVE_VCPKG_INSTALLED_DIR}/share/SDL3/SDL3Config.cmake")
   set(SDL3_DIR "${VVE_VCPKG_INSTALLED_DIR}/share/SDL3" CACHE PATH "SDL3 package configuration directory from vcpkg_installed" FORCE)
endif()

find_package(Vulkan REQUIRED)
find_package(slang CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

set(VVE_VCPKG_RUNTIME_DLLS_RELEASE)
if(WIN32 AND EXISTS "${VVE_VCPKG_INSTALLED_DIR}/bin")
   file(GLOB VVE_VCPKG_RUNTIME_DLLS_RELEASE CONFIGURE_DEPENDS
      "${VVE_VCPKG_INSTALLED_DIR}/bin/*.dll"
   )
endif()

set(VVE_VCPKG_RUNTIME_DLLS_DEBUG)
if(WIN32 AND EXISTS "${VVE_VCPKG_INSTALLED_DIR}/debug/bin")
   file(GLOB VVE_VCPKG_RUNTIME_DLLS_DEBUG CONFIGURE_DEPENDS
      "${VVE_VCPKG_INSTALLED_DIR}/debug/bin/*.dll"
   )
endif()

if(WIN32 AND (VVE_VCPKG_RUNTIME_DLLS_RELEASE OR VVE_VCPKG_RUNTIME_DLLS_DEBUG))
   add_custom_target(vve_copy_runtime_dlls
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_SOURCE_DIR}/bin/exe"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
         $<$<CONFIG:Debug>:${VVE_VCPKG_RUNTIME_DLLS_DEBUG}>
         $<$<NOT:$<CONFIG:Debug>>:${VVE_VCPKG_RUNTIME_DLLS_RELEASE}>
         "${CMAKE_SOURCE_DIR}/bin/exe"
      COMMAND_EXPAND_LISTS
   )
endif()

function(vve_set_output_dirs target_name)
   get_target_property(target_type "${target_name}" TYPE)

   set_target_properties("${target_name}" PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/lib"
   )
   foreach(config ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${config}" config_upper)
      set_target_properties("${target_name}" PROPERTIES
         "ARCHIVE_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_SOURCE_DIR}/bin/lib"
      )
   endforeach()

   if(target_type STREQUAL "EXECUTABLE")
      set_target_properties("${target_name}" PROPERTIES
         RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/exe"
      )
      foreach(config ${CMAKE_CONFIGURATION_TYPES})
         string(TOUPPER "${config}" config_upper)
         set_target_properties("${target_name}" PROPERTIES
            "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_SOURCE_DIR}/bin/exe"
         )
      endforeach()

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
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/exe"
         )
         foreach(config ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER "${config}" config_upper)
            set_target_properties("${target_name}" PROPERTIES
               "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_SOURCE_DIR}/bin/exe"
            )
         endforeach()
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
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/lib"
         )
         foreach(config ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER "${config}" config_upper)
            set_target_properties("${target_name}" PROPERTIES
               "LIBRARY_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_SOURCE_DIR}/bin/lib"
            )
         endforeach()
      endif()
   elseif(target_type STREQUAL "MODULE_LIBRARY")
      set_target_properties("${target_name}" PROPERTIES
         LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/dll"
      )
      foreach(config ${CMAKE_CONFIGURATION_TYPES})
         string(TOUPPER "${config}" config_upper)
         set_target_properties("${target_name}" PROPERTIES
            "LIBRARY_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_SOURCE_DIR}/bin/dll"
         )
      endforeach()
   endif()
endfunction()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
   add_subdirectory(src)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
   add_subdirectory(examples)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
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
