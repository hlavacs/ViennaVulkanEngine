#pragma once

#include <cstdint>
#include <shared_mutex>


namespace vve {

    using ArchitectureType = int;
	const ArchitectureType ENGINETYPE_SEQUENTIAL = 0;
	const ArchitectureType ENGINETYPE_PARALLEL = 1;

	#define VVE_ARCHITECTURE_TYPE ENGINETYPE_SEQUENTIAL

	#define MAX_MESSAGE_SIZE 256

    struct Empty {};

    template<ArchitectureType ATYPE>
    using Mutex = std::conditional_t<VVE_ARCHITECTURE_TYPE == ENGINETYPE_SEQUENTIAL, Empty, std::shared_mutex>;

   	class System;
   	class Engine;
	class GUI;
    class Window;
	class WindowSDL;
	class Renderer;
	class RendererImgui;
	class RendererForward;
	class RendererVulkan;
   	class SceneManager;
}

#include "VECS.h"
#include "VESystem.h"
#include "VEEngine.h"
#include "VEGUI.h"
#include "VEWindow.h"
#include "VEWindowSDL.h"
#include "VERenderer.h"
#include "VERendererImgui.h"
#include "VERendererForward.h"
#include "VERendererVulkan.h"
#include "VESceneManager.h"
#include "VEAssetManager.h"


