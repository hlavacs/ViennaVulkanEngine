#pragma once

#include <cstdint>
#include <shared_mutex>
#include <vector>
#include <cstdint>
#include <variant>
#include <mutex>
#include <functional>
#include <typeindex>
#include <typeinfo>
#include <type_traits>
#include <unordered_set>
#include <filesystem>
#include <chrono>
#include <any>
#include <algorithm>
#include <iterator>
#include <ranges>

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

namespace vve {

	#define MAX_MESSAGE_SIZE 256

    struct Empty {};

   	class System;
   	class Engine;
	class GUI;
    class Window;
	class WindowSDL;
	class Renderer;
	class RendererImgui;
	class RendererForward;
	class RendererForward11;
	class RendererVulkan;
   	class SceneManager;
   	class AssetManager;
	class SoundManager;

	using Name = vsty::strong_type_t<std::string, vsty::counter<>>;
	using SystemName = vsty::strong_type_t<std::string, vsty::counter<>>;
	using Filename = vsty::strong_type_t<std::string, vsty::counter<>>;

	using ObjectHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>;
	using ParentHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>;
	using ChildHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>;
	using SiblingHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>;
	using SoundHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>;

	using CameraHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Camera as a unique component

	using PointLightHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use PointLight as a unique component
	using DirectionalLightHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use DirectionalLight as a unique component
	using SpotLightHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use SpotLight as a unique component

	using MeshHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Texture as a unique comonent

	using TextureHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use Texture as a unique comonent
	using NormalMapHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use NormalMap as a unique component
	using HeightMapHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use HeightMap as a unique component
	using LightMapHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use LightMap as a unique component
	using OcclusionMapHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>; //need this to use OcclusionMap as a unique component
}

#include "VESystem.h"
#include "VEEngine.h"
#include "VEGUI.h"
#include "VEWindow.h"
#include "VEWindowSDL.h"
#include "VERenderer.h"
#include "VERendererImgui.h"
#include "VERendererForward.h"
#include "VERendererForward11.h"
#include "VERendererVulkan.h"
#include "VESceneManager.h"
#include "VEAssetManager.h"
#include "VESoundManager.h"
