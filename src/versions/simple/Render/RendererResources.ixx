module;
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>
#if __has_include(<backends/imgui_impl_vulkan.h>)
#include <backends/imgui_impl_vulkan.h>
#else
#include <imgui_impl_vulkan.h>
#endif

export module VEEngine.Simple.Renderer:RendererResources;
import std;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Shaders;
import VEEngine.Simple.Vulkan;

/**
	* @file
	* @brief Vulkan resource lifetime orchestration for the simple forward renderer.
	*
	* Functional objects:
	* - ForwardRendererResources initializes, recreates, and destroys swapchain-owned and frame-owned Vulkan renderer resources.
	*/
export namespace vve::simple {

	/// @brief CRTP mixin that owns ForwardRenderer Vulkan resource creation, swapchain rebuild, and teardown logic.
	template<typename Renderer>
	struct ForwardRendererResources {
		/**
			* @brief Initializes the Vulkan instance, device, swapchain, image views, depth attachment, shadow map, render pass, framebuffers, descriptor-set layout, pipeline layout, shader modules, graphics pipeline, command pool, command buffers, frame synchronization, per-frame uniform buffers, descriptor pool, per-frame descriptor sets, and uploaded per-object meshes.
			*
			* @param sdlWindow Borrowed SDL window that owns the native platform surface.
			* @return VK_SUCCESS after graphics-pipeline bring-up, otherwise the first failing Vulkan result.
			*/
		[[nodiscard]] VkResult init(SDL_Window *sdlWindow) {
			auto &renderer = static_cast<Renderer &>(*this);
			// CMake provides the binary shader directory so runtime loading follows the generated SPIR-V files.
			const std::string shaderDir{VVE_SIMPLE_SHADER_DIR};
			const std::string vertSpirvPath{shaderDir + "/simple_forward.vert.spv"};
			const std::string fragSpirvPath{shaderDir + "/simple_forward.frag.spv"};
			const std::string shadowVertSpirvPath{shaderDir + "/simple_forward.shadow.vert.spv"};
			const std::string dirShadowVertSpirvPath{shaderDir + "/simple_forward.dir_shadow.vert.spv"}; ///< Directional shadow vertex shader.
			const std::string spotShadowVertSpirvPath{shaderDir + "/simple_forward.spot_shadow.vert.spv"}; ///< Spot shadow vertex shader.
			const std::string pointShadowVertSpirvPath{shaderDir + "/simple_forward.point_shadow.vert.spv"}; ///< Point shadow vertex shader.

			renderer.window = sdlWindow;
			if (renderer.window == nullptr) { return VK_ERROR_INITIALIZATION_FAILED; }

			VkResult result = renderer.instance.create();
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.surface.create(renderer.instance.instance, renderer.window);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.physicalDevice.select(renderer.instance.instance, renderer.surface.surface);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.device.create(renderer.physicalDevice);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			int width{};
			int height{};
			SDL_GetWindowSizeInPixels(renderer.window, &width, &height);

			result = renderer.swapchain.create(
				renderer.physicalDevice.physicalDevice,
				renderer.device.device,
				renderer.surface.surface,
				*renderer.physicalDevice.graphicsQueueFamily,
				*renderer.physicalDevice.presentQueueFamily,
				static_cast<std::uint32_t>(width),
				static_cast<std::uint32_t>(height),
				Renderer::defaultPresentMode());
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.imageViews.create(renderer.device.device, renderer.swapchain.images, renderer.swapchain.imageFormat);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.depthImage.create(renderer.physicalDevice.physicalDevice, renderer.device.device, renderer.swapchain.extent);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			ShaderSystem shaderReflection{}; // Local reflection pass supplies the descriptor layout contract.
			Vector<std::string> forwardEntries{};
			forwardEntries.push_back("vertexMain");
			forwardEntries.push_back("fragmentMain");
			const auto shaderHandle = shaderReflection.compileAndReflect(std::filesystem::path{VVE_SIMPLE_SHADER_SOURCE}, std::move(forwardEntries));
			if (!shaderHandle) { renderer.cleanup(); return VK_ERROR_INITIALIZATION_FAILED; }
			const auto descriptorBindings = shaderReflection.descriptorSetLayoutBindings(*shaderHandle, 0U);
			if (!descriptorBindings) { renderer.cleanup(); return VK_ERROR_INITIALIZATION_FAILED; }
			const auto reflectedBindings = shaderReflection.reflectedBindings(*shaderHandle);
			if (!reflectedBindings) { renderer.cleanup(); return VK_ERROR_INITIALIZATION_FAILED; }
			constexpr std::string_view objectTextureParameterName{"baseColorTexture"}; // Slang parameter for optional object base color.
			const auto objectTextureBinding = std::ranges::find_if(*reflectedBindings, [objectTextureParameterName](const ShaderBindingReflection &binding) {
				return binding.set == 0U && binding.name == objectTextureParameterName && binding.category != "binding_range";
			});
			if (objectTextureBinding == reflectedBindings->end()) { renderer.cleanup(); return VK_ERROR_INITIALIZATION_FAILED; }
			constexpr std::array shadowSamplerParameterNames{"shadowMap", "spotShadowMap", "spotShadowArray", "dirShadowArray", "pointShadowArray"}; // Slang shadow sampler parameters.
			std::array<std::uint32_t, shadowSamplerParameterNames.size()> shadowSamplerBindings{};
			for (std::size_t index{}; index < shadowSamplerParameterNames.size(); ++index) {
				const auto shadowSamplerBinding = std::ranges::find_if(*reflectedBindings, [name = std::string_view{shadowSamplerParameterNames[index]}](const ShaderBindingReflection &binding) {
					return binding.set == 0U && binding.name == name && binding.category != "binding_range";
				});
				if (shadowSamplerBinding == reflectedBindings->end()) { renderer.cleanup(); return VK_ERROR_INITIALIZATION_FAILED; }
				shadowSamplerBindings[index] = shadowSamplerBinding->binding;
			}

			result = renderer.descriptorSetLayout.create(renderer.device.device, *descriptorBindings);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			VulkanVertexInputDescription vertexInput{}; // Fixed mesh vertex layout shared by forward and shadow pipelines.

			result = renderer.shadowMap.create(renderer.physicalDevice.physicalDevice, renderer.device.device);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.dirShadowArray.create(renderer.physicalDevice.physicalDevice, renderer.device.device, kMaxDirectionalLights);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.spotShadowMap.create(renderer.physicalDevice.physicalDevice, renderer.device.device);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.spotShadowArray.create(renderer.physicalDevice.physicalDevice, renderer.device.device, kMaxShadowedSpotLights);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			constexpr std::uint32_t pointShadowArrayLayerCount{static_cast<std::uint32_t>(kMaxShadowedPointLights * 6U)}; // Six cubemap-style faces per shadowed point light.
			result = renderer.pointShadowArray.create(renderer.physicalDevice.physicalDevice, renderer.device.device, pointShadowArrayLayerCount);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.pointShadowArray.createPipeline(renderer.descriptorSetLayout.descriptorSetLayout, pointShadowVertSpirvPath, "shadowVertexMainPoint", vertexInput);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.dirShadowArray.createPipeline(renderer.descriptorSetLayout.descriptorSetLayout, dirShadowVertSpirvPath, "shadowVertexMainDir", vertexInput);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.spotShadowArray.createPipeline(renderer.descriptorSetLayout.descriptorSetLayout, spotShadowVertSpirvPath, "shadowVertexMainSpot", vertexInput);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.pipelineLayout.create(renderer.device.device, renderer.descriptorSetLayout.descriptorSetLayout);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.shadowMap.createPipeline(renderer.descriptorSetLayout.descriptorSetLayout, shadowVertSpirvPath, "shadowVertexMain", vertexInput);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.spotShadowMap.createPipeline(renderer.descriptorSetLayout.descriptorSetLayout, spotShadowVertSpirvPath, "shadowVertexMainSpot", vertexInput);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.vertShaderModule.create(renderer.device.device, vertSpirvPath);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.fragShaderModule.create(renderer.device.device, fragSpirvPath);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.graphicsPipeline.create(
				renderer.device.device,
				VK_NULL_HANDLE,
				renderer.pipelineLayout.pipelineLayout,
				renderer.vertShaderModule.shaderModule,
				renderer.fragShaderModule.shaderModule,
				vertexInput,
				renderer.swapchain.extent,
				renderer.swapchain.imageFormat,
				VK_FORMAT_D32_SFLOAT);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.commandPool.create(renderer.device.device, *renderer.physicalDevice.graphicsQueueFamily);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.spotShadowDepthReadback.create(renderer.physicalDevice.physicalDevice, renderer.device.device, renderer.device.graphicsQueue, renderer.commandPool.commandPool, VkExtent2D{.width = ShadowMap::resolution, .height = ShadowMap::resolution});
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.dirShadowDepthReadback.create(renderer.physicalDevice.physicalDevice, renderer.device.device, renderer.device.graphicsQueue, renderer.commandPool.commandPool, VkExtent2D{.width = ShadowMap::resolution, .height = ShadowMap::resolution});
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.pointShadowDepthReadback.create(renderer.physicalDevice.physicalDevice, renderer.device.device, renderer.device.graphicsQueue, renderer.commandPool.commandPool, VkExtent2D{.width = ShadowMap::resolution, .height = ShadowMap::resolution});
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.commandBuffers.create(renderer.device.device, renderer.commandPool.commandPool, Renderer::framesInFlight);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.frameSync.create(renderer.device.device, Renderer::framesInFlight, static_cast<std::uint32_t>(renderer.swapchain.images.size()));
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.uniformBuffers.create(renderer.physicalDevice.physicalDevice, renderer.device.device, Renderer::framesInFlight);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			result = renderer.descriptorPool.create(renderer.device.device, Renderer::framesInFlight, *descriptorBindings);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			renderer.createImguiDescriptorPool();

			result = renderer.descriptorSets.create(renderer.device.device, renderer.descriptorPool.descriptorPool, renderer.descriptorSetLayout.descriptorSetLayout, *descriptorBindings, objectTextureBinding->binding, shadowSamplerBindings, Renderer::framesInFlight);
			if (result != VK_SUCCESS) { renderer.cleanup(); return result; }

			// Upload the optional scene base-color texture once descriptor sets and upload commands exist.
			const bool hasObjectTexture{renderer.scene.baseColorTexture.has_value() && renderer.objectTexture.create(renderer.physicalDevice.physicalDevice, renderer.device.device, renderer.device.graphicsQueue, renderer.commandPool.commandPool, *renderer.scene.baseColorTexture) == VK_SUCCESS};
			if (!hasObjectTexture) {
				constexpr std::array opaqueWhitePixel{std::byte{255U}, std::byte{255U}, std::byte{255U}, std::byte{255U}}; // Valid fallback for unused texture descriptors.
				result = renderer.defaultObjectTexture.create(renderer.physicalDevice.physicalDevice, renderer.device.device, renderer.device.graphicsQueue, renderer.commandPool.commandPool, std::span{opaqueWhitePixel}, VkExtent2D{.width = 1U, .height = 1U});
				if (result != VK_SUCCESS) { renderer.cleanup(); return result; }
			}
			const TextureImage &descriptorTexture{hasObjectTexture ? renderer.objectTexture : renderer.defaultObjectTexture};

			// Bind each frame descriptor set to its matching uniform buffer, shadow maps, and object texture slot.
			for (std::uint32_t frame{}; frame < Renderer::framesInFlight; ++frame) {
				result = renderer.descriptorSets.writeUniformBuffer(frame, renderer.uniformBuffers.buffers[frame].buffer, sizeof(FrameUniforms));
				if (result != VK_SUCCESS) { renderer.cleanup(); return result; }
				result = renderer.descriptorSets.writeShadowMap(frame, renderer.shadowMap.imageView, renderer.shadowMap.shadowSampler);
				if (result != VK_SUCCESS) { renderer.cleanup(); return result; }
				result = renderer.descriptorSets.writeDirShadowArray(frame, renderer.dirShadowArray.imageView, renderer.dirShadowArray.shadowSampler); // Bind directional shadow-array sampler.
				if (result != VK_SUCCESS) { renderer.cleanup(); return result; }
				result = renderer.descriptorSets.writeSpotShadowMap(frame, renderer.spotShadowMap.imageView, renderer.spotShadowMap.shadowSampler);
				if (result != VK_SUCCESS) { renderer.cleanup(); return result; }
				result = renderer.descriptorSets.writeSpotShadowArray(frame, renderer.spotShadowArray.imageView, renderer.spotShadowArray.shadowSampler);
				if (result != VK_SUCCESS) { renderer.cleanup(); return result; }
				result = renderer.descriptorSets.writePointShadowArray(frame, renderer.pointShadowArray.imageView, renderer.pointShadowArray.shadowSampler);
				if (result != VK_SUCCESS) { renderer.cleanup(); return result; }
				result = renderer.descriptorSets.writeObjectTexture(frame, descriptorTexture);
				if (result != VK_SUCCESS) { renderer.cleanup(); return result; }
			}

			// Upload one GPU mesh for each object in the current CPU scene.
			for (const Object &object : renderer.scene.objects) {
				VulkanMesh &mesh = renderer.meshes.emplace_back();
				result = mesh.create(renderer.physicalDevice.physicalDevice, renderer.device.device, object.mesh);
				if (result != VK_SUCCESS) { renderer.cleanup(); return result; }
			}

			return VK_SUCCESS;
		}

