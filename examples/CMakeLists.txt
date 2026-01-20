add_subdirectory(helper)
add_subdirectory(game)
add_subdirectory(physics)
add_subdirectory(deferred-demo)

# Gaussian demos require VVE_GAUSSIAN_ENABLED
if(VVE_GAUSSIAN_ENABLED)
    add_subdirectory(gaussian-splatting-demo)
    add_subdirectory(gaussian-splatting-cubemap-lighting-demo)
endif()
