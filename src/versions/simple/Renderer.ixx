module;
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>

export module VEEngine.Simple.Renderer;
import std;
import VEEngine.Simple.Math;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

/**
	* @file
	* @brief Vulkan renderer skeleton for the simple forward renderer.
	*
	* Functional objects:
	* - Renderer owns the current CPU scene, swapchain image stack, depth attachment, unbound shadow map and shadow pipeline, optional object texture, default object texture, forward render pass, framebuffers, descriptor-set layout, pipeline layout, shader modules, graphics pipeline, command pool, frame command buffers, frame synchronization, per-frame uniform buffers, descriptor pool, per-frame descriptor sets, and uploaded per-object meshes needed before rendering.
	*/
export namespace vve::simple {

	/// @brief Minimal forward renderer owning Vulkan swapchain bring-up without draw state.
	struct Renderer {
		static constexpr std::uint32_t framesInFlight{2U}; ///< Number of independent frame command buffers to allocate.
		VulkanInstance instance{};             ///< Owned Vulkan instance wrapper.
		VulkanSurface surface{};               ///< Owned SDL-backed Vulkan surface wrapper.
		VulkanPhysicalDevice physicalDevice{}; ///< Selected borrowed Vulkan physical device wrapper.
		VulkanDevice device{};                 ///< Owned Vulkan logical device wrapper.
		VulkanSwapchain swapchain{};           ///< Owned swapchain wrapper for presentation images.
		VulkanImageViews imageViews{};         ///< Owned color image views for swapchain images.
		VulkanDepthImage depthImage{};         ///< Owned swapchain-sized depth attachment image and view.
		ShadowMap shadowMap{};                 ///< Owned unbound shadow-map image reserved for later shadow rendering.
		ShadowMap dirShadowMap{};              ///< Owned directional shadow-map image reserved for later shadow rendering.
		ShadowMap spotShadowMap{};             ///< Owned spot shadow-map image reserved for later shadow rendering.
		TextureImage objectTexture{};           ///< Owned optional base-color texture bound only when the loaded scene requests one.
		TextureImage defaultObjectTexture{};    ///< Owned opaque-white texture bound when the scene has no base-color texture.
		VulkanRenderPass renderPass{};         ///< Owned forward render pass for swapchain color output.
		VulkanFramebuffers framebuffers{};     ///< Owned swapchain framebuffers for render-pass attachments.
		VulkanDescriptorSetLayout descriptorSetLayout{}; ///< Owned frame-uniform and shadow-map descriptor-set layout.
		VulkanPipelineLayout pipelineLayout{}; ///< Owned graphics pipeline layout using the frame descriptor set.
		VulkanShaderModule vertShaderModule{}; ///< Owned forward vertex shader module.
		VulkanShaderModule fragShaderModule{}; ///< Owned forward fragment shader module.
		VulkanGraphicsPipeline graphicsPipeline{}; ///< Owned forward graphics pipeline for swapchain rendering.
		VulkanCommandPool commandPool{};       ///< Owned resettable command pool for the graphics queue family.
		VulkanCommandBuffers commandBuffers{}; ///< Owned primary command buffers, one for each frame in flight.
		VulkanFrameSync frameSync{};           ///< Owned per-frame semaphores and fences for rendering.
		VulkanUniformBuffers uniformBuffers{}; ///< Owned per-frame uniform buffers for camera and object data.
		VulkanDescriptorPool descriptorPool{}; ///< Owned descriptor pool for per-frame uniform and shadow-map descriptor sets.
		VulkanDescriptorSets descriptorSets{}; ///< Owned per-frame descriptor sets binding frame uniform buffers and the shadow map.
		std::vector<VulkanMesh> meshes{};      ///< Owned GPU meshes uploaded from the current scene objects.
		SDL_Window *window{nullptr};           ///< Borrowed SDL window used to create the Vulkan surface.
		Scene scene{}; ///< CPU scene data kept in STL containers until renderer upload exists.
		Vec3 cameraEye{zero(), static_cast<Scalar>(6.0), static_cast<Scalar>(9.0)}; ///< World-space camera position used for the frame view matrix.
		Vec3 cameraTarget{zero(), one(), zero()}; ///< World-space point looked at by the frame view matrix.
		std::optional<std::uint32_t> lastRenderedImageIndex{}; ///< Swapchain image index from the last acquired, rendered, and presented frame.
		std::optional<VkResult> lastReadbackCaptureResult{}; ///< Result from the optional in-frame readback capture.