		/**
		* @brief Releases Vulkan device resources in reverse creation order.
			*/
		void cleanup() {
			auto &renderer = static_cast<Renderer &>(*this);
			if (renderer.device.device != VK_NULL_HANDLE) { (void)vkDeviceWaitIdle(renderer.device.device); }
			renderer.recordedPassOrder.clear();
			for (auto mesh = renderer.meshes.rbegin(); mesh != renderer.meshes.rend(); ++mesh) { mesh->cleanup(); }
			renderer.meshes.clear();
			renderer.objectTexture.cleanup();
			renderer.defaultObjectTexture.cleanup();
			renderer.descriptorSets.cleanup();
			if (renderer.imguiDescriptorPool_ != VK_NULL_HANDLE) {
				vkDestroyDescriptorPool(renderer.device.device, renderer.imguiDescriptorPool_, nullptr);
				renderer.imguiDescriptorPool_ = VK_NULL_HANDLE;
			}
			renderer.descriptorPool.cleanup();
			renderer.uniformBuffers.cleanup();
			renderer.frameSync.cleanup();
			renderer.commandBuffers.cleanup();
			renderer.pointShadowDepthReadback.cleanup();
			renderer.dirShadowDepthReadback.cleanup();
			renderer.spotShadowDepthReadback.cleanup();
			renderer.commandPool.cleanup();
			renderer.graphicsPipeline.cleanup();
			renderer.fragShaderModule.cleanup();
			renderer.vertShaderModule.cleanup();
			renderer.spotShadowMap.cleanupPipeline();
			renderer.shadowMap.cleanupPipeline();
			renderer.pipelineLayout.cleanup();
			renderer.descriptorSetLayout.cleanup();
			renderer.pointShadowArray.cleanup();
			renderer.spotShadowArray.cleanup();
			renderer.spotShadowMap.cleanup();
			renderer.dirShadowArray.cleanup();
			renderer.shadowMap.cleanup();
			renderer.depthImage.cleanup();
			renderer.imageViews.cleanup();
			renderer.swapchain.cleanup();
			renderer.device.cleanup();
			renderer.surface.cleanup();
			renderer.instance.cleanup();
			renderer.window = nullptr;
		}

