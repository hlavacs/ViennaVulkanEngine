add_library(ViennaVulkanEngineSimple STATIC EXCLUDE_FROM_ALL)

target_compile_features(ViennaVulkanEngineSimple PUBLIC cxx_std_23)

target_link_libraries(ViennaVulkanEngineSimple
   PUBLIC
      ViennaVulkanEngine
      Vulkan::Vulkan # Static-lib Vulkan calls must resolve at the executable link.
)

target_sources(ViennaVulkanEngineSimple
   PRIVATE
      stb_image_write_impl.cpp
   PUBLIC
      FILE_SET CXX_MODULES FILES
         Assets.ixx
         ECS.ixx
         Engine.ixx
         GUI.ixx
         Graph.ixx
         Handle.ixx
         Math.ixx
         Mesh.ixx
         Vulkan.ixx
         Scene.ixx
         Renderer.ixx
         SceneGraph.ixx
         Vector.ixx
         Window.ixx
         WindowSystem.ixx
         World.ixx
)

set(VVE_SIMPLE_FORWARD_VERTEX_SHADER "${CMAKE_CURRENT_LIST_DIR}/shaders/simple_forward.vert.slang")
set(VVE_SIMPLE_FORWARD_FRAGMENT_SHADER "${CMAKE_CURRENT_LIST_DIR}/shaders/simple_forward.frag.slang")
set(VVE_SIMPLE_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(VVE_SIMPLE_FORWARD_VERTEX_SPV "${VVE_SIMPLE_SHADER_OUTPUT_DIR}/simple_forward.vert.spv")
set(VVE_SIMPLE_FORWARD_FRAGMENT_SPV "${VVE_SIMPLE_SHADER_OUTPUT_DIR}/simple_forward.frag.spv")

target_compile_definitions(ViennaVulkanEngineSimple
   PRIVATE
      "VVE_SIMPLE_SHADER_DIR=\"${VVE_SIMPLE_SHADER_OUTPUT_DIR}\""
)

if(DEFINED SLANGC_EXECUTABLE AND NOT SLANGC_EXECUTABLE STREQUAL "")
   set(VVE_SIMPLE_SLANGC "${SLANGC_EXECUTABLE}")
elseif(DEFINED SLANG_EXECUTABLE AND NOT SLANG_EXECUTABLE STREQUAL "")
   set(VVE_SIMPLE_SLANGC "${SLANG_EXECUTABLE}")
elseif(TARGET slang::slangc)
   set(VVE_SIMPLE_SLANGC "$<TARGET_FILE:slang::slangc>")
else()
   get_target_property(VVE_SIMPLE_SLANG_LOCATION slang::slang IMPORTED_LOCATION_RELEASE)
   if(NOT VVE_SIMPLE_SLANG_LOCATION)
      get_target_property(VVE_SIMPLE_SLANG_LOCATION slang::slang IMPORTED_LOCATION)
   endif()
   get_filename_component(VVE_SIMPLE_SLANG_DIR "${VVE_SIMPLE_SLANG_LOCATION}" DIRECTORY)
   find_program(VVE_SIMPLE_SLANGC
      NAMES slangc slangc.exe
      HINTS "${VVE_SIMPLE_SLANG_DIR}" "${VVE_VULKAN_SDK_ROOT}/bin" "${VVE_VULKAN_SDK_ROOT}/Bin"
      NO_DEFAULT_PATH
   )
endif()

if(NOT VVE_SIMPLE_SLANGC)
   message(FATAL_ERROR "Could not find slangc for compiling simple engine shaders.")
endif()

add_custom_command(
   OUTPUT "${VVE_SIMPLE_FORWARD_VERTEX_SPV}"
   COMMAND "${CMAKE_COMMAND}" -E make_directory "${VVE_SIMPLE_SHADER_OUTPUT_DIR}"
   COMMAND "${VVE_SIMPLE_SLANGC}" "${VVE_SIMPLE_FORWARD_VERTEX_SHADER}"
      -target spirv -profile spirv_1_4 -stage vertex -entry vertexMain -fvk-use-entrypoint-name
      -o "${VVE_SIMPLE_FORWARD_VERTEX_SPV}"
   DEPENDS "${VVE_SIMPLE_FORWARD_VERTEX_SHADER}"
   COMMENT "Compiling simple forward vertex shader"
   VERBATIM
)

add_custom_command(
   OUTPUT "${VVE_SIMPLE_FORWARD_FRAGMENT_SPV}"
   COMMAND "${CMAKE_COMMAND}" -E make_directory "${VVE_SIMPLE_SHADER_OUTPUT_DIR}"
   COMMAND "${VVE_SIMPLE_SLANGC}" "${VVE_SIMPLE_FORWARD_FRAGMENT_SHADER}"
      -target spirv -profile spirv_1_4 -stage fragment -entry fragmentMain -fvk-use-entrypoint-name
      -o "${VVE_SIMPLE_FORWARD_FRAGMENT_SPV}"
   DEPENDS "${VVE_SIMPLE_FORWARD_FRAGMENT_SHADER}"
   COMMENT "Compiling simple forward fragment shader"
   VERBATIM
)

add_custom_target(vve_simple_shaders
   DEPENDS
      "${VVE_SIMPLE_FORWARD_VERTEX_SPV}"
      "${VVE_SIMPLE_FORWARD_FRAGMENT_SPV}"
)
add_dependencies(ViennaVulkanEngineSimple vve_simple_shaders)
