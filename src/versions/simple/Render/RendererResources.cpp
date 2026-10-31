module;
#include <SDL3/SDL_video.h>
#ifdef _WIN32
#include <SDL3/SDL_stdinc.h>
#endif
#include <vulkan/vulkan_core.h>
#include <VVPPL.h>
#include "../shaders/simple_shared.h"
// Build-generated uint32_t arrays are defined only in this translation unit.
#include "simple_forward_vert_spv.h"
#include "simple_forward_frag_spv.h"
#include "simple_forward_shadow_vert_spv.h"
#include "simple_forward_gi_frag_spv.h"
#include "simple_shadow_gi_vert_spv.h"
#include "simple_shadow_gi_frag_spv.h"
#include "simple_terrain_vert_spv.h"
#include "simple_terrain_hull_spv.h"
#include "simple_terrain_domain_spv.h"
#include "simple_terrain_shadow_domain_spv.h"
#if __has_include(<backends/imgui_impl_vulkan.h>)
#include <backends/imgui_impl_vulkan.h>
#else
#include <imgui_impl_vulkan.h>
#endif

module VVEngine.Simple.Renderer;
import std;
import VVEngine.Simple.Types;
import VVEngine.Simple.RenderResources;
import VVEngine.Simple.Scene;
import VVEngine.Simple.Vulkan;

/// @file
/// @brief ForwardRenderer Vulkan resource lifetime: bring-up, scene upload, swapchain rebuild, teardown, and ImGui wiring.

namespace vve::simple {
	// Format of the hdr offscreen color target the scene is rendered into.
	constexpr VkFormat hdrFormat{VK_FORMAT_R16G16B16A16_SFLOAT};

	/// @brief Changes startup antialiasing without replacing resources referenced by submitted work.
	auto ForwardRenderer::setAntialiasing(Antialiasing mode) -> std::expected<void, Error> {
		if (mode != Antialiasing::none && mode != Antialiasing::msaa4) { return std::unexpected(Error::invalid_argument); }
		if (initialized()) { return std::unexpected(Error::already_initialized); }
		antialiasing_ = mode;
		return {};
	}

	/// @brief Releases this window's resources while the renderer's device, allocator and command pool are alive.
	void WindowTarget::cleanup() {
		// The renderer waits for outstanding work before releasing any target resources.
		postProcess.reset();
		deferredPool.cleanup();
		deferredSet = VK_NULL_HANDLE;
		probes.cleanup();
		coarseProbes.cleanup(); probeShadow.cleanup();
		probeShadowInitialized = probeShadowDirty = false;
		probeShadowGeneration = probeShadowRenders = probeShadowBytes = 0;
		for (auto &image : aoImages) { image.cleanup(); }
		gpuQueries.reset(); gpuMs.fill(std::nullopt); timingSerial = 0; aoGpuMs.reset();
		for (auto &written : timingWritten) written.reset();
		aoGpuBytes = 0; aoInitialized = aoEnabled = false;
		for (auto &image : gbufferImages) { image.cleanup(); }
		descriptorSets.cleanup();
		descriptorPool.cleanup();
		uniformBuffers.cleanup();
		for (auto &buffer : materialBuffers) { buffer.cleanup(); }
		for (auto &buffer : terrainBuffers) { buffer.cleanup(); }
		dynamicVertices.clear();
		submitSerials.fill(0U);
		frameSync.cleanup();
		commandBuffers.cleanup();
		depthImage.cleanup();
		msaaImage.cleanup();
		hdrImage.cleanup();
		imageViews.cleanup();
		swapchain.cleanup();
		surface.cleanup();
		window = nullptr;
	}