		[[nodiscard]] VkExtent2D currentWindowPixelExtent() const {
			const auto &renderer = static_cast<const Renderer &>(*this);
			if (renderer.window == nullptr) { return {}; }
			int width{};
			int height{};
			SDL_GetWindowSizeInPixels(renderer.window, &width, &height);
			return VkExtent2D{
				.width = static_cast<std::uint32_t>(std::max(width, 0)),
				.height = static_cast<std::uint32_t>(std::max(height, 0)),
			};
		}

		[[nodiscard]] VkResult recreateSwapchain(VkExtent2D requestedExtent) {
			auto &renderer = static_cast<Renderer &>(*this);
			if (requestedExtent.width == 0U || requestedExtent.height == 0U) { return VK_NOT_READY; }
			if (renderer.physicalDevice.physicalDevice == VK_NULL_HANDLE || renderer.device.device == VK_NULL_HANDLE ||
				 renderer.surface.surface == VK_NULL_HANDLE || !renderer.physicalDevice.graphicsQueueFamily ||
				 !renderer.physicalDevice.presentQueueFamily || renderer.vertShaderModule.shaderModule == VK_NULL_HANDLE ||
				 renderer.fragShaderModule.shaderModule == VK_NULL_HANDLE || renderer.pipelineLayout.pipelineLayout == VK_NULL_HANDLE) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			VkResult result = vkDeviceWaitIdle(renderer.device.device);
			if (result != VK_SUCCESS) { return result; }

			renderer.graphicsPipeline.cleanup();
			renderer.depthImage.cleanup();
			renderer.imageViews.cleanup();
			renderer.frameSync.cleanup();
			renderer.swapchain.cleanup();

			result = renderer.swapchain.create(renderer.physicalDevice.physicalDevice, renderer.device.device, renderer.surface.surface,
											  *renderer.physicalDevice.graphicsQueueFamily, *renderer.physicalDevice.presentQueueFamily,
											  requestedExtent.width, requestedExtent.height, Renderer::defaultPresentMode());
			if (result != VK_SUCCESS) { return result; }

			result = renderer.imageViews.create(renderer.device.device, renderer.swapchain.images, renderer.swapchain.imageFormat);
			if (result != VK_SUCCESS) { return result; }

			result = renderer.depthImage.create(renderer.physicalDevice.physicalDevice, renderer.device.device, renderer.swapchain.extent);
			if (result != VK_SUCCESS) { return result; }

			VulkanVertexInputDescription vertexInput{};
			result = renderer.graphicsPipeline.create(renderer.device.device, VK_NULL_HANDLE, renderer.pipelineLayout.pipelineLayout,
													 renderer.vertShaderModule.shaderModule, renderer.fragShaderModule.shaderModule, vertexInput,
													 renderer.swapchain.extent, renderer.swapchain.imageFormat, VK_FORMAT_D32_SFLOAT);
			if (result != VK_SUCCESS) { return result; }

			result = renderer.frameSync.create(renderer.device.device, Renderer::framesInFlight, static_cast<std::uint32_t>(renderer.swapchain.images.size()));
			if (result != VK_SUCCESS) { return result; }

			renderer.currentFrame = 0U;
			renderer.lastRenderedImageIndex.reset();
			return VK_SUCCESS;
		}

