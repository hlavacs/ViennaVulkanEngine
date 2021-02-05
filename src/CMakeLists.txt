
set(TARGET vve)

set(SOURCE
  VEEngine.cpp
  VEImgui.cpp
)

set(HEADERS
  ${INCLUDE}/VGJS.h
  ${INCLUDE}/VGJSCoro.h
  ${INCLUDE}/VVE.h
  ${INCLUDE}/VEEngine.h
)

add_library (${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_link_libraries(${TARGET} LINK_INTERFACE_LIBRARIES $(VULKAN_SDK)/Lib/vulkan-1.lib)
