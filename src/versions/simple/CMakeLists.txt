target_sources(ViennaVulkanEngine
   PRIVATE
      "${CMAKE_CURRENT_LIST_DIR}/stb_image_write_impl.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/vma_impl.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererResources.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererShadowPrep.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererDraw.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererDebug.cpp"
   PUBLIC
      FILE_SET simple_modules TYPE CXX_MODULES FILES
         Error.ixx
         Math.ixx
         Handle.ixx
         Vector.ixx
         Types.ixx
         Graph.ixx
         Window.ixx
         Assets.ixx
         GUI.ixx
         Engine.ixx
         Mesh.ixx
         Vulkan/Handle.ixx
         Vulkan/Memory.ixx
         Vulkan/Device.ixx
         Vulkan/Commands.ixx
         Vulkan/Presentation.ixx
         Vulkan/Pipeline.ixx
         Vulkan/Shadow.ixx
         Vulkan/Readback.ixx
         Vulkan/Resources.ixx
         Vulkan/Vulkan.ixx
         Scene.ixx
         Render/RenderSceneImport.ixx
         Render/RenderSystemScene.ixx
         Render/RenderSystemObjects.ixx
         Render/RenderResources.ixx
         Render/Renderer.ixx
         Render/RenderSystem.ixx
)

set(VVE_SIMPLE_FORWARD_SHADER "${CMAKE_CURRENT_LIST_DIR}/shaders/simple_forward.slang")
set(VVE_SIMPLE_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(VVE_SIMPLE_FORWARD_VERTEX_SPV "${VVE_SIMPLE_SHADER_OUTPUT_DIR}/simple_forward.vert.spv")
set(VVE_SIMPLE_FORWARD_FRAGMENT_SPV "${VVE_SIMPLE_SHADER_OUTPUT_DIR}/simple_forward.frag.spv")
set(VVE_SIMPLE_FORWARD_SHADOW_VERTEX_SPV "${VVE_SIMPLE_SHADER_OUTPUT_DIR}/simple_forward.shadow.vert.spv")

target_compile_definitions(ViennaVulkanEngine
   PRIVATE
      "VVE_SIMPLE_SHADER_DIR=\"${VVE_SIMPLE_SHADER_OUTPUT_DIR}\""
      "$<$<NOT:$<CONFIG:Debug>>:VVE_SIMPLE_RELEASE_PRESENT_MAILBOX>"
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
   COMMAND "${VVE_SIMPLE_SLANGC}" "${VVE_SIMPLE_FORWARD_SHADER}"
      -target spirv -profile spirv_1_4 -stage vertex -entry vertexMain -fvk-use-entrypoint-name
      -o "${VVE_SIMPLE_FORWARD_VERTEX_SPV}"
   DEPENDS "${VVE_SIMPLE_FORWARD_SHADER}"
   COMMENT "Compiling simple forward vertex shader"
   VERBATIM
)

add_custom_command(
   OUTPUT "${VVE_SIMPLE_FORWARD_FRAGMENT_SPV}"
   COMMAND "${CMAKE_COMMAND}" -E make_directory "${VVE_SIMPLE_SHADER_OUTPUT_DIR}"
   COMMAND "${VVE_SIMPLE_SLANGC}" "${VVE_SIMPLE_FORWARD_SHADER}"
      -target spirv -profile spirv_1_4 -stage fragment -entry fragmentMain -fvk-use-entrypoint-name
      -o "${VVE_SIMPLE_FORWARD_FRAGMENT_SPV}"
   DEPENDS "${VVE_SIMPLE_FORWARD_SHADER}"
   COMMENT "Compiling simple forward fragment shader"
   VERBATIM
)

add_custom_command(
   OUTPUT "${VVE_SIMPLE_FORWARD_SHADOW_VERTEX_SPV}"
   COMMAND "${CMAKE_COMMAND}" -E make_directory "${VVE_SIMPLE_SHADER_OUTPUT_DIR}"
   COMMAND "${VVE_SIMPLE_SLANGC}" "${VVE_SIMPLE_FORWARD_SHADER}"
      -target spirv -profile spirv_1_4 -stage vertex -entry shadowVertexMain -fvk-use-entrypoint-name
      -o "${VVE_SIMPLE_FORWARD_SHADOW_VERTEX_SPV}"
   DEPENDS "${VVE_SIMPLE_FORWARD_SHADER}"
   COMMENT "Compiling simple forward shadow vertex shader"
   VERBATIM
)

add_custom_target(vve_simple_shaders
   DEPENDS
      "${VVE_SIMPLE_FORWARD_VERTEX_SPV}"
      "${VVE_SIMPLE_FORWARD_FRAGMENT_SPV}"
      "${VVE_SIMPLE_FORWARD_SHADOW_VERTEX_SPV}"
)
add_dependencies(ViennaVulkanEngine vve_simple_shaders)