		~Renderer() { cleanup(); }

		/**
			* @brief Initializes the Vulkan instance, device, swapchain, image views, depth attachment, shadow map, render pass, framebuffers, descriptor-set layout, pipeline layout, shader modules, graphics pipeline, command pool, command buffers, frame synchronization, per-frame uniform buffers, descriptor pool, per-frame descriptor sets, and uploaded per-object meshes.
			*
			* @param sdlWindow Borrowed SDL window that owns the native platform surface.
			* @return VK_SUCCESS after graphics-pipeline bring-up, otherwise the first failing Vulkan result.
			*/
		[[nodiscard]] VkResult init(SDL_Window *sdlWindow) {
			// CMake provides the binary shader directory so runtime loading follows the generated SPIR-V files.
			const std::string shaderDir{VVE_SIMPLE_SHADER_DIR};
			const std::string vertSpirvPath{shaderDir + "/simple_forward.vert.spv"};
			const std::string fragSpirvPath{shaderDir + "/simple_forward.frag.spv"};
			const std::string shadowVertSpirvPath{shaderDir + "/simple_forward.shadow.vert.spv"};
			const std::string dirShadowVertSpirvPath{shaderDir + "/simple_forward.dir_shadow.vert.spv"}; ///< Directional shadow vertex shader.
			const std::string spotShadowVertSpirvPath{shaderDir + "/simple_forward.spot_shadow.vert.spv"}; ///< Spot shadow vertex shader.

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
				static_cast<std::uint32_t>(height));
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = imageViews.create(device.device, swapchain.images, swapchain.imageFormat);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = depthImage.create(physicalDevice.physicalDevice, device.device, swapchain.extent);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = shadowMap.create(physicalDevice.physicalDevice, device.device);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = dirShadowMap.create(physicalDevice.physicalDevice, device.device);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = spotShadowMap.create(physicalDevice.physicalDevice, device.device);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = renderPass.create(device.device, swapchain.imageFormat);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = framebuffers.create(device.device, renderPass.renderPass, imageViews.views, swapchain.extent, depthImage.view);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = descriptorSetLayout.create(device.device);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = pipelineLayout.create(device.device, descriptorSetLayout.descriptorSetLayout);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			VulkanVertexInputDescription vertexInput{}; // Fixed mesh vertex layout shared by forward and shadow pipelines.
			result = shadowMap.createPipeline(descriptorSetLayout.descriptorSetLayout, shadowVertSpirvPath, "shadowVertexMain", vertexInput);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = dirShadowMap.createPipeline(descriptorSetLayout.descriptorSetLayout, dirShadowVertSpirvPath, "shadowVertexMainDir", vertexInput);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = spotShadowMap.createPipeline(descriptorSetLayout.descriptorSetLayout, spotShadowVertSpirvPath, "shadowVertexMainSpot", vertexInput);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = vertShaderModule.create(device.device, vertSpirvPath);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = fragShaderModule.create(device.device, fragSpirvPath);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = graphicsPipeline.create(
				device.device,
				renderPass.renderPass,
				pipelineLayout.pipelineLayout,
				vertShaderModule.shaderModule,
				fragShaderModule.shaderModule,
				vertexInput,
				swapchain.extent);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = commandPool.create(device.device, *physicalDevice.graphicsQueueFamily);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = commandBuffers.create(device.device, commandPool.commandPool, framesInFlight);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = frameSync.create(device.device, framesInFlight, static_cast<std::uint32_t>(swapchain.images.size()));
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = uniformBuffers.create(physicalDevice.physicalDevice, device.device, framesInFlight);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = descriptorPool.create(device.device, framesInFlight);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = descriptorSets.create(device.device, descriptorPool.descriptorPool, descriptorSetLayout.descriptorSetLayout, framesInFlight);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			// Upload the optional scene base-color texture once descriptor sets and upload commands exist.
			const bool hasObjectTexture{scene.baseColorTexture.has_value() && objectTexture.create(physicalDevice.physicalDevice, device.device, device.graphicsQueue, commandPool.commandPool, *scene.baseColorTexture) == VK_SUCCESS};
			if (!hasObjectTexture) {
				constexpr std::array opaqueWhitePixel{std::byte{255U}, std::byte{255U}, std::byte{255U}, std::byte{255U}}; // Valid fallback for unused texture descriptors.
				result = defaultObjectTexture.create(physicalDevice.physicalDevice, device.device, device.graphicsQueue, commandPool.commandPool, std::span{opaqueWhitePixel}, VkExtent2D{.width = 1U, .height = 1U});
				if (result != VK_SUCCESS) { cleanup(); return result; }
			}
			const TextureImage &descriptorTexture{hasObjectTexture ? objectTexture : defaultObjectTexture};

