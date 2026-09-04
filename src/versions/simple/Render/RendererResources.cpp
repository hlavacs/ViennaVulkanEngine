module;
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>
#if __has_include(<backends/imgui_impl_vulkan.h>)
#include <backends/imgui_impl_vulkan.h>
#else
#include <imgui_impl_vulkan.h>
#endif

module VEEngine.Simple.Renderer;
import std;
import VEEngine.Simple.Types;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

/// @file
/// @brief ForwardRenderer Vulkan resource lifetime: bring-up, scene upload, swapchain rebuild, teardown, and ImGui wiring.

namespace vve::simple {

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

		result = device.create(physicalDevice);
		if (result != VK_SUCCESS) { cleanup(); return result; }

		VkPhysicalDeviceProperties deviceProperties{};
		vkGetPhysicalDeviceProperties(physicalDevice.physicalDevice, &deviceProperties);
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
															  fragShaderModule.shaderModule, vertexInput, swapchain.extent, swapchain.imageFormat, depthFormat);
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

		// Upload one GPU mesh for each object in the current CPU scene.
		for (const Object &object : scene.objects) {
			VulkanMesh &mesh = meshes.emplace_back();
			result = mesh.create(allocator, object.mesh);
			if (result != VK_SUCCESS) { cleanup(); return result; }
		}
		sceneGeometryDirty_.clear();
		sceneResourcesDirty_ = false;
		sceneRequiresFullUpload_ = false;

		return VK_SUCCESS;
	}

	/**
	 * @brief Uploads every Scene::textures entry into its texture slot and binds all slots in every frame descriptor set.
	 *
	 * Unused slots point at the opaque-white default texture so the whole shader array stays valid.
	 * @return VK_SUCCESS when all textures are resident and bound, otherwise the first Vulkan error.
	 */
	VkResult ForwardRenderer::uploadSceneTextures() {
		for (TextureImage &texture : objectTextures) { texture.cleanup(); }
		defaultObjectTexture.cleanup();
		uploadedTextures_.clear();

		constexpr std::array opaqueWhitePixel{std::byte{255U}, std::byte{255U}, std::byte{255U}, std::byte{255U}};
		VkResult result = defaultObjectTexture.create(allocator, device.device, device.graphicsQueue, commandPool.commandPool, std::span{opaqueWhitePixel}, VkExtent2D{.width = 1U, .height = 1U});
		if (result != VK_SUCCESS) { return result; }
		const std::size_t textureCount{std::min(scene.textures.size(), kMaxSceneTextures)};
		for (std::size_t index{}; index < textureCount; ++index) {
			result = objectTextures[index].create(allocator, device.device, device.graphicsQueue, commandPool.commandPool, scene.textures[index]);
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
		uploadedTextures_.assign(scene.textures.begin(), scene.textures.begin() + static_cast<std::ptrdiff_t>(textureCount));
		return VK_SUCCESS;
	}

	/**
	 * @brief Synchronizes runtime CPU-scene topology and texture changes with Vulkan resources.
	 *
	 * @return VK_SUCCESS when GPU meshes and the shared object texture match the CPU scene.
	 */
	VkResult ForwardRenderer::syncSceneResources() {
		if (!sceneResourcesDirty_) { return VK_SUCCESS; }
		if (device.device == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

		const bool textureChanged = defaultObjectTexture.imageView == VK_NULL_HANDLE ||
			!std::ranges::equal(scene.textures | std::views::take(kMaxSceneTextures), uploadedTextures_);
		const bool geometryChanged = !sceneGeometryDirty_.empty();
		if (sceneRequiresFullUpload_ || textureChanged || geometryChanged) {
			const VkResult idle = vkDeviceWaitIdle(device.device);
			if (idle != VK_SUCCESS) { return idle; }
		}

		// Descriptor images can be replaced only after in-flight frames stop referencing them.
		if (textureChanged) {
			if (const VkResult result = uploadSceneTextures(); result != VK_SUCCESS) { return result; }
		}

		// Additions upload only the new suffix; removals and scene replacement rebuild index alignment.
		const bool rebuildMeshes = sceneRequiresFullUpload_ ||
			meshes.size() > scene.objects.size();
		if (rebuildMeshes) {
			meshes.clear();
		}
		while (meshes.size() < scene.objects.size()) {
			const Object &object = scene.objects[meshes.size()];
			VulkanMesh &mesh = meshes.emplace_back();
			const VkResult result = mesh.create(allocator, object.mesh);
			if (result != VK_SUCCESS) {
				meshes.pop_back();
				return result;
			}
		}
		if (!rebuildMeshes) {
			for (const std::size_t index : sceneGeometryDirty_) {
				if (index >= meshes.size() || index >= scene.objects.size()) {
					return VK_ERROR_INITIALIZATION_FAILED;
				}
				const VkResult result = meshes[index].updateVertices(
					scene.objects[index].mesh);
				if (result != VK_SUCCESS) { return result; }
			}
		}
		sceneGeometryDirty_.clear();
		sceneResourcesDirty_ = false;
		sceneRequiresFullUpload_ = false;
		return VK_SUCCESS;
	}

	/**
	* @brief Releases Vulkan device resources in reverse creation order.
		*/
	void ForwardRenderer::cleanup() {
		if (device.device != VK_NULL_HANDLE) { (void)vkDeviceWaitIdle(device.device); }
		recordedPassOrder.clear();
		for (auto mesh = meshes.rbegin(); mesh != meshes.rend(); ++mesh) { mesh->cleanup(); }
		meshes.clear();
		for (TextureImage &texture : objectTextures) { texture.cleanup(); }
		defaultObjectTexture.cleanup();
		descriptorSets.cleanup();
		if (imguiDescriptorPool_ != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(device.device, imguiDescriptorPool_, nullptr);
			imguiDescriptorPool_ = VK_NULL_HANDLE;
		}
		descriptorPool.cleanup();
		uploadedTextures_.clear();
		sceneGeometryDirty_.clear();
		sceneResourcesDirty_ = true;
		sceneRequiresFullUpload_ = true;
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

		VulkanVertexInputDescription vertexInput{};
		result = graphicsPipeline.create(device.device, pipelineLayout.pipelineLayout, vertShaderModule.shaderModule, "vertexMain",
													  fragShaderModule.shaderModule, vertexInput, swapchain.extent, swapchain.imageFormat, depthFormat);
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
