target_sources(ViennaVulkanEngine
   PRIVATE
      "${CMAKE_CURRENT_LIST_DIR}/Audio.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/stb_image_write_impl.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/vma_impl.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererResources.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererShadowPrep.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererDraw.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererDebug.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RenderSceneImport.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RenderSystemScene.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RenderSystemObjects.cpp"
   PUBLIC
      FILE_SET simple_modules TYPE CXX_MODULES FILES
         Types.ixx
         Graph.ixx
         Window.ixx
         Assets.ixx
         GUI.ixx
         Audio.ixx
         Engine.ixx
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
         Render/RenderResources.ixx
         Render/Renderer.ixx
         Render/RenderSystem.ixx
)

set(VVE_SIMPLE_FORWARD_SHADER "${CMAKE_CURRENT_LIST_DIR}/shaders/simple_forward.slang")
set(VVE_SIMPLE_SHARED_SHADER_HEADER "${CMAKE_CURRENT_LIST_DIR}/shaders/simple_shared.h")
set(VVE_SIMPLE_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

target_compile_definitions(ViennaVulkanEngine
   PRIVATE
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

# Embed aligned SPIR-V words, as vvppl does; only RendererResources.cpp includes these headers.
set(VVE_SIMPLE_SHADER_NAMES simple_forward_vert simple_forward_frag simple_forward_shadow_vert)
set(VVE_SIMPLE_SHADER_ENTRIES vertexMain fragmentMain shadowVertexMain)
set(VVE_SIMPLE_SHADER_STAGES vertex fragment vertex)
set(VVE_SIMPLE_SHADER_HEADERS "")
foreach(NAME ENTRY STAGE IN ZIP_LISTS VVE_SIMPLE_SHADER_NAMES VVE_SIMPLE_SHADER_ENTRIES VVE_SIMPLE_SHADER_STAGES)
   set(HEADER "${VVE_SIMPLE_SHADER_OUTPUT_DIR}/${NAME}_spv.h")
   add_custom_command(
      OUTPUT "${HEADER}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${VVE_SIMPLE_SHADER_OUTPUT_DIR}"
      COMMAND "${VVE_SIMPLE_SLANGC}" "${VVE_SIMPLE_FORWARD_SHADER}"
         -target spirv -profile spirv_1_4 -stage ${STAGE} -entry ${ENTRY} -fvk-use-entrypoint-name
         -source-embed-style u32 -source-embed-name ${NAME}_spv -o "${HEADER}"
      # Slang preserves unchanged outputs; still rebuild the embedding unit after a source touch.
      COMMAND "${CMAKE_COMMAND}" -E touch "${HEADER}"
      DEPENDS "${VVE_SIMPLE_FORWARD_SHADER}" "${VVE_SIMPLE_SHARED_SHADER_HEADER}"
      COMMENT "Embedding simple forward ${ENTRY} shader"
      VERBATIM
   )
   list(APPEND VVE_SIMPLE_SHADER_HEADERS "${HEADER}")
endforeach()

# Generate before module scanning, and track the included headers for incremental rebuilds.
add_custom_target(vve_simple_shaders DEPENDS ${VVE_SIMPLE_SHADER_HEADERS})
add_dependencies(ViennaVulkanEngine vve_simple_shaders)
target_sources(ViennaVulkanEngine PRIVATE ${VVE_SIMPLE_SHADER_HEADERS})
target_include_directories(ViennaVulkanEngine PRIVATE "${VVE_SIMPLE_SHADER_OUTPUT_DIR}")
