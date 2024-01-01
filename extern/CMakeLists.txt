
# Vulkan Memory Allocator
file(DOWNLOAD
    https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/master/include/vk_mem_alloc.h
    ${PROJECT_SOURCE_DIR}/extern/VulkanMemoryAllocator/vk_mem_alloc.h)

# Vienna Type List Library
file(DOWNLOAD
    https://raw.githubusercontent.com/hlavacs/ViennaTypeListLibrary/main/include/VTLL.h
    ${PROJECT_SOURCE_DIR}/extern/ViennaTypeListLibrary/VTLL.h)

# Vienna Strong Type Library
file(DOWNLOAD
    https://raw.githubusercontent.com/hlavacs/ViennaStrongType/main/include/VSTY.h
    ${PROJECT_SOURCE_DIR}/extern/ViennaStrongType/VSTY.h)

# Vienna Game Job System (branch simple)
file(DOWNLOAD
    https://raw.githubusercontent.com/hlavacs/ViennaGameJobSystem/simple/include/VGJS.h
    ${PROJECT_SOURCE_DIR}/extern/ViennaGameJobSystem/VGJS.h)

# Vienna Lock Less Table
file(DOWNLOAD
    https://raw.githubusercontent.com/hlavacs/ViennaLockLessTable/main/VLLT.h
    ${PROJECT_SOURCE_DIR}/extern/ViennaLockLessTable/VLLT.h)

# Vienna Entity Component System
file(DOWNLOAD
    https://raw.githubusercontent.com/hlavacs/ViennaEntityComponentSystem/main/include/VECS.h
    ${PROJECT_SOURCE_DIR}/extern/ViennaEntityComponentSystem/VECS.h)

# Vienna Physics Engine
file(DOWNLOAD
    https://raw.githubusercontent.com/hlavacs/ViennaPhysicsEngine/main/include/VPE.hpp
    ${PROJECT_SOURCE_DIR}/extern/ViennaPhysicsEngine/VPE.h)


add_subdirectory (imgui)