			// Bind each frame descriptor set to its matching uniform buffer, shadow maps, and object texture slot.
			for (std::uint32_t frame{}; frame < framesInFlight; ++frame) {
				result = descriptorSets.writeUniformBuffer(frame, uniformBuffers.buffers[frame].buffer, sizeof(FrameUniforms));
				if (result != VK_SUCCESS) { cleanup(); return result; }
				result = descriptorSets.writeShadowMap(frame, shadowMap.view, shadowMap.sampler);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				result = descriptorSets.writeDirShadowMap(frame, dirShadowMap.view, dirShadowMap.sampler);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				result = descriptorSets.writeSpotShadowMap(frame, spotShadowMap.view, spotShadowMap.sampler);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				result = descriptorSets.writeObjectTexture(frame, descriptorTexture);
				if (result != VK_SUCCESS) { cleanup(); return result; }
			}

			// Upload one GPU mesh for each object in the current CPU scene.
			for (const Object &object : scene.objects) {
				VulkanMesh &mesh = meshes.emplace_back();
				result = mesh.create(physicalDevice.physicalDevice, device.device, object.mesh);
				if (result != VK_SUCCESS) { cleanup(); return result; }
			}

			return VK_SUCCESS;
		}

		/**
			* @brief Replaces the current CPU scene before future renderer upload.
			*
			* @param nextScene Scene data prepared by the caller.
			*/
		void loadScene(Scene nextScene) { scene = std::move(nextScene); }

		/**
			* @brief Stores the camera eye and target used by future frame uniform updates.
			*
			* @param eye World-space camera position.
			* @param target World-space point the camera looks at.
			*/
		void setCamera(Vec3 eye, Vec3 target) {
			cameraEye = eye;
			cameraTarget = target;
		}

