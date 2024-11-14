#pragma once

#include <cstdint>
#include <mutex>

namespace vve {

    enum class ArchitectureType : int {
        SEQUENTIAL = 0,
        PARALLEL
    };	

    struct Empty {};

    template<ArchitectureType ATYPE>
    using Mutex = std::conditional_t<ATYPE == ArchitectureType::SEQUENTIAL, Empty, std::mutex>;

   	template<ArchitectureType ATYPE> class System;
   	template<ArchitectureType ATYPE> class Engine;
    template<ArchitectureType ATYPE> class Window;
	template<ArchitectureType ATYPE> class WindowSDL;
	template<ArchitectureType ATYPE> class Renderer;
	template<ArchitectureType ATYPE> class RendererImgui;
	template<ArchitectureType ATYPE> class RendererForward;
	template<ArchitectureType ATYPE> class RendererVulkan;
   	template<ArchitectureType ATYPE> class SceneManager;
	struct Message;
}



