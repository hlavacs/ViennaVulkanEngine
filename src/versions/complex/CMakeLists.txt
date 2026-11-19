target_sources(ViennaVulkanEngine
   PRIVATE
      "${CMAKE_CURRENT_LIST_DIR}/Audio.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/stb_image_write_impl.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/vma_impl.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererResources.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererDeferred.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererToneMap.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererSmaa.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererGi.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererGiLifecycle.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererTiming.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererAo.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererShadowPrep.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererDraw.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RendererDebug.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RenderSceneImport.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RenderSystemScene.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/Render/RenderSystemObjects.cpp"
   PUBLIC
      FILE_SET complex_modules TYPE CXX_MODULES FILES
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
         Vulkan/DebugLabels.ixx
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
         Render/IrradianceProbes.ixx
         Render/RenderSystem.ixx
)

set(VVE_COMPLEX_FORWARD_SHADER "${CMAKE_CURRENT_LIST_DIR}/shaders/complex_forward.slang")
set(VVE_COMPLEX_SHARED_SHADER_HEADER "${CMAKE_CURRENT_LIST_DIR}/shaders/complex_shared.h")
set(VVE_COMPLEX_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

target_compile_definitions(ViennaVulkanEngine
   PRIVATE
      "$<$<NOT:$<CONFIG:Debug>>:VVE_COMPLEX_RELEASE_PRESENT_MAILBOX>"
)

if(DEFINED SLANGC_EXECUTABLE AND NOT SLANGC_EXECUTABLE STREQUAL "")
   set(VVE_COMPLEX_SLANGC "${SLANGC_EXECUTABLE}")
elseif(DEFINED SLANG_EXECUTABLE AND NOT SLANG_EXECUTABLE STREQUAL "")
   set(VVE_COMPLEX_SLANGC "${SLANG_EXECUTABLE}")
elseif(TARGET slang::slangc)
   set(VVE_COMPLEX_SLANGC "$<TARGET_FILE:slang::slangc>")
else()
   get_target_property(VVE_COMPLEX_SLANG_LOCATION slang::slang IMPORTED_LOCATION_RELEASE)
   if(NOT VVE_COMPLEX_SLANG_LOCATION)
      get_target_property(VVE_COMPLEX_SLANG_LOCATION slang::slang IMPORTED_LOCATION)
   endif()
   get_filename_component(VVE_COMPLEX_SLANG_DIR "${VVE_COMPLEX_SLANG_LOCATION}" DIRECTORY)
   find_program(VVE_COMPLEX_SLANGC
      NAMES slangc slangc.exe
      HINTS "${VVE_COMPLEX_SLANG_DIR}" "${VVE_VULKAN_SDK_ROOT}/bin" "${VVE_VULKAN_SDK_ROOT}/Bin"
      NO_DEFAULT_PATH
   )
endif()

if(NOT VVE_COMPLEX_SLANGC)
   message(FATAL_ERROR "Could not find slangc for compiling complex engine shaders.")
endif()
# Consumers compiling native stages can reuse the selected compiler without a second SDK search.
set_property(TARGET ViennaVulkanEngine PROPERTY VVE_SLANGC "${VVE_COMPLEX_SLANGC}")

# Embed aligned SPIR-V words, as vvppl does; only RendererResources.cpp includes these headers.
set(VVE_COMPLEX_SHADER_NAMES complex_forward_vert complex_forward_frag complex_forward_shadow_vert complex_gbuffer_frag complex_gbuffer_msaa_frag complex_lighting_vert complex_lighting_frag complex_lighting_msaa_frag
   complex_terrain_vert complex_terrain_hull complex_terrain_domain complex_terrain_shadow_domain complex_probe_frag complex_probe_integrate complex_lighting_gi_frag complex_lighting_gi_msaa_frag
   complex_ao_frag complex_ao_msaa_frag complex_ao_filter_frag complex_ao_filter_msaa_frag complex_lighting_gi_ao_frag complex_lighting_gi_ao_msaa_frag)
set(VVE_COMPLEX_SHADER_ENTRIES vertexMain fragmentMain shadowVertexMain gbufferMain gbufferMsaaMain lightingVertexMain lightingMain lightingMain
   terrainVertexMain terrainHullMain terrainDomainMain terrainShadowDomainMain fragmentMain integrateMain lightingMain lightingMain
   aoMain aoMain aoFilterMain aoFilterMain lightingMain lightingMain)
set(VVE_COMPLEX_SHADER_STAGES vertex fragment vertex fragment fragment vertex fragment fragment
   vertex hull domain domain fragment fragment fragment fragment
   fragment fragment fragment fragment fragment fragment)
list(APPEND VVE_COMPLEX_SHADER_NAMES complex_forward_gi_frag complex_gbuffer_gi_frag complex_gbuffer_gi_msaa_frag complex_shadow_gi_vert complex_shadow_gi_frag)
list(APPEND VVE_COMPLEX_SHADER_ENTRIES fragmentMain gbufferMain gbufferMsaaMain shadowMaterialVertexMain shadowFragmentMain)
list(APPEND VVE_COMPLEX_SHADER_STAGES fragment fragment fragment vertex fragment)
list(APPEND VVE_COMPLEX_SHADER_NAMES complex_tonemap_vert complex_tonemap_frag)
list(APPEND VVE_COMPLEX_SHADER_ENTRIES toneMapVertex toneMapFragment)
list(APPEND VVE_COMPLEX_SHADER_STAGES vertex fragment)
list(APPEND VVE_COMPLEX_SHADER_NAMES complex_smaa_edges complex_smaa_weights complex_smaa_blend)
list(APPEND VVE_COMPLEX_SHADER_ENTRIES smaaEdges smaaWeights smaaBlend)
list(APPEND VVE_COMPLEX_SHADER_STAGES fragment fragment fragment)
list(APPEND VVE_COMPLEX_SHADER_NAMES complex_gui_output_vert complex_gui_output_frag)
list(APPEND VVE_COMPLEX_SHADER_ENTRIES guiOutputVertex guiOutputFragment)
list(APPEND VVE_COMPLEX_SHADER_STAGES vertex fragment)
set(VVE_COMPLEX_SHADER_HEADERS "")
foreach(NAME ENTRY STAGE IN ZIP_LISTS VVE_COMPLEX_SHADER_NAMES VVE_COMPLEX_SHADER_ENTRIES VVE_COMPLEX_SHADER_STAGES)
   set(HEADER "${VVE_COMPLEX_SHADER_OUTPUT_DIR}/${NAME}_spv.h")
   set(SHADER "${VVE_COMPLEX_FORWARD_SHADER}")
   set(SHADER_DEFINES "")
   set(SHADER_DEPENDENCIES "")
   if(NAME MATCHES "complex_smaa")
      set(SHADER "${CMAKE_CURRENT_LIST_DIR}/shaders/complex_smaa.slang")
      set(SHADER_DEPENDENCIES "${PROJECT_SOURCE_DIR}/third_party/SMAA/SMAA.hlsl")
   endif()
   if(NAME MATCHES "complex_tonemap")
      set(SHADER "${CMAKE_CURRENT_LIST_DIR}/shaders/complex_tonemap.slang")
   endif()
   if(NAME MATCHES "complex_gui_output")
      set(SHADER "${CMAKE_CURRENT_LIST_DIR}/shaders/complex_gui_output.slang")
   endif()
   if(NAME MATCHES "complex_lighting")
      set(SHADER "${CMAKE_CURRENT_LIST_DIR}/shaders/complex_deferred.slang")
   endif()
   if(NAME MATCHES "complex_ao")
      set(SHADER "${CMAKE_CURRENT_LIST_DIR}/shaders/complex_ao.slang")
   endif()
   if(NAME MATCHES "msaa")
      set(SHADER_DEFINES -DVVE_DEFERRED_MSAA=1)
   endif()
   if(NAME MATCHES "_gi_")
      if(NAME MATCHES "gbuffer|shadow")
         list(APPEND SHADER_DEFINES -DVVE_GI_MATERIAL=1)
      else()
         list(APPEND SHADER_DEFINES -DVVE_GI=1)
      endif()
   endif()
   if(NAME MATCHES "lighting_gi_ao")
      list(APPEND SHADER_DEFINES -DVVE_CONTACT_AO=1)
   endif()
   if(NAME STREQUAL "complex_probe_frag")
      list(APPEND SHADER_DEFINES -DVVE_PROBE_CAPTURE=1)
   elseif(NAME STREQUAL "complex_probe_integrate")
      set(SHADER "${CMAKE_CURRENT_LIST_DIR}/shaders/complex_probe.slang")
   endif()
   add_custom_command(
      OUTPUT "${HEADER}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${VVE_COMPLEX_SHADER_OUTPUT_DIR}"
      COMMAND "${VVE_COMPLEX_SLANGC}" "${SHADER}" ${SHADER_DEFINES}
         -target spirv -profile spirv_1_4 -stage ${STAGE} -entry ${ENTRY} -fvk-use-entrypoint-name
         -source-embed-style u32 -source-embed-name ${NAME}_spv -o "${HEADER}"
      # Slang preserves unchanged outputs; still rebuild the embedding unit after a source touch.
      COMMAND "${CMAKE_COMMAND}" -E touch "${HEADER}"
      DEPENDS "${SHADER}" "${VVE_COMPLEX_FORWARD_SHADER}" "${VVE_COMPLEX_SHARED_SHADER_HEADER}" "${CMAKE_CURRENT_LIST_DIR}/shaders/complex_material.slang"
         ${SHADER_DEPENDENCIES}
         "${CMAKE_CURRENT_LIST_DIR}/shaders/complex_atmosphere.slang"
         "${CMAKE_CURRENT_LIST_DIR}/shaders/complex_irradiance.slang"
         "${CMAKE_CURRENT_LIST_DIR}/shaders/complex_deferred.slang"
         "${PROJECT_SOURCE_DIR}/src/shaders/vve_irradiance.slang" "${PROJECT_SOURCE_DIR}/src/shaders/vve_irradiance.h"
         "${PROJECT_SOURCE_DIR}/src/shaders/vve_surface_flags.slang"
      COMMENT "Embedding complex forward ${ENTRY} shader"
      VERBATIM
   )
   list(APPEND VVE_COMPLEX_SHADER_HEADERS "${HEADER}")
endforeach()

# Generate before module scanning, and track the included headers for incremental rebuilds.
add_custom_target(vve_complex_shaders DEPENDS ${VVE_COMPLEX_SHADER_HEADERS})
add_dependencies(ViennaVulkanEngine vve_complex_shaders)
target_sources(ViennaVulkanEngine PRIVATE ${VVE_COMPLEX_SHADER_HEADERS})
target_include_directories(ViennaVulkanEngine PRIVATE "${VVE_COMPLEX_SHADER_OUTPUT_DIR}")
