
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
  ${EXTERNAL}/imgui/imconfig.h
  ${EXTERNAL}/imgui/imgui.h
  ${EXTERNAL}/imgui/imgui_impl_glfw.h
  ${EXTERNAL}/imgui/imgui_impl_sdl.h
  ${EXTERNAL}/imgui/imgui_impl_vulkan.h
  ${EXTERNAL}/imgui/imgui_internal.h
  ${EXTERNAL}/imgui/imstb_rectpack.h
  ${EXTERNAL}/imgui/imstb_textedit.h
  ${EXTERNAL}/imgui/imstb_truetype.h
)

add_library (${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)
