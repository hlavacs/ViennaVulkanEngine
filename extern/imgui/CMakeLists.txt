
set(TARGET imgui)

set(SOURCE
  imgui.cpp
  imgui_demo.cpp
  imgui_draw.cpp
  imgui_impl_glfw.cpp
  imgui_impl_sdl.cpp
  imgui_impl_vulkan.cpp
  imgui_tables.cpp
  imgui_widgets.cpp
)

set(HEADERS
  ${EXTERN}/imgui/imconfig.h
  ${EXTERN}/imgui/imgui.h
  ${EXTERN}/imgui/imgui_impl_glfw.h
  ${EXTERN}/imgui/imgui_impl_sdl.h
  ${EXTERN}/imgui/imgui_impl_vulkan.h
  ${EXTERN}/imgui/imgui_internal.h
  ${EXTERN}/imgui/imstb_rectpack.h
  ${EXTERN}/imgui/imstb_textedit.h
  ${EXTERN}/imgui/imstb_truetype.h
)

add_library (${TARGET} ${SOURCE} ${HEADERS})

target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/include)
target_include_directories(${TARGET} PUBLIC ${SDL2}/include/SDL2)
target_include_directories(${TARGET} PUBLIC ../../extern/glfw/include)

target_compile_features(${TARGET} PUBLIC cxx_std_20)
