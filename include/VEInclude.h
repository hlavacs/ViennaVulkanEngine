#pragma once

#include <cstdint>
#include <shared_mutex>


namespace vve {

	#define MAX_MESSAGE_SIZE 256

    struct Empty {};

    using Mutex = std::shared_mutex;

   	class System;
   	class Engine;
	class SoundManager;
	class GUI;
    class Window;
	class WindowSDL;
	class Renderer;
	class RendererImgui;
	class RendererForward;
	class RendererVulkan;
   	class SceneManager;
   	class AssetManager;
}

#include <assimp/cimport.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/DefaultLogger.hpp>

inline auto to_vec4 (const aiColor4D &color) {
	return glm::vec4{color.r, color.g, color.b, color.a};
}

#include "VSTY.h"
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
#include "VESoundManager.h"


