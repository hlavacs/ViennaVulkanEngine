
set(TARGET imgui)

set(SOURCE
  imgui.cpp
  imgui_demo.cpp
  imgui_draw.cpp
  imgui_impl_sdl.cpp
  imgui_impl_vulkan.cpp
  imgui_tables.cpp
  imgui_widgets.cpp
)

set(HEADERS
  imconfig.h
  imgui.h
  imgui_impl_sdl.h
  imgui_impl_vulkan.h
  imgui_internal.h
  imstb_rectpack.h
  imstb_textedit.h
  imstb_truetype.h
)

add_library (${TARGET} ${SOURCE} ${HEADERS})

target_include_directories(${TARGET} PUBLIC $ENV{VULKAN_SDK}/include)
target_include_directories(${TARGET} PUBLIC ${SDL2}/include/SDL2)

target_compile_features(${TARGET} PUBLIC cxx_std_20)