	/**
		* @brief Initializes the Vulkan instance, device, swapchain, image views, depth and HDR targets, shadow-map arrays, descriptor-set layout, pipeline layout, shader modules, forward and shadow pipelines (dynamic rendering), command pool and buffers, frame synchronization, per-frame uniform buffers, descriptor pool and sets, textures, materials, meshes, and the optional post-processing chain.
		*
		* @param sdlWindow Borrowed SDL window that owns the native platform surface.
		* @return VK_SUCCESS after graphics-pipeline bring-up, otherwise the first failing Vulkan result.
		*/
	VkResult ForwardRenderer::init(SDL_Window *sdlWindow, WindowHandle handle) {
		textureUploadError_.reset();
		if (sdlWindow == nullptr || !targets.empty()) { return VK_ERROR_INITIALIZATION_FAILED; }
#ifdef _WIN32
		constexpr std::string_view amdLayer{"VK_LAYER_AMD_switchable_graphics"};
		const char *const currentFilters = SDL_getenv_unsafe("VK_LOADER_LAYERS_DISABLE");
		const std::string current{currentFilters != nullptr ? currentFilters : ""};
		const auto filtered = detail::appendLayerFilter(current, amdLayer);
		bool amdFiltered = filtered == current;
		// SDL updates the process environment that the Vulkan loader reads on instance creation.
		const auto applyAmdFilter = [&]() {
			if (SDL_setenv_unsafe("VK_LOADER_LAYERS_DISABLE", filtered.c_str(), 1) == 0) { return true; }
			std::cerr << "[vve::simple] Could not apply Vulkan layer compatibility mode: " << SDL_GetError() << '\n';
			return false;
		};
		const char *const optIn = SDL_getenv_unsafe("VVE_DISABLE_AMD_SWITCHABLE_GRAPHICS");
		// A crashing layer cannot return an error; explicit opt-in filters it before the first instance.
		if (!amdFiltered && optIn != nullptr && std::string_view{optIn} == "1") {
			if (!applyAmdFilter()) { return VK_ERROR_INITIALIZATION_FAILED; }
			amdFiltered = true;
		}
#endif

		VkResult result{};
		// Cleanup destroys targets, so every attempt must create a fresh target and surface.
		for (;;) {
			auto &pendingTarget = targets.emplace_back();
			pendingTarget.handle = handle;
			pendingTarget.window = sdlWindow;
			pendingTarget.guiWindow = true;
			result = instance.create();
			if (result == VK_SUCCESS) {
				result = pendingTarget.surface.create(instance.instance, sdlWindow);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				result = physicalDevice.select(instance.instance, pendingTarget.surface.surface);
			}
			if (result == VK_SUCCESS) { break; }
#ifdef _WIN32
			// Retry only instance/device discovery failures, once, when the unfiltered AMD layer is present.
			std::uint32_t layerCount{};
			if (!amdFiltered && vkEnumerateInstanceLayerProperties(&layerCount, nullptr) == VK_SUCCESS && layerCount != 0U) {
				auto layers = std::vector<VkLayerProperties>(layerCount);
				if (vkEnumerateInstanceLayerProperties(&layerCount, layers.data()) == VK_SUCCESS &&
					std::ranges::any_of(layers, [amdLayer](const VkLayerProperties &layer) { return std::string_view{layer.layerName} == amdLayer; }) &&
					applyAmdFilter()) {
					amdFiltered = true;
					cleanup();
					std::cerr << "[vve::simple] Vulkan compatibility mode: AMD switchable-graphics layer disabled for this process; retrying discovery once.\n";
					continue;
				}
			}
#endif
			cleanup();
			return result;
		}
		auto &target = targets.front();

		VkPhysicalDeviceProperties deviceProperties{};
		vkGetPhysicalDeviceProperties(physicalDevice.physicalDevice, &deviceProperties);
		if (sampleCount() != VK_SAMPLE_COUNT_1_BIT) {
			// Both concrete formats and the framebuffer limits must support the requested coverage count.
			VkImageFormatProperties colour{}, depth{};
			const auto colourResult = vkGetPhysicalDeviceImageFormatProperties(physicalDevice.physicalDevice, hdrFormat,
				VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0U, &colour);
			const auto depthResult = vkGetPhysicalDeviceImageFormatProperties(physicalDevice.physicalDevice, depthFormat,
				VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0U, &depth);
			const auto supported = colour.sampleCounts & depth.sampleCounts & deviceProperties.limits.framebufferColorSampleCounts &
				deviceProperties.limits.framebufferDepthSampleCounts;
			if (colourResult != VK_SUCCESS || depthResult != VK_SUCCESS || (supported & sampleCount()) == 0U) {
				std::cerr << "[vve::simple] renderer init failed: 4x MSAA unsupported for HDR/depth attachments; colour VkResult="
					<< colourResult << " depth VkResult=" << depthResult << '\n';
				cleanup();
				return VK_ERROR_FEATURE_NOT_PRESENT;
			}
		}
		if (usesDeferred()) {
			const auto supported = deferredSupport(physicalDevice.physicalDevice, sampleCount(), backend_ == RendererBackend::hybrid_gi);
			if (supported != VK_SUCCESS) {
				std::cerr << "[vve::simple] deferred unavailable: sampled G-buffer/depth or sample shading unsupported; VkResult=" << supported << '\n';
				cleanup();
				return supported;
			}
		}
		const auto availableCapacity = materialTextureCapacity(deviceProperties.limits);
		const auto reserved = backend_ == RendererBackend::hybrid_gi ? 11U : usesDeferred() ? 4U : 0U;
		const auto capacity = availableCapacity > reserved ? availableCapacity - reserved : 0U;
		if (capacity == 0U) {
			std::cerr << "[vve::simple] renderer init failed: device descriptor limits leave no material texture slot.\n";
			cleanup();
			return VK_ERROR_FEATURE_NOT_PRESENT;
		}
		// Existing CPU scenes must fit the selected device; report overflow instead of dropping material textures.
		if (renderScene_ != nullptr) {
			const auto fit = renderScene_->setTextureCapacity(capacity);
			if (!fit) {
				textureUploadError_ = fit.error();
				std::cerr << "[vve::simple] renderer init failed: scene exceeds device texture capacity=" << capacity << '\n';
				cleanup();
				return VK_ERROR_INITIALIZATION_FAILED;
			}
		}
		textureCapacity_ = capacity;

		result = device.create(physicalDevice);
		if (result != VK_SUCCESS) { cleanup(); return result; }
		debugLabels.load(instance.instance, device.device, instance.debugUtilsEnabled());

		// All material images, including the white fallback, use this sampler for the device's lifetime.
		const VkSamplerCreateInfo samplerInfo{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			.anisotropyEnable = device.samplerAnisotropy ? VK_TRUE : VK_FALSE,
			.maxAnisotropy = device.samplerAnisotropy ? deviceProperties.limits.maxSamplerAnisotropy : 1.0F,
			.maxLod = VK_LOD_CLAMP_NONE,
		};
		result = vkCreateSampler(device.device, &samplerInfo, nullptr, &materialSampler);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		target.presentQueueFamily = *physicalDevice.presentQueueFamily;
		target.presentQueue = device.presentQueue;

		result = allocator.create(instance.instance, physicalDevice.physicalDevice, device.device,
													  std::min<std::uint32_t>(deviceProperties.apiVersion, VK_API_VERSION_1_3)); ///< VMA needs a version both instance and device support.
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = vertShaderModule.create(device.device, simple_forward_vert_spv);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = fragShaderModule.create(device.device, simple_forward_frag_spv);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = shadowShaderModule.create(device.device, simple_forward_shadow_vert_spv);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		// Terrain stages are created only where they can be used; without them terrain objects are skipped.
		if (device.tessellationShader) {
			result = terrainVertexShader.create(device.device, simple_terrain_vert_spv);
			if (result == VK_SUCCESS) { result = terrainHullShader.create(device.device, simple_terrain_hull_spv); }
			if (result == VK_SUCCESS) { result = terrainDomainShader.create(device.device, simple_terrain_domain_spv); }
			if (result == VK_SUCCESS) { result = terrainShadowDomainShader.create(device.device, simple_terrain_shadow_domain_spv); }
			if (result != VK_SUCCESS) { cleanup(); return result; }
		}

		result = ensureTextureCapacity();
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = commandPool.create(device.device, *physicalDevice.graphicsQueueFamily);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		// The physical sky is drawn on an inward cube: corners by bits x = 1, y = 2, z = 4, faces wound towards the centre.
		std::vector<RenderVertex> skyCorners(8U);
		for (std::uint32_t corner{}; corner < 8U; ++corner) {
			skyCorners[corner].position = Vec3{corner & 1U ? one() : -one(), corner & 2U ? one() : -one(), corner & 4U ? one() : -one()};
		}
		const struct { const std::vector<RenderVertex> *vertices; std::vector<std::uint32_t> indices; } skyCube{&skyCorners,
			{1, 7, 3, 1, 5, 7, 0, 6, 4, 0, 2, 6, 2, 7, 6, 2, 3, 7, 0, 5, 1, 0, 4, 5, 4, 7, 5, 4, 6, 7, 0, 3, 2, 0, 1, 3}};
		result = skyMesh.create(allocator, device.device, device.graphicsQueue, commandPool.commandPool, skyCube);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		// Start with one shader-readable layer per array; the first draw grows to the packed light counts.
		frameUniforms_ = {};
		result = ensureShadowCapacity();
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = shadowDepthReadback.create(allocator, device.device, device.graphicsQueue, commandPool.commandPool, VkExtent2D{.width = ShadowMap::resolution, .height = ShadowMap::resolution}, depthFormat);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		createImguiDescriptorPool();

		result = syncSceneResources();
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = createTargetResources(target);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		return VK_SUCCESS;
	}