		/**
			* @brief Draws one swapchain frame through the per-frame synchronization objects.
			*/
		void drawFrame(VulkanReadback *readback = nullptr) {
			lastReadbackCaptureResult.reset();
			const auto windowExtent = currentWindowPixelExtent();
			if (windowExtent.width == 0U || windowExtent.height == 0U) { return; }
			if (windowExtent.width != swapchain.extent.width || windowExtent.height != swapchain.extent.height) {
				if (recreateSwapchain(windowExtent) != VK_SUCCESS) { return; }
			}

			const std::size_t frameCount{frameSync.inFlightFences.size()}; // Existing sync count defines frames in flight.
			if (frameCount == 0U || frameSync.imageAvailableSemaphores.size() < frameCount || frameSync.renderFinishedSemaphores.empty()) { return; }
			if (commandBuffers.commandBuffers.size() < frameCount || device.device == VK_NULL_HANDLE || swapchain.swapchain == VK_NULL_HANDLE) { return; }
			if (currentFrame >= frameCount) { currentFrame = 0U; }

			const VkFence inFlightFence{frameSync.inFlightFences[currentFrame]};
			const VkSemaphore imageAvailableSemaphore{frameSync.imageAvailableSemaphores[currentFrame]};
			if (inFlightFence == VK_NULL_HANDLE || imageAvailableSemaphore == VK_NULL_HANDLE) { return; }

			VkResult result = vkWaitForFences(device.device, 1U, &inFlightFence, VK_TRUE, UINT64_MAX);
			if (result != VK_SUCCESS) { return; }

			std::uint32_t imageIndex{};
			result = vkAcquireNextImageKHR(device.device, swapchain.swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
			if (result == VK_ERROR_OUT_OF_DATE_KHR) { (void)recreateSwapchain(currentWindowPixelExtent()); return; }
			if (result != VK_SUCCESS) { return; }
			if (imageIndex >= frameSync.renderFinishedSemaphores.size()) { return; }
			const VkSemaphore renderFinishedSemaphore{frameSync.renderFinishedSemaphores[imageIndex]}; // Present-wait semaphore follows the acquired swapchain image.
			if (renderFinishedSemaphore == VK_NULL_HANDLE) { return; }

			result = vkResetFences(device.device, 1U, &inFlightFence);
			if (result != VK_SUCCESS) { return; }

			const Scalar aspectRatio{swapchain.extent.height == 0U ? one() : static_cast<Scalar>(swapchain.extent.width) / static_cast<Scalar>(swapchain.extent.height)}; ///< Live swapchain aspect with a zero-height guard.
			const PointLight light{scene.pointLight};
			const DirectionalLight dirLight{scene.directionalLight}; ///< Directional light data uploaded for future shader use.
			const SpotLight spot{scene.spotLight}; ///< Spot light data uploaded for future shader use.
			const Vec3 dirLightDirection{normalize(dirLight.direction)}; ///< Normalized world-space direction keeps uniform packing stable.
			const Vec3 spotDirection{normalize(spot.direction)}; ///< Normalized world-space direction keeps uniform packing stable.
			const Vec3 lightCenter{zeroVec3()}; ///< Origin-centered debug scene framing.
			const Vec3 shadowSurfaceLightDir{normalize(subtract(light.position, lightCenter))}; ///< Point-light direction approximated by one shadow map.
			const Vec3 lightEye{light.position}; ///< Place the shadow camera at the point light for the current simple approximation.
			const Scalar lightExtent{static_cast<Scalar>(4.0)}; ///< Light-space half-size covers the 4x4 floor and tall cube.
			const Vec3 dirLightEye{subtract(lightCenter, scale(dirLightDirection, lightExtent))}; ///< Directional shadow camera aimed at the scene origin.
			const Scalar spotLightFov{std::clamp(spot.outerConeAngle.radians * static_cast<Scalar>(2), static_cast<Scalar>(0.001), static_cast<Scalar>(3.0))}; ///< Spot-light cone field of view.
			const Scalar spotLightFar{std::isfinite(spot.range.value) && spot.range.value > zero() ? spot.range.value : static_cast<Scalar>(0.001)}; ///< Positive spot-light far plane.
			const Mat4 spotLightViewProj{multiply(perspectiveVulkan(spotLightFov, one(), static_cast<Scalar>(0.1), spotLightFar), lookAt(spot.position, add(spot.position, spotDirection), Vec3{zero(), one(), zero()}))}; ///< Spot-light projection for future shadows.
			const FrameUniforms frameUniforms{ // Shared camera matrices keep the sample cubes inside Vulkan clip space.
				.view = lookAt(cameraEye, cameraTarget, Vec3{zero(), one(), zero()}),
				.projection = perspectiveVulkan(static_cast<Scalar>(0.7853981633974483), aspectRatio, static_cast<Scalar>(0.1), static_cast<Scalar>(100.0)),
				.lightViewProj = multiply(orthoVulkan(-lightExtent, lightExtent, -lightExtent, lightExtent, static_cast<Scalar>(0.1), static_cast<Scalar>(16.0)), lookAt(lightEye, lightCenter, Vec3{zero(), one(), zero()})),
				.dirLightViewProj = multiply(orthoVulkan(-lightExtent, lightExtent, -lightExtent, lightExtent, static_cast<Scalar>(0.1), static_cast<Scalar>(16.0)), lookAt(dirLightEye, lightCenter, Vec3{zero(), one(), zero()})),
				.spotLightViewProj = spotLightViewProj,
				.lightPositionRange = Vec4{light.position.x, light.position.y, light.position.z, light.range},
				.lightColorIntensity = Vec4{light.color.x, light.color.y, light.color.z, light.intensity},
				.lightShadowAmbient = Vec4{shadowSurfaceLightDir.x, shadowSurfaceLightDir.y, shadowSurfaceLightDir.z, light.ambient},
				.dirLightDirection = Vec4{dirLightDirection.x, dirLightDirection.y, dirLightDirection.z, zero()}, ///< Directional light vector with unused w.
				.dirLightColorIntensity = Vec4{dirLight.color.x, dirLight.color.y, dirLight.color.z, dirLight.intensity.value}, ///< Directional tint with intensity in w.
				.dirLightShadowAmbient = Vec4{zero(), zero(), zero(), dirLight.ambient}, ///< Directional ambient term packed in w.
				.spotLightPositionRange = Vec4{spot.position.x, spot.position.y, spot.position.z, spot.range.value}, ///< Spot xyz position with range in w.
				.spotLightColorIntensity = Vec4{spot.color.x, spot.color.y, spot.color.z, spot.intensity.value}, ///< Spot rgb color with intensity in w.
				.spotLightDirection = Vec4{spotDirection.x, spotDirection.y, spotDirection.z, zero()}, ///< Spot xyz direction with unused w.
				.spotLightConeAmbient = Vec4{std::cos(spot.innerConeAngle.radians), std::cos(spot.outerConeAngle.radians), zero(), spot.ambient}, ///< Spot x inner cosine, y outer cosine, z unused, w ambient.
			};
			result = uniformBuffers.update(currentFrame, frameUniforms);
			if (result != VK_SUCCESS) { return; }

			result = recordCommandBuffer(currentFrame, imageIndex);
			if (result != VK_SUCCESS) { return; }

			const VkPipelineStageFlags waitStage{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
			const VkSubmitInfo submitInfo{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = 1U,
				.pWaitSemaphores = &imageAvailableSemaphore,
				.pWaitDstStageMask = &waitStage,
				.commandBufferCount = 1U,
				.pCommandBuffers = &commandBuffers.commandBuffers[currentFrame],
				.signalSemaphoreCount = 1U,
				.pSignalSemaphores = &renderFinishedSemaphore,
			};
			result = vkQueueSubmit(device.graphicsQueue, 1U, &submitInfo, inFlightFence);
			if (result != VK_SUCCESS) { return; }
			if (readback != nullptr && imageIndex < swapchain.images.size()) {
				lastReadbackCaptureResult = readback->capture(swapchain.images[imageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			}

			const VkPresentInfoKHR presentInfo{
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.waitSemaphoreCount = 1U,
				.pWaitSemaphores = &renderFinishedSemaphore,
				.swapchainCount = 1U,
				.pSwapchains = &swapchain.swapchain,
				.pImageIndices = &imageIndex,
			};
			result = vkQueuePresentKHR(device.presentQueue, &presentInfo);
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
				(void)recreateSwapchain(currentWindowPixelExtent());
				return;
			}
			if (result != VK_SUCCESS) { return; }

			lastRenderedImageIndex = imageIndex;
			currentFrame = static_cast<std::uint32_t>((currentFrame + 1U) % frameCount);
		}

		/**
		* @brief Releases Vulkan device resources in reverse creation order.
			*/
		void cleanup() {
			if (device.device != VK_NULL_HANDLE) { (void)vkDeviceWaitIdle(device.device); }
			for (auto mesh = meshes.rbegin(); mesh != meshes.rend(); ++mesh) { mesh->cleanup(); }
			meshes.clear();
			objectTexture.cleanup();
			defaultObjectTexture.cleanup();
			descriptorSets.cleanup();
			descriptorPool.cleanup();
			uniformBuffers.cleanup();
			frameSync.cleanup();
			commandBuffers.cleanup();
			commandPool.cleanup();
			graphicsPipeline.cleanup();
			fragShaderModule.cleanup();
			vertShaderModule.cleanup();
			spotShadowMap.cleanupPipeline();
			dirShadowMap.cleanupPipeline();
			shadowMap.cleanupPipeline();
			pipelineLayout.cleanup();
			descriptorSetLayout.cleanup();
			framebuffers.cleanup();
			renderPass.cleanup();
			spotShadowMap.cleanup();
			dirShadowMap.cleanup();
			shadowMap.cleanup();
			depthImage.cleanup();
			imageViews.cleanup();
			swapchain.cleanup();
			device.cleanup();
			surface.cleanup();
			instance.cleanup();
			window = nullptr;
		}

	private:
		std::uint32_t currentFrame{0U}; ///< Index of the frame synchronization set used by the next draw.

		[[nodiscard]] VkExtent2D currentWindowPixelExtent() const {
			if (window == nullptr) { return {}; }
			int width{};
			int height{};
			SDL_GetWindowSizeInPixels(window, &width, &height);
			return VkExtent2D{
				.width = static_cast<std::uint32_t>(std::max(width, 0)),
				.height = static_cast<std::uint32_t>(std::max(height, 0)),
			};
		}

		[[nodiscard]] VkResult recreateSwapchain(VkExtent2D requestedExtent) {
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
			framebuffers.cleanup();
			renderPass.cleanup();
			depthImage.cleanup();
			imageViews.cleanup();
			frameSync.cleanup();
			swapchain.cleanup();

			result = swapchain.create(physicalDevice.physicalDevice, device.device, surface.surface,
											  *physicalDevice.graphicsQueueFamily, *physicalDevice.presentQueueFamily,
											  requestedExtent.width, requestedExtent.height);
			if (result != VK_SUCCESS) { return result; }

			result = imageViews.create(device.device, swapchain.images, swapchain.imageFormat);
			if (result != VK_SUCCESS) { return result; }

			result = depthImage.create(physicalDevice.physicalDevice, device.device, swapchain.extent);
			if (result != VK_SUCCESS) { return result; }

			result = renderPass.create(device.device, swapchain.imageFormat);
			if (result != VK_SUCCESS) { return result; }

			result = framebuffers.create(device.device, renderPass.renderPass, imageViews.views, swapchain.extent, depthImage.view);
			if (result != VK_SUCCESS) { return result; }

			VulkanVertexInputDescription vertexInput{};
			result = graphicsPipeline.create(device.device, renderPass.renderPass, pipelineLayout.pipelineLayout,
													 vertShaderModule.shaderModule, fragShaderModule.shaderModule, vertexInput,
													 swapchain.extent);
			if (result != VK_SUCCESS) { return result; }

			result = frameSync.create(device.device, framesInFlight, static_cast<std::uint32_t>(swapchain.images.size()));
			if (result != VK_SUCCESS) { return result; }

			currentFrame = 0U;
			lastRenderedImageIndex.reset();
			return VK_SUCCESS;
		}

		/**
			* @brief Records one forward render pass for a swapchain image into a frame command buffer.
			*
			* @param frameIndex Index selecting the per-frame command buffer and descriptor set.
			* @param imageIndex Index selecting the swapchain framebuffer.
			* @return VK_SUCCESS when command recording succeeds, otherwise the first failing Vulkan result.
			*/
		[[nodiscard]] VkResult recordCommandBuffer(std::uint32_t frameIndex, std::uint32_t imageIndex) {
			if (frameIndex >= commandBuffers.commandBuffers.size() || frameIndex >= descriptorSets.descriptorSets.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (imageIndex >= framebuffers.framebuffers.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (shadowMap.renderPass == VK_NULL_HANDLE || shadowMap.framebuffer == VK_NULL_HANDLE || shadowMap.pipeline == VK_NULL_HANDLE || shadowMap.pipelineLayout == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (dirShadowMap.renderPass == VK_NULL_HANDLE || dirShadowMap.framebuffer == VK_NULL_HANDLE || dirShadowMap.pipeline == VK_NULL_HANDLE || dirShadowMap.pipelineLayout == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (spotShadowMap.renderPass == VK_NULL_HANDLE || spotShadowMap.framebuffer == VK_NULL_HANDLE || spotShadowMap.pipeline == VK_NULL_HANDLE || spotShadowMap.pipelineLayout == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkCommandBuffer commandBuffer{commandBuffers.commandBuffers[frameIndex]};
			VkResult result = vkResetCommandBuffer(commandBuffer, 0U);
			if (result != VK_SUCCESS) { return result; }

			const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
			result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
			if (result != VK_SUCCESS) { return result; }

			const auto drawUploadedObjects = [&](VkPipelineLayout activePipelineLayout) {
				std::size_t objectIndex{}; // Meshes and scene objects share submission order.
				for (const VulkanMesh &mesh : meshes) {
					if (objectIndex >= scene.objects.size()) { break; }

					const VkBuffer vertexBuffers[]{mesh.vertexBuffer.buffer};
					const VkDeviceSize offsets[]{0U};
					const Object &object = scene.objects[objectIndex];
					const ObjectPushConstants pushConstants{.model = object.model, .useBaseColorTexture = object.useBaseColorTexture};
					vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, vertexBuffers, offsets);
					vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.buffer, 0U, VK_INDEX_TYPE_UINT32);
					vkCmdPushConstants(commandBuffer, activePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0U, sizeof(ObjectPushConstants), &pushConstants);
					vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1U, 0U, 0, 0U);
					++objectIndex;
				}
			};
			const VkClearValue shadowClear{.depthStencil = {.depth = 1.0F, .stencil = 0U}}; // Shadow depth starts at the far plane.
			const VkRenderPassBeginInfo shadowPassInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = shadowMap.renderPass,
				.framebuffer = shadowMap.framebuffer,
				.renderArea = {.offset = {0, 0}, .extent = {.width = ShadowMap::resolution, .height = ShadowMap::resolution}},
				.clearValueCount = 1U,
				.pClearValues = &shadowClear,
			};

			vkCmdBeginRenderPass(commandBuffer, &shadowPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowMap.pipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowMap.pipelineLayout, 0U, 1U, &descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
			drawUploadedObjects(shadowMap.pipelineLayout);
			vkCmdEndRenderPass(commandBuffer);

			const VkRenderPassBeginInfo dirShadowPassInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = dirShadowMap.renderPass,
				.framebuffer = dirShadowMap.framebuffer,
				.renderArea = {.offset = {0, 0}, .extent = {.width = ShadowMap::resolution, .height = ShadowMap::resolution}},
				.clearValueCount = 1U,
				.pClearValues = &shadowClear,
			};

			vkCmdBeginRenderPass(commandBuffer, &dirShadowPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, dirShadowMap.pipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, dirShadowMap.pipelineLayout, 0U, 1U, &descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
			drawUploadedObjects(dirShadowMap.pipelineLayout);
			vkCmdEndRenderPass(commandBuffer);

			const VkRenderPassBeginInfo spotShadowPassInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = spotShadowMap.renderPass,
				.framebuffer = spotShadowMap.framebuffer,
				.renderArea = {.offset = {0, 0}, .extent = {.width = ShadowMap::resolution, .height = ShadowMap::resolution}},
				.clearValueCount = 1U,
				.pClearValues = &shadowClear,
			};

			vkCmdBeginRenderPass(commandBuffer, &spotShadowPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spotShadowMap.pipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spotShadowMap.pipelineLayout, 0U, 1U, &descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
			drawUploadedObjects(spotShadowMap.pipelineLayout);
			vkCmdEndRenderPass(commandBuffer);

			constexpr std::array<float, 4U> skyBackgroundColor{0.45F, 0.70F, 1.00F, 1.00F}; // Sky background for the forward color pass.
			const std::array<VkClearValue, 2U> clearValues{{{.color = {.float32 = {skyBackgroundColor[0], skyBackgroundColor[1], skyBackgroundColor[2], skyBackgroundColor[3]}}}, {.depthStencil = {.depth = 1.0F, .stencil = 0U}}}}; // Index 1 clears depth to the far plane.
			const VkRenderPassBeginInfo renderPassInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = renderPass.renderPass,
				.framebuffer = framebuffers.framebuffers[imageIndex],
				.renderArea = {.offset = {0, 0}, .extent = swapchain.extent},
				.clearValueCount = static_cast<std::uint32_t>(clearValues.size()),
				.pClearValues = clearValues.data(),
			};

			vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.pipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.pipelineLayout, 0U, 1U, &descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
			drawUploadedObjects(pipelineLayout.pipelineLayout);

			vkCmdEndRenderPass(commandBuffer);
			result = vkEndCommandBuffer(commandBuffer);
			if (result != VK_SUCCESS) { return result; }

			return VK_SUCCESS;
		}
	};

} // namespace vve::simple
