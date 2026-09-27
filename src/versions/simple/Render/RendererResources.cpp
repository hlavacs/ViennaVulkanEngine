module;
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>
#include <VVPPL.h>
#if __has_include(<backends/imgui_impl_vulkan.h>)
#include <backends/imgui_impl_vulkan.h>
#else
#include <imgui_impl_vulkan.h>
#endif

module VEEngine.Simple.Renderer;
import std;
import VEEngine.Simple.Types;
import VEEngine.Simple.RenderResources;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

/// @file
/// @brief ForwardRenderer Vulkan resource lifetime: bring-up, scene upload, swapchain rebuild, teardown, and ImGui wiring.

namespace vve::simple {
	// Format of the hdr offscreen color target the scene is rendered into.
	constexpr VkFormat hdrFormat{VK_FORMAT_R16G16B16A16_SFLOAT};

	/**
		* @brief Initializes the Vulkan instance, device, swapchain, image views, depth attachment, shadow map, render pass, framebuffers, descriptor-set layout, pipeline layout, shader modules, graphics pipeline, command pool, command buffers, frame synchronization, per-frame uniform buffers, descriptor pool, per-frame descriptor sets, and uploaded per-object meshes.
		*
		* @param sdlWindow Borrowed SDL window that owns the native platform surface.
		* @return VK_SUCCESS after graphics-pipeline bring-up, otherwise the first failing Vulkan result.
		*/
	VkResult ForwardRenderer::init(SDL_Window *sdlWindow) {
		// CMake provides the binary shader directory so runtime loading follows the generated SPIR-V files.
		const std::string shaderDir{VVE_SIMPLE_SHADER_DIR};
		const std::string vertSpirvPath{shaderDir + "/simple_forward.vert.spv"};
		const std::string fragSpirvPath{shaderDir + "/simple_forward.frag.spv"};
		const std::string shadowVertSpirvPath{shaderDir + "/simple_forward.shadow.vert.spv"};

		window = sdlWindow;
		if (window == nullptr) { return VK_ERROR_INITIALIZATION_FAILED; }

		VkResult result = instance.create();
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = surface.create(instance.instance, window);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = physicalDevice.select(instance.instance, surface.surface);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		VkPhysicalDeviceProperties deviceProperties{};
		vkGetPhysicalDeviceProperties(physicalDevice.physicalDevice, &deviceProperties);
		constexpr auto shadowSamplerCount = 3U; ///< Spot, directional, and point shadow arrays share the fragment stage.
		const auto requiredSamplerCount = static_cast<std::uint32_t>(kMaxSceneTextures) + shadowSamplerCount;
		if (deviceProperties.limits.maxPerStageDescriptorSamplers < requiredSamplerCount) {
			std::cerr << "[vve::simple] renderer init failed: maxPerStageDescriptorSamplers="
				<< deviceProperties.limits.maxPerStageDescriptorSamplers << " requires " << requiredSamplerCount << '\n';
			cleanup();
			return VK_ERROR_FEATURE_NOT_PRESENT;
		}

		result = device.create(physicalDevice);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = allocator.create(instance.instance, physicalDevice.physicalDevice, device.device,
													  std::min<std::uint32_t>(deviceProperties.apiVersion, VK_API_VERSION_1_3)); ///< VMA needs a version both instance and device support.
		if (result != VK_SUCCESS) { cleanup(); return result; }

		int width{};
		int height{};
		SDL_GetWindowSizeInPixels(window, &width, &height);

		result = swapchain.create(
			physicalDevice.physicalDevice,
			device.device,
			surface.surface,
			*physicalDevice.graphicsQueueFamily,
			*physicalDevice.presentQueueFamily,
			static_cast<std::uint32_t>(width),
			static_cast<std::uint32_t>(height),
			defaultPresentMode());
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = imageViews.create(device.device, swapchain.images, swapchain.imageFormat);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = depthImage.create(allocator, device.device, swapchain.extent, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = hdrImage.create(allocator, device.device, swapchain.extent, hdrFormat,
										 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = descriptorSetLayout.create(device.device);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		VulkanVertexInputDescription vertexInput{}; // Fixed mesh vertex layout shared by forward and shadow pipelines.

		constexpr std::uint32_t directionalShadowLayerCount{static_cast<std::uint32_t>(kMaxDirectionalLights * kNumShadowCascades)}; // Four cascades for every directional-light slot.
		result = dirShadowArray.create(allocator, device.device, directionalShadowLayerCount);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = spotShadowArray.create(allocator, device.device, kMaxShadowedSpotLights);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		constexpr std::uint32_t pointShadowArrayLayerCount{static_cast<std::uint32_t>(kMaxShadowedPointLights * 6U)}; // Six cubemap-style faces per shadowed point light.
		result = pointShadowArray.create(allocator, device.device, pointShadowArrayLayerCount);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = pipelineLayout.create(device.device, descriptorSetLayout.descriptorSetLayout);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = vertShaderModule.create(device.device, vertSpirvPath);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = fragShaderModule.create(device.device, fragSpirvPath);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = shadowShaderModule.create(device.device, shadowVertSpirvPath);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = graphicsPipeline.create(device.device, pipelineLayout.pipelineLayout, vertShaderModule.shaderModule, "vertexMain",
															  fragShaderModule.shaderModule, vertexInput, swapchain.extent, hdrFormat, depthFormat);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = shadowPipeline.create(device.device, pipelineLayout.pipelineLayout, shadowShaderModule.shaderModule, "shadowVertexMain",
															VK_NULL_HANDLE, vertexInput, VkExtent2D{.width = ShadowMap::resolution, .height = ShadowMap::resolution}, VK_FORMAT_UNDEFINED, depthFormat);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = commandPool.create(device.device, *physicalDevice.graphicsQueueFamily);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = shadowDepthReadback.create(allocator, device.device, device.graphicsQueue, commandPool.commandPool, VkExtent2D{.width = ShadowMap::resolution, .height = ShadowMap::resolution}, depthFormat);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = commandBuffers.create(device.device, commandPool.commandPool, framesInFlight);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = frameSync.create(device.device, framesInFlight, static_cast<std::uint32_t>(swapchain.images.size()));
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = uniformBuffers.create(allocator, framesInFlight);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		result = descriptorPool.create(device.device, framesInFlight);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		createImguiDescriptorPool();

		result = descriptorSets.create(device.device, descriptorPool.descriptorPool, descriptorSetLayout.descriptorSetLayout, framesInFlight);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		// Bind each frame descriptor set to its matching uniform buffer and shadow maps; textures follow below.
		for (std::uint32_t frame{}; frame < framesInFlight; ++frame) {
			result = descriptorSets.writeUniformBuffer(frame, uniformBuffers.buffers[frame].buffer, sizeof(FrameUniforms));
			if (result != VK_SUCCESS) { cleanup(); return result; }
			result = descriptorSets.writeShadowArray(frame, shaderBinding::spotShadowArray, spotShadowArray.imageView, spotShadowArray.shadowSampler);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			result = descriptorSets.writeShadowArray(frame, shaderBinding::dirShadowArray, dirShadowArray.imageView, dirShadowArray.shadowSampler);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			result = descriptorSets.writeShadowArray(frame, shaderBinding::pointShadowArray, pointShadowArray.imageView, pointShadowArray.shadowSampler);
			if (result != VK_SUCCESS) { cleanup(); return result; }
		}

		result = uploadSceneTextures();
		if (result != VK_SUCCESS) { cleanup(); return result; }
		result = uploadSceneMaterials();
		if (result != VK_SUCCESS) { cleanup(); return result; }

		// Upload each referenced CPU mesh once, regardless of how many instances draw it.
		if (renderInstances_ != nullptr) {
			for (const RenderInstance &renderInstance : *renderInstances_) {
				if (meshes.contains(renderInstance.mesh)) { continue; }
				const auto *renderMesh = findRenderMesh(renderInstance.mesh);
				if (renderMesh == nullptr) { cleanup(); return VK_ERROR_INITIALIZATION_FAILED; }
				auto [uploaded, _] = meshes.try_emplace(renderInstance.mesh);
				result = uploaded->second.create(allocator, *renderMesh);
				if (result != VK_SUCCESS) { meshes.erase(uploaded); cleanup(); return result; }
				++meshUploadCount_;
			}
		}
		sceneGeometryDirty_.clear();
		sceneResourcesDirty_ = false;
		sceneRequiresFullUpload_ = false;
		sceneMaterialsDirty_ = false;

		if (postProcessSetup_) {
			// The VVPPL throws, whereas the Engine works with std::expected and VKResult
			try {
				postProcess = std::make_unique<vvppl::PostProcessing>(device.device, physicalDevice.physicalDevice,
								swapchain.extent.width, swapchain.extent.height, framesInFlight);
				postProcessSetup_(*postProcess);
			} catch (const std::exception &) { cleanup(); return VK_ERROR_INITIALIZATION_FAILED; }
		}

		return VK_SUCCESS;
	}

	/**
	 * @brief Rebuilds the dense material table and uploads it to one shared storage buffer.
	 *
	 * Texture indices outside the bound descriptor array become the shared no-texture sentinel.
	 * @return VK_SUCCESS when the current material table is uploaded and bound, otherwise a Vulkan error.
	 */
	VkResult ForwardRenderer::uploadSceneMaterials() {
		auto materials = std::vector<GpuMaterial>{};
		materialSlots_.clear();
		if (renderMaterials_ != nullptr) {
			materials.reserve(renderMaterials_->size());
			for (const RenderMaterial &material : *renderMaterials_) {
				const auto textureIndex = [this](RenderTextureIndex index) {
					return index < kMaxSceneTextures && index < scene.textures.size() ? index : kNoTexture;
				};
				const auto slot = static_cast<std::uint32_t>(materials.size());
				materialSlots_.emplace(material.handle, slot);
				materials.push_back(GpuMaterial{
					.baseColorFactor = Vec4{material.base_color.value.x, material.base_color.value.y,
						material.base_color.value.z, one()},
					.baseColorTexture = textureIndex(material.base_color_texture_index),
					.normalTexture = textureIndex(material.normal_texture_index),
					.metalnessTexture = textureIndex(material.metalness_texture_index),
					.roughnessTexture = textureIndex(material.roughness_texture_index),
					.emissiveTexture = textureIndex(material.emissive_texture_index),
					.ambientOcclusionTexture = textureIndex(material.ambient_occlusion_texture_index)});
			}
		}

		const std::size_t requiredCapacity = std::max<std::size_t>(materials.size(), 1U);
		const bool resized = materialBuffer.buffer == VK_NULL_HANDLE || materialBufferCapacity_ < requiredCapacity;
		if (resized) {
			materialBuffer.cleanup();
			const VkResult result = materialBuffer.create(allocator, requiredCapacity * sizeof(GpuMaterial),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
			if (result != VK_SUCCESS) { materialBufferCapacity_ = 0U; return result; }
			materialBufferCapacity_ = requiredCapacity;
		}

		const GpuMaterial fallback{};
		const VkResult upload = materials.empty()
			? materialBuffer.upload(std::addressof(fallback), sizeof(fallback))
			: materialBuffer.upload(materials.data(), materials.size() * sizeof(GpuMaterial));
		if (upload != VK_SUCCESS) { return upload; }
		if (resized) {
			for (std::uint32_t frame{}; frame < framesInFlight; ++frame) {
				const VkResult result = descriptorSets.writeMaterialBuffer(frame, materialBuffer.buffer,
					materialBufferCapacity_ * sizeof(GpuMaterial));
				if (result != VK_SUCCESS) { return result; }
			}
		}
		uploadedMaterialCount_ = materials.size();
		++materialUploadCount_;
		return VK_SUCCESS;
	}

	/**
	 * @brief Uploads new RenderScene texture-table entries and binds all slots in every frame descriptor set.
	 *
	 * Unused slots point at the opaque-white default texture so the whole shader array stays valid.
	 * @return VK_SUCCESS when all textures are resident and bound, otherwise the first Vulkan error.
	 */
	VkResult ForwardRenderer::uploadSceneTextures() {
		if (sceneRequiresFullUpload_) {
			for (TextureImage &texture : objectTextures) { texture.cleanup(); }
			uploadedTextureCount_ = 0U;
		}

		constexpr std::array opaqueWhitePixel{std::byte{255U}, std::byte{255U}, std::byte{255U}, std::byte{255U}};
		VkResult result{VK_SUCCESS};
		if (defaultObjectTexture.imageView == VK_NULL_HANDLE) {
			result = defaultObjectTexture.create(allocator, device.device, device.graphicsQueue, commandPool.commandPool,
				std::span{opaqueWhitePixel}, VkExtent2D{.width = 1U, .height = 1U});
			if (result != VK_SUCCESS) { return result; }
		}
		const std::size_t textureCount{std::min(scene.textures.size(), kMaxSceneTextures)};
		for (std::size_t index{uploadedTextureCount_}; index < textureCount; ++index) {
			const RenderTexture *texture = scene.textures[index];
			if (texture == nullptr) { return VK_ERROR_INITIALIZATION_FAILED; }
			result = objectTextures[index].create(allocator, device.device, device.graphicsQueue, commandPool.commandPool,
				texture->rgba8, VkExtent2D{.width = texture->extent.width, .height = texture->extent.height},
				texture->linear ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB);
			if (result != VK_SUCCESS) { return result; }
		}

		std::array<VkDescriptorImageInfo, kMaxSceneTextures> images{};
		for (std::size_t index{}; index < kMaxSceneTextures; ++index) {
			const TextureImage &texture = index < textureCount ? objectTextures[index] : defaultObjectTexture;
			images[index] = VkDescriptorImageInfo{.sampler = texture.textureSampler, .imageView = texture.imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
		}
		for (std::uint32_t frame{}; frame < framesInFlight; ++frame) {
			result = descriptorSets.writeObjectTextures(frame, images);
			if (result != VK_SUCCESS) { return result; }
		}
		uploadedTextureCount_ = textureCount;
		return VK_SUCCESS;
	}

	/**
	 * @brief Synchronizes runtime CPU-scene topology and texture changes with Vulkan resources.
	 *
	 * @return VK_SUCCESS when GPU meshes and the shared object texture match the CPU scene.
	 */
	VkResult ForwardRenderer::syncSceneResources() {
		if (!sceneResourcesDirty_ && !sceneMaterialsDirty_) { return VK_SUCCESS; }
		if (device.device == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

		const bool textureChanged = sceneRequiresFullUpload_ || defaultObjectTexture.imageView == VK_NULL_HANDLE ||
			std::min(scene.textures.size(), kMaxSceneTextures) != uploadedTextureCount_;
		auto liveMeshes = std::set<RenderMeshHandle>{};
		if (renderInstances_ != nullptr) {
			for (const RenderInstance &instance : *renderInstances_) { liveMeshes.insert(instance.mesh); }
		}
		const bool topologyChanged = meshes.size() != liveMeshes.size() ||
			std::ranges::any_of(liveMeshes, [this](RenderMeshHandle handle) { return !meshes.contains(handle); });
		const bool geometryChanged = !sceneGeometryDirty_.empty();
		if (sceneRequiresFullUpload_ || textureChanged || topologyChanged || geometryChanged || sceneMaterialsDirty_) {
			const VkResult idle = vkDeviceWaitIdle(device.device);
			if (idle != VK_SUCCESS) { return idle; }
		}

		// Descriptor images can be replaced only after in-flight frames stop referencing them.
		if (textureChanged) {
			if (const VkResult result = uploadSceneTextures(); result != VK_SUCCESS) { return result; }
		}
		if (sceneMaterialsDirty_) {
			if (const VkResult result = uploadSceneMaterials(); result != VK_SUCCESS) { return result; }
		}

		// Release only meshes with no live instance; stable handles keep every surviving buffer untouched.
		if (sceneRequiresFullUpload_) { meshes.clear(); }
		for (auto uploaded = meshes.begin(); uploaded != meshes.end();) {
			if (liveMeshes.contains(uploaded->first)) { ++uploaded; continue; }
			uploaded->second.cleanup();
			uploaded = meshes.erase(uploaded);
		}
		auto newlyUploaded = std::set<RenderMeshHandle>{};
		for (const RenderMeshHandle handle : liveMeshes) {
			if (meshes.contains(handle)) { continue; }
			const auto *renderMesh = findRenderMesh(handle);
			if (renderMesh == nullptr) { return VK_ERROR_INITIALIZATION_FAILED; }
			auto [uploaded, _] = meshes.try_emplace(handle);
			const VkResult result = uploaded->second.create(allocator, *renderMesh);
			if (result != VK_SUCCESS) { meshes.erase(uploaded); return result; }
			newlyUploaded.insert(handle);
			++meshUploadCount_;
		}
		for (const RenderMeshHandle handle : sceneGeometryDirty_) {
			if (!liveMeshes.contains(handle) || newlyUploaded.contains(handle)) { continue; }
			const auto *renderMesh = findRenderMesh(handle);
			const auto uploaded = meshes.find(handle);
			if (renderMesh == nullptr || uploaded == meshes.end()) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (const VkResult result = uploaded->second.updateVertices(*renderMesh); result != VK_SUCCESS) { return result; }
			++meshUploadCount_;
		}
		sceneGeometryDirty_.clear();
		sceneResourcesDirty_ = false;
		sceneRequiresFullUpload_ = false;
		sceneMaterialsDirty_ = false;
		return VK_SUCCESS;
	}

	/**
	* @brief Releases Vulkan device resources in reverse creation order.
		*/
	void ForwardRenderer::cleanup() {
		if (device.device != VK_NULL_HANDLE) { (void)vkDeviceWaitIdle(device.device); }
		recordedPassOrder.clear();
		for (auto &[_, mesh] : meshes) { mesh.cleanup(); }
		meshes.clear();
		for (TextureImage &texture : objectTextures) { texture.cleanup(); }
		defaultObjectTexture.cleanup();
		materialBuffer.cleanup();
		materialSlots_.clear();
		descriptorSets.cleanup();
		if (imguiDescriptorPool_ != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(device.device, imguiDescriptorPool_, nullptr);
			imguiDescriptorPool_ = VK_NULL_HANDLE;
		}
		postProcess.reset();
		descriptorPool.cleanup();
		uploadedTextureCount_ = 0U;
		uploadedMaterialCount_ = 0U;
		materialBufferCapacity_ = 0U;
		sceneGeometryDirty_.clear();
		sceneResourcesDirty_ = true;
		sceneRequiresFullUpload_ = true;
		sceneMaterialsDirty_ = true;
		uniformBuffers.cleanup();
		frameSync.cleanup();
		commandBuffers.cleanup();
		shadowDepthReadback.cleanup();
		shadowDepthSamples.clear();
		commandPool.cleanup();
		graphicsPipeline.cleanup();
		shadowPipeline.cleanup();
		shadowShaderModule.cleanup();
		fragShaderModule.cleanup();
		vertShaderModule.cleanup();
		pipelineLayout.cleanup();
		descriptorSetLayout.cleanup();
		pointShadowArray.cleanup();
		spotShadowArray.cleanup();
		dirShadowArray.cleanup();
		depthImage.cleanup();
		hdrImage.cleanup();
		imageViews.cleanup();
		swapchain.cleanup();
		allocator.cleanup();
		device.cleanup();
		surface.cleanup();
		instance.cleanup();
		window = nullptr;
	}

	VkExtent2D ForwardRenderer::currentWindowPixelExtent() const {
		if (window == nullptr) { return {}; }
		int width{};
		int height{};
		SDL_GetWindowSizeInPixels(window, &width, &height);
		return VkExtent2D{
			.width = static_cast<std::uint32_t>(std::max(width, 0)),
			.height = static_cast<std::uint32_t>(std::max(height, 0)),
		};
	}

	VkResult ForwardRenderer::recreateSwapchain(VkExtent2D requestedExtent) {
		if (requestedExtent.width == 0U || requestedExtent.height == 0U) { return VK_NOT_READY; }
		if (physicalDevice.physicalDevice == VK_NULL_HANDLE || device.device == VK_NULL_HANDLE ||
			 surface.surface == VK_NULL_HANDLE || !physicalDevice.graphicsQueueFamily ||
			 !physicalDevice.presentQueueFamily || vertShaderModule.shaderModule == VK_NULL_HANDLE ||
			 fragShaderModule.shaderModule == VK_NULL_HANDLE || pipelineLayout.pipelineLayout == VK_NULL_HANDLE) {
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		VkResult result = vkDeviceWaitIdle(device.device);
		if (result != VK_SUCCESS) { return result; }

		graphicsPipeline.cleanup();
		depthImage.cleanup();
		hdrImage.cleanup();
		imageViews.cleanup();
		frameSync.cleanup();
		swapchain.cleanup();

		result = swapchain.create(physicalDevice.physicalDevice, device.device, surface.surface,
										  *physicalDevice.graphicsQueueFamily, *physicalDevice.presentQueueFamily,
										  requestedExtent.width, requestedExtent.height, defaultPresentMode());
		if (result != VK_SUCCESS) { return result; }

		result = imageViews.create(device.device, swapchain.images, swapchain.imageFormat);
		if (result != VK_SUCCESS) { return result; }

		result = depthImage.create(allocator, device.device, swapchain.extent, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
		if (result != VK_SUCCESS) { return result; }

		result = hdrImage.create(allocator, device.device, swapchain.extent, hdrFormat,
										 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		if (postProcess) { postProcess->resize(swapchain.extent.width, swapchain.extent.height); }

		VulkanVertexInputDescription vertexInput{};
		result = graphicsPipeline.create(device.device, pipelineLayout.pipelineLayout, vertShaderModule.shaderModule, "vertexMain",
													  fragShaderModule.shaderModule, vertexInput, swapchain.extent, hdrFormat, depthFormat);
		if (result != VK_SUCCESS) { return result; }

		result = frameSync.create(device.device, framesInFlight, static_cast<std::uint32_t>(swapchain.images.size()));
		if (result != VK_SUCCESS) { return result; }

		currentFrame = 0U;
		lastRenderedImageIndex.reset();
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
		info.ApiVersion = VK_API_VERSION_1_4;
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
			.pColorAttachmentFormats = &swapchain.imageFormat,
		};
		info.MinImageCount = static_cast<std::uint32_t>(swapchain.images.size());
		info.ImageCount = static_cast<std::uint32_t>(swapchain.images.size());
		info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		return info;
	}

} // namespace vve::simple