	/// @brief Adds a surface that the existing graphics queue can present, retaining the shared device and pipelines.
	VkResult ForwardRenderer::addTarget(SDL_Window *sdlWindow, WindowHandle handle) {
		if (!initialized() || sdlWindow == nullptr) { return VK_ERROR_INITIALIZATION_FAILED; }
		auto &target = targets.emplace_back();
		target.handle = handle;
		target.window = sdlWindow;
		target.presentQueueFamily = *physicalDevice.graphicsQueueFamily;
		target.presentQueue = device.graphicsQueue;
		VkResult result = target.surface.create(instance.instance, sdlWindow);
		if (result == VK_SUCCESS) {
			VkBool32 supported{};
			result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice.physicalDevice,
				target.presentQueueFamily, target.surface.surface, &supported);
			if (result != VK_SUCCESS || !supported) {
				std::cerr << "[vve::simple] window '" << SDL_GetWindowTitle(sdlWindow)
					<< "' cannot present from graphics queue family " << target.presentQueueFamily << '\n';
				if (result == VK_SUCCESS) { result = VK_ERROR_INITIALIZATION_FAILED; }
			}
		}
		if (result == VK_SUCCESS) { result = createTargetResources(target); }
		if (result != VK_SUCCESS) { target.cleanup(); targets.pop_back(); }
		return result;
	}

	/// @brief Allocates matched forward attachments; the resolved HDR image remains usable by every post effect.
	VkResult ForwardRenderer::createTargetAttachments(WindowTarget &target) {
		auto result = target.depthImage.create(allocator, device.device, target.swapchain.extent, depthFormat,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | (usesDeferred() ? VK_IMAGE_USAGE_SAMPLED_BIT : 0U), VK_IMAGE_ASPECT_DEPTH_BIT, 1U, false, false, 1U, sampleCount());
		if (result != VK_SUCCESS) { return result; }
		result = target.hdrImage.create(allocator, device.device, target.swapchain.extent, hdrFormat,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
		if (result != VK_SUCCESS) { return result; }
		if (sampleCount() != VK_SAMPLE_COUNT_1_BIT) {
			result = target.msaaImage.create(allocator, device.device, target.swapchain.extent, hdrFormat,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 1U, false, false, 1U, sampleCount());
			if (result != VK_SUCCESS) { return result; }
		}
		if (usesDeferred()) {
			// Keep every surface sample separate until lighting and transparent composition are complete.
			for (auto i : std::views::iota(0U, 3U)) {
				result = target.gbufferImages[i].create(allocator, device.device, target.swapchain.extent, gbufferFormats[i],
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 1U, false, false, 1U, sampleCount());
				if (result != VK_SUCCESS) { return result; }
			}
			return createDeferredDescriptors(target);
		}
		return VK_SUCCESS;
	}

	/// @brief Builds one window's attachments, frame slots and post-processing chain from the shared device.
	VkResult ForwardRenderer::createTargetResources(WindowTarget &target) {
		VkResult result{VK_SUCCESS};
		int width{};
		int height{};
		SDL_GetWindowSizeInPixels(target.window, &width, &height);

		result = target.swapchain.create(
			physicalDevice.physicalDevice,
			device.device,
			target.surface.surface,
			*physicalDevice.graphicsQueueFamily,
			target.presentQueueFamily,
			static_cast<std::uint32_t>(width),
			static_cast<std::uint32_t>(height),
			defaultPresentMode(), target.guiWindow && device.swapchainMutableFormat);
		if (result != VK_SUCCESS) { return result; }

		result = target.imageViews.create(device.device, target.swapchain.images, target.swapchain.imageFormat, target.swapchain.guiFormat);
		if (result != VK_SUCCESS) { return result; }

		result = createTargetAttachments(target);
		if (result != VK_SUCCESS) { return result; }

		result = target.commandBuffers.create(device.device, commandPool.commandPool, framesInFlight);
		if (result != VK_SUCCESS) { return result; }
		nameTargetResources(target);

		result = target.frameSync.create(device.device, framesInFlight, static_cast<std::uint32_t>(target.swapchain.images.size()));
		if (result != VK_SUCCESS) { return result; }

		result = target.uniformBuffers.create(allocator, framesInFlight);
		if (result != VK_SUCCESS) { return result; }

		result = createSceneDescriptors(target);
		if (result != VK_SUCCESS) { return result; }

		// An additional window starts with both vertex slots pending for meshes already made dynamic.
		for (const auto &[handle, mesh] : meshes) {
			if (mesh.vertexBuffer.buffer == VK_NULL_HANDLE) { target.dynamicVertices[handle].second.set(); }
		}

		if (postProcessSetup_) {
			// The VVPPL throws, whereas the Engine works with std::expected and VKResult
			try {
				target.postProcess = std::make_unique<vvppl::PostProcessing>(device.device, physicalDevice.physicalDevice,
								target.swapchain.extent.width, target.swapchain.extent.height, framesInFlight);
				postProcessSetup_(*target.postProcess);
			} catch (const std::exception &) { return VK_ERROR_INITIALIZATION_FAILED; }
		}

		return VK_SUCCESS;
	}

	/// @brief Creates the shared forward/shadow pipelines with a matching specialized material descriptor array.
	VkResult ForwardRenderer::createScenePipelines(std::uint32_t textureCount) {
		for (auto &pipeline : aoPipelines) { pipeline.cleanup(); }
		aoLightingPipeline.cleanup();
		terrainGbufferPipeline.cleanup();
		terrainShadowPipeline.cleanup();
		terrainPipeline.cleanup();
		probePipeline.cleanup(); probeTerrainPipeline.cleanup(); probeIntegrationPipeline.cleanup();
		gbufferPipeline.cleanup();
		lightingPipeline.cleanup();
		deferredPipelineLayout.cleanup();
		deferredLayout.reset();
		graphicsPipeline.cleanup();
		transparentPipeline.cleanup();
		shadowPipeline.cleanup();
		pipelineLayout.cleanup();
		VkResult result = descriptorSetLayout.create(device.device, textureCount);
		if (result != VK_SUCCESS) { return result; }
		result = pipelineLayout.create(device.device, descriptorSetLayout.descriptorSetLayout);
		if (result != VK_SUCCESS) { return result; }
		const VulkanVertexInputDescription vertexInput{};
		result = graphicsPipeline.create(device.device, pipelineLayout.pipelineLayout, vertShaderModule.shaderModule, "vertexMain",
			fragShaderModule.shaderModule, vertexInput, hdrFormat, depthFormat, textureCount, false, sampleCount());
		if (result != VK_SUCCESS) { return result; }
		++forwardPipelineCreateCount_;
		// The blended variant is part of the same forward setup and is not counted separately.
		const bool gi = backend_ == RendererBackend::hybrid_gi;
		// GI transparency needs set 1; create its pipeline after the shared deferred layout below.
		result = gi ? VK_SUCCESS : transparentPipeline.create(device.device, pipelineLayout.pipelineLayout, vertShaderModule.shaderModule, "vertexMain",
			fragShaderModule.shaderModule, vertexInput, hdrFormat, depthFormat, textureCount, true, sampleCount());
		if (result != VK_SUCCESS) { return result; }
		if (gi) {
			result = giShadowVertex.create(device.device, simple_shadow_gi_vert_spv);
			if (result != VK_SUCCESS) { return result; }
			result = giShadowFragment.create(device.device, simple_shadow_gi_frag_spv);
			if (result != VK_SUCCESS) { return result; }
		}
		result = shadowPipeline.create(device.device, pipelineLayout.pipelineLayout,
			gi ? giShadowVertex.shaderModule : shadowShaderModule.shaderModule, gi ? "shadowMaterialVertexMain" : "shadowVertexMain",
			gi ? giShadowFragment.shaderModule : VK_NULL_HANDLE, vertexInput, VK_FORMAT_UNDEFINED, depthFormat, gi ? textureCount : 0U,
			false, VK_SAMPLE_COUNT_1_BIT, {}, gi ? "shadowFragmentMain" : "fragmentMain");
		if (result == VK_SUCCESS && device.tessellationShader) {
			// Terrain reuses the forward fragment stage; the domain stage supplies the same interpolants as vertexMain.
			result = terrainPipeline.create(device.device, pipelineLayout.pipelineLayout, terrainVertexShader.shaderModule, "terrainVertexMain",
				fragShaderModule.shaderModule, vertexInput, hdrFormat, depthFormat, textureCount, false, sampleCount(), {}, "fragmentMain", false, false,
				terrainHullShader.shaderModule, terrainDomainShader.shaderModule, "terrainDomainMain");
			if (result == VK_SUCCESS) {
				result = terrainShadowPipeline.create(device.device, pipelineLayout.pipelineLayout, terrainVertexShader.shaderModule, "terrainVertexMain",
					VK_NULL_HANDLE, vertexInput, VK_FORMAT_UNDEFINED, depthFormat, 0U, false, VK_SAMPLE_COUNT_1_BIT, {}, "fragmentMain", false, false,
					terrainHullShader.shaderModule, terrainShadowDomainShader.shaderModule, "terrainShadowDomainMain");
			}
		}
		if (result == VK_SUCCESS && usesDeferred()) { result = createDeferredPipelines(textureCount); }
		if (result == VK_SUCCESS && gi) {
			result = giTransparentShader.create(device.device, simple_forward_gi_frag_spv);
			if (result != VK_SUCCESS) { return result; }
			result = transparentPipeline.create(device.device, deferredPipelineLayout.pipelineLayout, vertShaderModule.shaderModule, "vertexMain",
				giTransparentShader.shaderModule, vertexInput, hdrFormat, depthFormat, textureCount, true, sampleCount(), {}, "fragmentMain", false, false,
				VK_NULL_HANDLE, VK_NULL_HANDLE, "terrainDomainMain", true);
		}
		if (result == VK_SUCCESS) { namePipelines(); }
		return result;
	}

	/// @brief Allocates and fills a window's two frame sets; called at startup or after an idle descriptor-array growth.
	VkResult ForwardRenderer::createSceneDescriptors(WindowTarget &target) {
		const auto count = static_cast<std::uint32_t>(objectTextures.size());
		target.descriptorSets.cleanup();
		VkResult result = target.descriptorPool.create(device.device, framesInFlight, descriptorSetLayout.bindings);
		if (result != VK_SUCCESS) { return result; }
		result = target.descriptorSets.create(device.device, target.descriptorPool.descriptorPool, descriptorSetLayout.descriptorSetLayout, framesInFlight, count);
		if (result != VK_SUCCESS) { return result; }
		// Fresh sets have no GPU users; initialize every slot before the first submission.
		target.materialsDirty.set();
		target.terrainDirty.set();
		target.shadowsDirty.set();
		for (auto &dirty : target.texturesDirty) { dirty.assign(count, true); }
		for (std::uint32_t frame{}; frame < framesInFlight; ++frame) {
			result = target.descriptorSets.writeUniformBuffer(frame, target.uniformBuffers.buffers[frame].buffer, sizeof(FrameUniforms));
			if (result != VK_SUCCESS) { return result; }
			result = writeShadowDescriptors(target, frame);
			if (result != VK_SUCCESS) { return result; }
			result = writeSceneDescriptors(target, frame);
			if (result != VK_SUCCESS) { return result; }
		}
		return VK_SUCCESS;
	}

	/// @brief Grows texture owners and descriptors geometrically within device limits, synchronizing only a layout change.
	VkResult ForwardRenderer::ensureTextureCapacity() {
		const auto needed = std::max<std::size_t>(1U, renderScene_ != nullptr ? renderScene_->textures().size() : 0U);
		if (needed <= objectTextures.size()) { return VK_SUCCESS; }
		if (needed > textureCapacity_) {
			textureUploadError_ = Error::capacity_exceeded;
			return VK_ERROR_INITIALIZATION_FAILED;
		}
		const auto count = static_cast<std::uint32_t>(std::min<std::size_t>(textureCapacity_, std::bit_ceil(needed)));
		// Old descriptor pools and pipelines may be used by any window; all must finish before the layout grows.
		if (!objectTextures.empty()) {
			const VkResult result = waitDeviceIdle();
			if (result != VK_SUCCESS) { return result; }
		}
		for (auto &target : targets) {
			target.descriptorSets.cleanup();
			target.descriptorPool.cleanup();
		}
		objectTextures.resize(count);
		uploadedTextureGenerations_.resize(count);
		VkResult result = createScenePipelines(count);
		if (result != VK_SUCCESS) { return result; }
		// The initial target has no uniform buffers yet; already-rendering windows adopt the grown range together.
		for (auto &target : targets) {
			if (target.uniformBuffers.buffers.empty()) { continue; }
			if (usesDeferred()) {
				result = createDeferredDescriptors(target);
				if (result != VK_SUCCESS) { return result; }
			}
			result = createSceneDescriptors(target);
			if (result != VK_SUCCESS) { return result; }
		}
		return VK_SUCCESS;
	}

	/// @brief Grows shadow arrays without destroying images or changing descriptors still used by submitted frames.
	VkResult ForwardRenderer::ensureShadowCapacity() {
		const std::array requirements{
			std::pair{&dirShadowArray, std::max(1U, static_cast<std::uint32_t>(frameUniforms_.activeDirectionalLightCount * kNumShadowCascades))},
			std::pair{&spotShadowArray, std::max(1U, frameUniforms_.activeSpotLightCount)},
			std::pair{&pointShadowArray, std::max(1U, static_cast<std::uint32_t>(frameUniforms_.activePointLightCount * pointShadowFaceCount))}};
		if (std::ranges::none_of(requirements, [](const auto &entry) { return entry.first->layerCount < entry.second; })) { return VK_SUCCESS; }
		// Unused layers keep this initial read-only layout; only shadow-casting lights clear and draw their layers.
		for (const auto &[map, layers] : requirements) {
			if (map->layerCount >= layers) { continue; }
			ShadowMap replacement{};
			VkResult result = replacement.create(allocator, device.device, layers);
			if (result != VK_SUCCESS) { return result; }
			result = submitOnce(device.device, device.graphicsQueue, commandPool.commandPool, [&](VkCommandBuffer commandBuffer) {
				transitionImage(commandBuffer, replacement.image, VK_IMAGE_ASPECT_DEPTH_BIT, 0U, layers,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			});
			if (result != VK_SUCCESS) { return result; }
			if (map->image != VK_NULL_HANDLE) { retire(std::move(*map)); }
			*map = std::move(replacement);
			nameShadowMap(*map);
			// Every target adopts the new arrays only after its own slot's fence has completed.
			for (auto &target : targets) { target.shadowsDirty.set(); }
		}
		return VK_SUCCESS;
	}

	/// @brief Binds the latest shadow arrays to a completed slot, leaving other submitted sets untouched.
	VkResult ForwardRenderer::writeShadowDescriptors(WindowTarget &target, std::uint32_t frame) {
		if (!target.shadowsDirty.test(frame)) { return VK_SUCCESS; }
		const std::array bindings{std::pair{&dirShadowArray, shaderBinding::dirShadowArray},
			std::pair{&spotShadowArray, shaderBinding::spotShadowArray}, std::pair{&pointShadowArray, shaderBinding::pointShadowArray}};
		for (const auto &[map, binding] : bindings) {
			const VkResult result = target.descriptorSets.writeShadowArray(frame, binding, map->imageView, map->shadowSampler);
			if (result != VK_SUCCESS) { return result; }
		}
		target.shadowsDirty.reset(frame);
		return VK_SUCCESS;
	}

	/// @brief Updates only changed texture elements and the material copy of a completed frame slot.
	VkResult ForwardRenderer::writeSceneDescriptors(WindowTarget &target, std::uint32_t frame) {
		// Dirty elements include releases, which must restore the white fallback before this set is submitted again.
		for (std::size_t index{}; index < objectTextures.size(); ++index) {
			if (!target.texturesDirty[frame][index]) { continue; }
			const TextureImage &texture = uploadedTextureGenerations_[index] != 0U ? objectTextures[index] : defaultObjectTexture;
			const std::array images{VkDescriptorImageInfo{.sampler = materialSampler, .imageView = texture.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
			const VkResult result = target.descriptorSets.writeObjectTextures(frame, images, static_cast<std::uint32_t>(index));
			if (result != VK_SUCCESS) { return result; }
			target.texturesDirty[frame][index] = false;
		}
		// Terrain heights change only when terrain objects are added or removed; an empty table keeps the unused binding unwritten.
		if (target.terrainDirty.test(frame) && !terrainHeights_.empty()) {
			auto &heights = target.terrainBuffers[frame];
			const auto heightBytes = terrainHeights_.size() * sizeof(float);
			if (heights.size < heightBytes) {
				VulkanBuffer replacement{};
				const VkResult created = replacement.create(allocator, heightBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, BufferMemory::upload);
				if (created != VK_SUCCESS) { return created; }
				if (heights.buffer != VK_NULL_HANDLE) { retire(std::move(heights)); }
				heights = std::move(replacement);
			}
			VkResult written = heights.upload(terrainHeights_.data(), heightBytes);
			if (written == VK_SUCCESS) { written = target.descriptorSets.writeTerrainBuffer(frame, heights.buffer, heights.size); }
			if (written != VK_SUCCESS) { return written; }
			target.terrainDirty.reset(frame);
		}
		if (!target.materialsDirty.test(frame)) { return VK_SUCCESS; }
		auto &buffer = target.materialBuffers[frame];
		const auto bytes = std::max<std::size_t>(materials_.size(), 1U) * sizeof(GpuMaterial);
		if (buffer.size < bytes) {
			VulkanBuffer replacement{};
			const VkResult result = replacement.create(allocator, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, BufferMemory::upload);
			if (result != VK_SUCCESS) { return result; }
			if (buffer.buffer != VK_NULL_HANDLE) { retire(std::move(buffer)); }
			buffer = std::move(replacement);
		}
		const GpuMaterial fallback{};
		VkResult result = materials_.empty() ? buffer.upload(&fallback, sizeof(fallback))
			: buffer.upload(materials_.data(), materials_.size() * sizeof(GpuMaterial));
		if (result != VK_SUCCESS) { return result; }
		result = target.descriptorSets.writeMaterialBuffer(frame, buffer.buffer, buffer.size);
		if (result != VK_SUCCESS) { return result; }
		target.materialsDirty.reset(frame);
		++materialUploadCount_;
		return VK_SUCCESS;
	}

	/// @brief Packs every terrain's heights once per set of terrain ids; each window slot uploads the table when it is free.
	void ForwardRenderer::refreshTerrainHeights() {
		std::vector<std::uint64_t> ids{};
		if (renderInstances_ != nullptr) {
			for (const RenderInstance &instance : *renderInstances_) {
				if (instance.terrain != nullptr) { ids.push_back(instance.terrain->id); }
			}
		}
		if (ids == terrainIds_) { return; }
		terrainIds_ = std::move(ids);
		terrainHeights_.clear();
		terrainOffsets_.clear();
		if (renderInstances_ != nullptr) {
			for (const RenderInstance &instance : *renderInstances_) {
				if (instance.terrain == nullptr || terrainOffsets_.contains(instance.terrain->id)) { continue; }
				terrainOffsets_.emplace(instance.terrain->id, static_cast<std::uint32_t>(terrainHeights_.size()));
				terrainHeights_.insert(terrainHeights_.end(), instance.terrain->heights.begin(), instance.terrain->heights.end());
			}
		}
		for (auto &target : targets) { target.terrainDirty.set(); }
	}

	/**
	 * @brief Rebuilds the dense CPU material table; each window uploads it when its frame slot is free.
	 *
	 * Texture indices outside the bound descriptor array become the shared no-texture sentinel.
	 */
	void ForwardRenderer::rebuildSceneMaterials() {
		materials_.clear();
		materialSlots_.clear();
		if (renderMaterials_ != nullptr) {
			materials_.reserve(renderMaterials_->size());
			for (const RenderMaterial &material : *renderMaterials_) {
				const auto textureIndex = [this](RenderTextureIndex index) {
					return index < objectTextures.size() && uploadedTextureGenerations_[index] != 0U ? index : kNoTexture;
				};
				const auto slot = static_cast<std::uint32_t>(materials_.size());
				materialSlots_.emplace(material.handle, slot);
				materials_.push_back(GpuMaterial{
					.baseColorFactor = Vec4{material.base_color.value.x, material.base_color.value.y,
						material.base_color.value.z, material.opacity},
					.emissiveFactor = Vec4{material.emissive.value.x, material.emissive.value.y, material.emissive.value.z,
						static_cast<Scalar>(material.alpha_mode)},
					.baseColorTexture = textureIndex(material.base_color_texture_index),
					.normalTexture = textureIndex(material.normal_texture_index),
					.metalnessTexture = textureIndex(material.metalness_texture_index),
					.roughnessTexture = textureIndex(material.roughness_texture_index),
					.emissiveTexture = textureIndex(material.emissive_texture_index),
					.ambientOcclusionTexture = textureIndex(material.ambient_occlusion_texture_index),
					.roughnessFactor = material.roughness,
					.metalnessFactor = material.metalness, .alphaCutoff = material.alpha_cutoff,
					.sidedness = material.double_sided ? (*material.double_sided ? VVE_MATERIAL_DOUBLE_SIDED : VVE_MATERIAL_SINGLE_SIDED) : VVE_MATERIAL_LEGACY_SIDED});
			}
		}

		uploadedMaterialCount_ = materials_.size();
		for (auto &target : targets) { target.materialsDirty.set(); }
	}

	/**
	 * @brief Replaces changed texture generations and defers descriptor updates to each completed frame slot.
	 *
	 * A slot is re-uploaded only when its texture generation changed (new, reused, or released slot). Empty slots point at
	 * the opaque-white default texture so the whole shader array stays valid. Replaced images stay in the retirement queue
	 * until every submission that may sample them has completed. All new images upload in one batch; their generations
	 * become resident only after its fence completes, so failed preparation leaves the previous texture table intact.
	 * Released pixels are restored from their retained source only for changed slots; resident slots need no decode.
	 * @return VK_SUCCESS on upload, otherwise a Vulkan error; textureUploadError() preserves any source decode error.
	 */
	VkResult ForwardRenderer::uploadSceneTextures() {
		textureUploadError_.reset();
		constexpr std::array opaqueWhitePixel{std::byte{255U}, std::byte{255U}, std::byte{255U}, std::byte{255U}};
		std::vector<TextureImage> replacements(objectTextures.size());
		TextureImage fallback{};
		std::vector<bool> changed(objectTextures.size());
		const bool createFallback = defaultObjectTexture.imageView == VK_NULL_HANDLE;
		bool uploadNeeded = createFallback;
		// Releases change descriptors without needing an upload submission.
		for (std::size_t index{}; index < objectTextures.size(); ++index) {
			if (uploadedTextureGenerations_[index] == textureGeneration(index)) { continue; }
			changed[index] = true;
			uploadNeeded |= textureGeneration(index) != 0U;
		}
		VulkanUploadBatch batch{}; // Destroy unsubmitted commands before pending images on a preparation failure.
		if (uploadNeeded) {
			const VkResult result = batch.begin(device.device, device.graphicsQueue, commandPool.commandPool);
			if (result != VK_SUCCESS) { return result; }
		}
		if (createFallback) {
			const VkResult result = fallback.create(allocator, device.device, physicalDevice.physicalDevice, batch,
				std::span{opaqueWhitePixel}, VkExtent2D{.width = 1U, .height = 1U});
			if (result != VK_SUCCESS) { return result; }
		}
		// Record every changed image and its mip blits before the batch's only submit and fence wait.
		for (std::size_t index{}; index < objectTextures.size(); ++index) {
			if (!changed[index] || textureGeneration(index) == 0U) { continue; }
			const auto source = renderScene_->textureForUpload(static_cast<RenderTextureIndex>(index));
			if (!source) {
				textureUploadError_ = source.error(); // Preserve io_error across the VkResult boundary.
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			const RenderTexture &texture = **source;
			const VkResult result = replacements[index].create(allocator, device.device, physicalDevice.physicalDevice, batch,
				texture.rgba8, VkExtent2D{.width = texture.extent.width, .height = texture.extent.height},
				texture.linear ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB);
			if (result != VK_SUCCESS) { return result; }
		}
		if (uploadNeeded) {
			const VkResult result = batch.submit();
			if (result != VK_SUCCESS) { return result; }
			++textureUploadSubmitCount_;
		}
		if (createFallback) { defaultObjectTexture = std::move(fallback); }
		// Commit completed images while older submitted frames retain their original views through retirement.
		for (std::size_t index{}; index < objectTextures.size(); ++index) {
			if (!changed[index]) { continue; }
			if (objectTextures[index].image != VK_NULL_HANDLE) { retire(std::move(objectTextures[index])); }
			objectTextures[index] = std::move(replacements[index]);
			uploadedTextureGenerations_[index] = textureGeneration(index);
			// A pending set keeps its old view until its own fence completes, even in a skipped window.
			for (auto &target : targets) {
				for (auto &dirty : target.texturesDirty) { if (!dirty.empty()) { dirty[index] = true; } }
			}
			sceneMaterialsDirty_ = true;
		}

		uploadedTextureCount_ = static_cast<std::size_t>(std::ranges::count_if(uploadedTextureGenerations_,
			[](std::uint64_t generation) { return generation != 0U; }));
		return VK_SUCCESS;
	}

	/// @brief Returns the committed GPU generation, or zero for an unused or out-of-range slot.
	std::uint64_t ForwardRenderer::uploadedTextureGeneration(std::size_t index) const {
		return index < uploadedTextureGenerations_.size() ? uploadedTextureGenerations_[index] : 0U;
	}

	/// @brief Preserves decode failures that VkResult cannot express for RenderSystem and frame capture.
	std::optional<Error> ForwardRenderer::textureUploadError() const { return textureUploadError_; }

	/**
	 * @brief Synchronizes runtime CPU-scene topology and texture changes with Vulkan resources.
	 *
	 * @return VK_SUCCESS when GPU meshes and the shared object texture match the CPU scene.
	 */
	VkResult ForwardRenderer::syncSceneResources() {
		if (const VkResult result = ensureTextureCapacity(); result != VK_SUCCESS) { return result; }
		const bool textureChanged = defaultObjectTexture.imageView == VK_NULL_HANDLE || texturesOutOfDate();
		if (!sceneResourcesDirty_ && !sceneMaterialsDirty_ && !textureChanged) { return VK_SUCCESS; }
		if (device.device == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

		auto liveMeshes = std::set<RenderMeshHandle>{};
		if (renderInstances_ != nullptr) {
			for (const RenderInstance &instance : *renderInstances_) { liveMeshes.insert(instance.mesh); }
		}
		// Replaced owners retire at the last submitted serial instead of stalling all windows.
		// Materials store texture slots, so they are rebuilt whenever a slot changes.
		if (textureChanged) {
			if (const VkResult result = uploadSceneTextures(); result != VK_SUCCESS) { return result; }
			sceneMaterialsDirty_ = true;
		}
		if (sceneMaterialsDirty_) {
			rebuildSceneMaterials();
			sceneMaterialsDirty_ = false;
		}

		// Release only meshes with no live instance; stable handles keep every surviving buffer untouched.
		for (auto uploaded = meshes.begin(); uploaded != meshes.end();) {
			if (liveMeshes.contains(uploaded->first)) { ++uploaded; continue; }
			// Dynamic copies may still be bound by any target's pending frame slot.
			for (auto &target : targets) {
				const auto vertices = target.dynamicVertices.find(uploaded->first);
				if (vertices == target.dynamicVertices.end()) { continue; }
				for (auto &buffer : vertices->second.first) {
					if (buffer.buffer != VK_NULL_HANDLE) { retire(std::move(buffer)); }
				}
				target.dynamicVertices.erase(vertices);
			}
			retire(std::move(uploaded->second));
			uploaded = meshes.erase(uploaded);
		}
		auto newlyUploaded = std::set<RenderMeshHandle>{};
		for (const RenderMeshHandle handle : liveMeshes) {
			if (meshes.contains(handle)) { continue; }
			const auto *renderMesh = findRenderMesh(handle);
			if (renderMesh == nullptr) { return VK_ERROR_INITIALIZATION_FAILED; }
			auto [uploaded, _] = meshes.try_emplace(handle);
			const VkResult result = uploaded->second.create(allocator, device.device, device.graphicsQueue, commandPool.commandPool, *renderMesh);
			if (result != VK_SUCCESS) { meshes.erase(uploaded); return result; }
			newlyUploaded.insert(handle);
			++meshUploadCount_;
		}
		for (const RenderMeshHandle handle : sceneGeometryDirty_) {
			if (!liveMeshes.contains(handle) || newlyUploaded.contains(handle)) { continue; }
			const auto *renderMesh = findRenderMesh(handle);
			const auto uploaded = meshes.find(handle);
			if (renderMesh == nullptr || uploaded == meshes.end()) { return VK_ERROR_INITIALIZATION_FAILED; }
			// The original static vertices stop receiving writes; each window acquires independently fenced copies.
			if (uploaded->second.vertexBuffer.buffer != VK_NULL_HANDLE) { retire(std::move(uploaded->second.vertexBuffer)); }
			for (auto &target : targets) { target.dynamicVertices[handle].second.set(); }
		}
		sceneGeometryDirty_.clear();
		sceneResourcesDirty_ = false;
		return VK_SUCCESS;
	}

	/// @brief Uploads pending dynamic vertices only into the window slot whose fence has completed.
	VkResult ForwardRenderer::uploadDynamicVertices(WindowTarget &target, std::uint32_t frame) {
		for (auto &[handle, vertices] : target.dynamicVertices) {
			auto &[buffers, dirty] = vertices;
			if (!dirty.test(frame)) { continue; }
			const auto *mesh = findRenderMesh(handle);
			const auto uploaded = meshes.find(handle);
			if (mesh == nullptr || uploaded == meshes.end()) { return VK_ERROR_INITIALIZATION_FAILED; }
			auto &buffer = buffers[frame];
			if (buffer.buffer == VK_NULL_HANDLE) {
				const VkResult result = buffer.create(allocator, mesh->vertices->size() * sizeof(RenderVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, BufferMemory::upload);
				if (result != VK_SUCCESS) { return result; }
			}
			if (const VkResult result = uploaded->second.updateVertices(buffer, *mesh); result != VK_SUCCESS) { return result; }
			dirty.reset(frame);
			++meshUploadCount_;
		}
		return VK_SUCCESS;
	}

	/// @brief Reclaims old owners covered by a completed fence on the single graphics queue shared by all windows.
	void ForwardRenderer::collectRetiredResources(std::uint64_t completed) {
		completedSerial_ = std::max(completedSerial_, completed);
		// Queue submission order makes this fence cover every earlier graphics submission as well.
		while (!retiredResources_.empty() && retiredResources_.front().first <= completedSerial_) { retiredResources_.pop_front(); }
	}

	/// @brief Counts device-wide waits reserved for swapchain, teardown and explicit debug-readback boundaries.
	VkResult ForwardRenderer::waitDeviceIdle() {
		if (device.device == VK_NULL_HANDLE) { return VK_SUCCESS; }
		++deviceWaitIdleCount_;
		const VkResult result = vkDeviceWaitIdle(device.device);
		if (result == VK_SUCCESS) { collectRetiredResources(submitSerial_); }
		return result;
	}

	/**
	* @brief Releases Vulkan device resources in reverse creation order.
		*/
	void ForwardRenderer::cleanup() {
		(void)waitDeviceIdle();
		nativeDraw_ = {}; // Release application GPU owners before the idle device is destroyed.
		frameError_.reset();
		// Release every target before the device-wide resources its descriptors and commands reference.
		for (auto &target : targets) { target.cleanup(); }
		targets.clear();
		recordedPassOrder.clear();
		debugLabels.reset();
		lastShadowLayerPassCount_ = 0U;
		lastFrameDrawStats_ = {};
		for (auto &[_, mesh] : meshes) { mesh.cleanup(); }
		meshes.clear();
		skyMesh.cleanup();
		for (TextureImage &texture : objectTextures) { texture.cleanup(); }
		objectTextures.clear();
		uploadedTextureGenerations_.clear();
		textureCapacity_ = 0U;
		if (renderScene_ != nullptr) { (void)renderScene_->setTextureCapacity(kNoTexture); }
		defaultObjectTexture.cleanup();
		retiredResources_.clear();
		if (materialSampler != VK_NULL_HANDLE) { vkDestroySampler(device.device, materialSampler, nullptr); }
		materialSampler = VK_NULL_HANDLE;
		submitSerial_ = completedSerial_ = 0U;
		materials_.clear();
		materialSlots_.clear();
		if (imguiDescriptorPool_ != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(device.device, imguiDescriptorPool_, nullptr);
			imguiDescriptorPool_ = VK_NULL_HANDLE;
		}
		uploadedTextureCount_ = 0U;
		uploadedMaterialCount_ = 0U;
		sceneGeometryDirty_.clear();
		sceneResourcesDirty_ = true;
		sceneMaterialsDirty_ = true;
		shadowDepthReadback.cleanup();
		shadowDepthSamples.clear();
		commandPool.cleanup();
		terrainGbufferPipeline.cleanup();
		terrainShadowPipeline.cleanup();
		terrainPipeline.cleanup();
		terrainShadowDomainShader.cleanup();
		terrainDomainShader.cleanup();
		terrainHullShader.cleanup();
		terrainVertexShader.cleanup();
		terrainHeights_.clear();
		terrainOffsets_.clear();
		terrainIds_.clear();
		terrainWarned_ = false;
		probePipeline.cleanup(); probeTerrainPipeline.cleanup(); probeIntegrationPipeline.cleanup();
		gbufferPipeline.cleanup();
		lightingPipeline.cleanup();
		deferredPipelineLayout.cleanup();
		deferredLayout.reset();
		probeFragment.cleanup(); probeIntegration.cleanup();
		for (auto &pipeline : aoPipelines) { pipeline.cleanup(); }
		for (auto &shader : aoShaders) { shader.cleanup(); }
		aoLightingPipeline.cleanup(); aoLightingShader.cleanup();
		gbufferShader.cleanup();
		lightingShader.cleanup();
		lightingVertexShader.cleanup();
		graphicsPipeline.cleanup();
		transparentPipeline.cleanup();
		shadowPipeline.cleanup();
		shadowShaderModule.cleanup();
		giShadowVertex.cleanup(); giShadowFragment.cleanup(); giTransparentShader.cleanup();
		fragShaderModule.cleanup();
		vertShaderModule.cleanup();
		pipelineLayout.cleanup();
		descriptorSetLayout.cleanup();
		pointShadowArray.cleanup();
		spotShadowArray.cleanup();
		dirShadowArray.cleanup();
		if (allocator.allocator != VK_NULL_HANDLE) cleanupLiveAllocations = gpuAllocationCount(); // Observe leaks before destroying the allocator.
		allocator.cleanup();
		device.cleanup();
		instance.cleanup();
	}

	/// @brief Reads the target window's drawable pixel extent without changing its swapchain.
	VkExtent2D ForwardRenderer::currentWindowPixelExtent(const WindowTarget &target) const {
		if (target.window == nullptr) { return {}; }
		int width{};
		int height{};
		SDL_GetWindowSizeInPixels(target.window, &width, &height);
		return VkExtent2D{
			.width = static_cast<std::uint32_t>(std::max(width, 0)),
			.height = static_cast<std::uint32_t>(std::max(height, 0)),
		};
	}

	/// @brief Replaces size-dependent attachments and synchronization, retaining the graphics pipelines.
	VkResult ForwardRenderer::recreateSwapchain(WindowTarget &target, VkExtent2D requestedExtent) {
		if (requestedExtent.width == 0U || requestedExtent.height == 0U) { return VK_NOT_READY; }
		if (physicalDevice.physicalDevice == VK_NULL_HANDLE || device.device == VK_NULL_HANDLE ||
			 target.surface.surface == VK_NULL_HANDLE || !physicalDevice.graphicsQueueFamily ||
			 !physicalDevice.presentQueueFamily) {
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		VkResult result = waitDeviceIdle();
		if (result != VK_SUCCESS) { return result; }

		// A failed rebuild keeps the device alive and clears the requested extent, so the next frame tries again.
		const auto retryNextFrame = [&target](VkResult failed) {
			target.swapchain.requestedExtent = {};
			return failed;
		};

		// Release views before their images, but keep the old swapchain for the presentation handoff.
		target.deferredPool.cleanup();
		target.deferredSet = VK_NULL_HANDLE;
		for (auto &image : target.aoImages) { image.cleanup(); }
		target.aoGpuBytes = 0; target.aoInitialized = target.aoEnabled = false;
		target.gpuQueries.reset(); target.gpuMs.fill(std::nullopt); target.timingSerial = 0; target.aoGpuMs.reset();
		for (auto &written : target.timingWritten) written.reset();
		for (auto &image : target.gbufferImages) { image.cleanup(); }
		target.depthImage.cleanup();
		target.msaaImage.cleanup();
		target.hdrImage.cleanup();
		target.imageViews.cleanup();
		target.frameSync.cleanup();

		result = target.swapchain.create(physicalDevice.physicalDevice, device.device, target.surface.surface,
										  *physicalDevice.graphicsQueueFamily, target.presentQueueFamily,
										  requestedExtent.width, requestedExtent.height, defaultPresentMode(), target.guiWindow && device.swapchainMutableFormat, target.swapchain.swapchain);
		if (result != VK_SUCCESS) { return retryNextFrame(result); }

		result = target.imageViews.create(device.device, target.swapchain.images, target.swapchain.imageFormat, target.swapchain.guiFormat);
		if (result != VK_SUCCESS) { return retryNextFrame(result); }

		result = createTargetAttachments(target);
		if (result != VK_SUCCESS) { return retryNextFrame(result); }
		nameTargetResources(target); // The resized images are new objects.

		if (target.postProcess) {
			// The VVPPL throws, whereas the Engine works with std::expected and VKResult
			try {
				target.postProcess->resize(target.swapchain.extent.width, target.swapchain.extent.height);
			} catch (const std::exception &) { return retryNextFrame(VK_ERROR_OUT_OF_DEVICE_MEMORY); }
		}

		result = target.frameSync.create(device.device, framesInFlight, static_cast<std::uint32_t>(target.swapchain.images.size()));
		if (result != VK_SUCCESS) { return retryNextFrame(result); }

		target.currentFrame = 0U;
		target.lastRenderedImageIndex.reset();
		return VK_SUCCESS;
	}

	/// @brief Creates the dedicated Dear ImGui descriptor pool when the Vulkan device is available.
	void ForwardRenderer::createImguiDescriptorPool() {
		if (imguiDescriptorPool_ != VK_NULL_HANDLE || device.device == VK_NULL_HANDLE) { return; }

		constexpr std::uint32_t maxSets{16U}; // Small immediate-mode UI pool kept separate from renderer descriptors.
		const VkDescriptorPoolSize poolSize{
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = maxSets,
		};
		const VkDescriptorPoolCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
			.maxSets = maxSets,
			.poolSizeCount = 1U,
			.pPoolSizes = &poolSize,
		};

		if (vkCreateDescriptorPool(device.device, &createInfo, nullptr, &imguiDescriptorPool_) != VK_SUCCESS) {
			imguiDescriptorPool_ = VK_NULL_HANDLE;
		}
	}

	/// @brief Builds dormant Dear ImGui Vulkan backend data from the renderer-owned Vulkan objects.
	ImGui_ImplVulkan_InitInfo ForwardRenderer::makeImguiInitInfo() const {
		ImGui_ImplVulkan_InitInfo info{};
		if (targets.empty() || !targets.front().guiWindow) { return info; }
		const auto &target = targets.front();
		info.ApiVersion = VK_API_VERSION_1_3;
		info.Instance = instance.instance;
		info.PhysicalDevice = physicalDevice.physicalDevice;
		info.Device = device.device;
		info.QueueFamily = physicalDevice.graphicsQueueFamily.value_or(0U);
		info.Queue = device.graphicsQueue;
		info.DescriptorPool = imguiDescriptorPool_;
		info.RenderPass = VK_NULL_HANDLE;
		info.UseDynamicRendering = true;
		info.PipelineRenderingCreateInfo = VkPipelineRenderingCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1U,
			.pColorAttachmentFormats = &target.swapchain.guiFormat, ///< Match the GUI-only view; scene blits retain imageFormat.
		};
		info.MinImageCount = static_cast<std::uint32_t>(target.swapchain.images.size());
		info.ImageCount = static_cast<std::uint32_t>(target.swapchain.images.size());
		info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		return info;
	}

} // namespace vve::simple