		/// @brief Creates the dedicated Dear ImGui descriptor pool when the Vulkan device is available.
		void createImguiDescriptorPool() {
			auto &renderer = static_cast<Renderer &>(*this);
			if (renderer.imguiDescriptorPool_ != VK_NULL_HANDLE || renderer.device.device == VK_NULL_HANDLE) { return; }

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

			if (vkCreateDescriptorPool(renderer.device.device, &createInfo, nullptr, &renderer.imguiDescriptorPool_) != VK_SUCCESS) {
				renderer.imguiDescriptorPool_ = VK_NULL_HANDLE;
			}
		}

		/// @brief Builds dormant Dear ImGui Vulkan backend data from the renderer-owned Vulkan objects.
		[[nodiscard]] ImGui_ImplVulkan_InitInfo makeImguiInitInfo() const {
			const auto &renderer = static_cast<const Renderer &>(*this);
			ImGui_ImplVulkan_InitInfo info{};
			info.ApiVersion = VK_API_VERSION_1_4;
			info.Instance = renderer.instance.instance;
			info.PhysicalDevice = renderer.physicalDevice.physicalDevice;
			info.Device = renderer.device.device;
			info.QueueFamily = renderer.physicalDevice.graphicsQueueFamily.value_or(0U);
			info.Queue = renderer.device.graphicsQueue;
			info.DescriptorPool = renderer.imguiDescriptorPool_;
			info.RenderPass = VK_NULL_HANDLE;
			info.UseDynamicRendering = true;
			info.PipelineRenderingCreateInfo = VkPipelineRenderingCreateInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
				.colorAttachmentCount = 1U,
				.pColorAttachmentFormats = &renderer.swapchain.imageFormat,
			};
			info.MinImageCount = static_cast<std::uint32_t>(renderer.swapchain.images.size());
			info.ImageCount = static_cast<std::uint32_t>(renderer.swapchain.images.size());
			info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
			return info;
		}
	};

} // namespace vve::simple
