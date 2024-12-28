#pragma once

#include <cstdint>
#include <shared_mutex>


namespace vve {

    using ArchitectureType = int;
	const ArchitectureType ENGINETYPE_SEQUENTIAL = 0;
	const ArchitectureType ENGINETYPE_PARALLEL = 1;

    struct Empty {};

    template<ArchitectureType ATYPE>
    using Mutex = std::conditional_t<ATYPE == ENGINETYPE_SEQUENTIAL, Empty, std::shared_mutex>;

   	template<ArchitectureType ATYPE> class System;
   	template<ArchitectureType ATYPE> class Vulkan;
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

#include "VECS.h"
#include "VEVulkan.h"
#include "VEEngine.h"
#include "VEGUI.h"
#include "VESystem.h"
#include "VEWindow.h"
#include "VEWindowSDL.h"
#include "VERenderer.h"
#include "VERendererImgui.h"
#include "VERendererForward.h"
#include "VERendererVulkan.h"
#include "VESceneManager.h"


