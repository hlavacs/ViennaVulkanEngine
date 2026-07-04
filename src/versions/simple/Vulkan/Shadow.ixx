module;
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#define VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#ifdef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif

export module VEEngine.Simple.Vulkan:Shadow;
import :Device;
import :Pipeline;
import std;

/**
	* @file
	* @brief Vulkan shadow-map ownership for the simple forward renderer.
	*
	* Functional objects:
	* - ShadowMap owns one square sampled D32 depth image, optional array views, sampler, backing memory, and unused shadow pipeline.
	*/
export namespace vve::simple {

	/// @brief Minimal standalone shadow-map owner; array layers are reserved for later multi-light shadow targets.
	struct ShadowMap {
		static constexpr std::uint32_t resolution{1024U};          ///< Fixed square shadow-map side length in pixels.
		VkDevice device{VK_NULL_HANDLE};                           ///< Borrowed Vulkan logical device used to destroy resources.
		VkImage image{VK_NULL_HANDLE};                             ///< Owned D32 depth image handle.
		VkDeviceMemory memory{VK_NULL_HANDLE};                     ///< Owned device-local memory backing the depth image.
		VkImageView view{VK_NULL_HANDLE};                          ///< Owned depth image view for single-layer or whole-array sampling use.
		std::vector<VkImageView> layerViews{};                     ///< Owned per-layer 2D depth views for future array-layer framebuffers.
		std::vector<VkFramebuffer> layerFramebuffers{};            ///< Owned per-layer depth framebuffers for future multi-light shadow passes.
		VkSampler sampler{VK_NULL_HANDLE};                         ///< Owned clamp sampler for later shadow-map reads.
		VkRenderPass renderPass{VK_NULL_HANDLE};                   ///< Owned depth-only render pass compatible with the shadow image.
		VkFramebuffer framebuffer{VK_NULL_HANDLE};                 ///< Owned square framebuffer attaching the shadow depth view.
		VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};           ///< Owned layout for frame uniforms and model push constants.
		VkPipeline pipeline{VK_NULL_HANDLE};                       ///< Owned depth-only shadow graphics pipeline, currently unused.
		std::uint32_t layerCount{1U};                              ///< Number of array layers allocated in the owned depth image.

		ShadowMap() = default;
		ShadowMap(const ShadowMap &) = delete;
		ShadowMap &operator=(const ShadowMap &) = delete;

		/**
			* @brief Creates a square D32 depth image usable as a depth attachment and sampled image.
			*
			* @param physicalDevice Physical device used to query memory types.
			* @param owningDevice Logical device that owns the image, memory, view, and sampler.
			* @param requestedLayerCount Number of array layers to allocate, defaulting to one for existing callers.
			* @return VK_SUCCESS when the shadow-map resources are ready, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkPhysicalDevice physicalDevice, VkDevice owningDevice, std::uint32_t requestedLayerCount = 1U) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE || owningDevice == VK_NULL_HANDLE || requestedLayerCount == 0U) { return VK_ERROR_INITIALIZATION_FAILED; }

			device = owningDevice;
			layerCount = requestedLayerCount;
			/// @brief Depth image descriptor for a fixed-size sampled shadow attachment.
			const VkImageCreateInfo imageInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_D32_SFLOAT,
				.extent = {.width = resolution, .height = resolution, .depth = 1U},
				.mipLevels = 1U,
				.arrayLayers = layerCount,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			};

			VkResult result = vkCreateImage(device, &imageInfo, nullptr, &image);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			VkMemoryRequirements requirements{};
			vkGetImageMemoryRequirements(device, image, &requirements);
			const std::optional<std::uint32_t> memoryType = findMemoryType(
				physicalDevice,
				requirements.memoryTypeBits,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			);
			if (!memoryType.has_value()) { cleanup(); return VK_ERROR_FEATURE_NOT_PRESENT; }

			/// @brief Device-local allocation descriptor for the shadow-map image.
			const VkMemoryAllocateInfo allocateInfo{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = requirements.size,
				.memoryTypeIndex = *memoryType,
			};

			result = vkAllocateMemory(device, &allocateInfo, nullptr, &memory);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = vkBindImageMemory(device, image, memory, 0U);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			/// @brief Depth-only view descriptor for single-layer attachment use or whole-array sampling use.
			const VkImageViewCreateInfo viewInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = image,
				.viewType = layerCount == 1U ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY,
				.format = VK_FORMAT_D32_SFLOAT,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
					.baseMipLevel = 0U,
					.levelCount = 1U,
					.baseArrayLayer = 0U,
					.layerCount = layerCount,
				},
			};

			result = vkCreateImageView(device, &viewInfo, nullptr, &view);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			// Multi-layer shadow maps expose one 2D view per layer for later per-light framebuffers.
			if (layerCount > 1U) {
				layerViews.reserve(layerCount);
				for (std::uint32_t layer{}; layer < layerCount; ++layer) {
					const VkImageViewCreateInfo layerViewInfo{
						.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
						.image = image,
						.viewType = VK_IMAGE_VIEW_TYPE_2D,
						.format = VK_FORMAT_D32_SFLOAT,
						.subresourceRange = {
							.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
							.baseMipLevel = 0U,
							.levelCount = 1U,
							.baseArrayLayer = layer,
							.layerCount = 1U,
						},
					};
					VkImageView layerView{VK_NULL_HANDLE}; ///< Per-layer 2D view owned by layerViews after creation.
					result = vkCreateImageView(device, &layerViewInfo, nullptr, &layerView);
					if (result != VK_SUCCESS) { cleanup(); return result; }
					layerViews.push_back(layerView);
				}
			}

			/// @brief Clamp sampler descriptor for future depth sampling without comparison state.
			const VkSamplerCreateInfo samplerInfo{
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter = VK_FILTER_LINEAR,
				.minFilter = VK_FILTER_LINEAR,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
				.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.minLod = 0.0F,
				.maxLod = 1.0F,
			};

			result = vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			const VkAttachmentDescription attachment{
				.format = VK_FORMAT_D32_SFLOAT,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			const VkAttachmentReference depthReference{
				.attachment = 0U,
				.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			};
			const VkSubpassDescription subpass{
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.pDepthStencilAttachment = &depthReference,
			};
			/// @brief Shadow depth writes must be visible before the forward pass samples them.
			const std::array<VkSubpassDependency, 2U> dependencies{{
				{
					.srcSubpass = VK_SUBPASS_EXTERNAL,
					.dstSubpass = 0U,
					.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
					.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				},
				{
					.srcSubpass = 0U,
					.dstSubpass = VK_SUBPASS_EXTERNAL,
					.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
				},
			}};
			const VkRenderPassCreateInfo renderPassInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
				.attachmentCount = 1U,
				.pAttachments = &attachment,
				.subpassCount = 1U,
				.pSubpasses = &subpass,
				.dependencyCount = static_cast<std::uint32_t>(dependencies.size()),
				.pDependencies = dependencies.data(),
			};

			result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			// Multi-layer maps get one framebuffer per 2D layer view; the pipeline remains single-layer only for now.
			if (layerCount > 1U) {
				layerFramebuffers.reserve(layerViews.size());
				for (VkImageView layerView : layerViews) {
					const VkFramebufferCreateInfo layerFramebufferInfo{
						.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
						.renderPass = renderPass,
						.attachmentCount = 1U,
						.pAttachments = &layerView,
						.width = resolution,
						.height = resolution,
						.layers = 1U,
					};
					VkFramebuffer layerFramebuffer{VK_NULL_HANDLE}; ///< Per-layer framebuffer owned by layerFramebuffers after creation.
					result = vkCreateFramebuffer(device, &layerFramebufferInfo, nullptr, &layerFramebuffer);
					if (result != VK_SUCCESS) { cleanup(); return result; }
					layerFramebuffers.push_back(layerFramebuffer);
				}
				return VK_SUCCESS;
			}

			const VkFramebufferCreateInfo framebufferInfo{
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass = renderPass,
				.attachmentCount = 1U,
				.pAttachments = &view,
				.width = resolution,
				.height = resolution,
				.layers = 1U,
			};

			result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			return VK_SUCCESS;
		}

		/**
			* @brief Creates the unused depth-only graphics pipeline for future shadow rendering.
			*
			* @param setLayout Existing frame-uniform descriptor-set layout used as set 0.
			* @param shadowVertexSpirvPath Path to the shadow vertex SPIR-V file.
			* @param vertexEntry Entry point name contained in the shadow vertex SPIR-V module.
			* @param vertexInput Existing mesh vertex layout shared with the forward pipeline.
			* @return VK_SUCCESS when the shadow pipeline is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult createPipeline(VkDescriptorSetLayout setLayout, std::string_view shadowVertexSpirvPath, std::string_view vertexEntry, const VulkanVertexInputDescription &vertexInput);

		/**
			* @brief Destroys the shadow graphics pipeline and pipeline layout before render-pass teardown.
			*/
		void cleanupPipeline() {
			if (pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, pipeline, nullptr); }
			if (pipelineLayout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, pipelineLayout, nullptr); }
			pipeline = VK_NULL_HANDLE;
			pipelineLayout = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned shadow framebuffer, render pass, sampler, depth image view, image, and memory.
			*/
		void cleanup() {
			cleanupPipeline();
			if (framebuffer != VK_NULL_HANDLE) { vkDestroyFramebuffer(device, framebuffer, nullptr); }
			for (VkFramebuffer layerFramebuffer : layerFramebuffers) { vkDestroyFramebuffer(device, layerFramebuffer, nullptr); }
			if (renderPass != VK_NULL_HANDLE) { vkDestroyRenderPass(device, renderPass, nullptr); }
			if (sampler != VK_NULL_HANDLE) { vkDestroySampler(device, sampler, nullptr); }
			for (VkImageView layerView : layerViews) { vkDestroyImageView(device, layerView, nullptr); }
			if (view != VK_NULL_HANDLE) { vkDestroyImageView(device, view, nullptr); }
			if (image != VK_NULL_HANDLE) { vkDestroyImage(device, image, nullptr); }
			if (memory != VK_NULL_HANDLE) { vkFreeMemory(device, memory, nullptr); }
			framebuffer = VK_NULL_HANDLE;
			layerFramebuffers.clear();
			renderPass = VK_NULL_HANDLE;
			sampler = VK_NULL_HANDLE;
			layerViews.clear();
			view = VK_NULL_HANDLE;
			image = VK_NULL_HANDLE;
			memory = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
			layerCount = 1U;
		}

		/**
			* @brief Destroys the owned shadow-map resources on scope exit.
			*/
		~ShadowMap() { cleanup(); }
	};

	/**
		* @brief Creates the fixed-function shadow graphics pipeline for the depth-only shadow render pass.
		*
		* @param setLayout Existing frame-uniform descriptor-set layout used as set 0.
		* @param shadowVertexSpirvPath Path to the shadow vertex SPIR-V binary.
		* @param vertexInput Existing mesh vertex binding and attribute description.
		* @return VK_SUCCESS when the pipeline layout and pipeline are ready, otherwise a Vulkan error code.
		*/
	[[nodiscard]] VkResult ShadowMap::createPipeline(VkDescriptorSetLayout setLayout, std::string_view shadowVertexSpirvPath, std::string_view vertexEntry, const VulkanVertexInputDescription &vertexInput) {
		cleanupPipeline();
		if (device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || setLayout == VK_NULL_HANDLE || vertexEntry.empty()) { return VK_ERROR_INITIALIZATION_FAILED; }

		VulkanShaderModule shadowVertexModule{}; // Temporary module is only needed while creating the pipeline.
		VkResult result = shadowVertexModule.create(device, shadowVertexSpirvPath);
		if (result != VK_SUCCESS) { return result; }

		const VkPushConstantRange modelPushConstants{
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset = 0U,
			.size = sizeof(ObjectPushConstants),
		};
		const VkPipelineLayoutCreateInfo layoutInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 1U,
			.pSetLayouts = &setLayout,
			.pushConstantRangeCount = 1U,
			.pPushConstantRanges = &modelPushConstants,
		};

		result = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);
		if (result != VK_SUCCESS) { cleanupPipeline(); return result; }

		const VkPipelineShaderStageCreateInfo shaderStage{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shadowVertexModule.shaderModule,
			.pName = vertexEntry.data(),
		};

		const VkVertexInputBindingDescription &binding = vertexInput.binding;
		const auto &attributes = vertexInput.attributes;
		const VkPipelineVertexInputStateCreateInfo vertexInputState{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1U,
			.pVertexBindingDescriptions = &binding,
			.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size()),
			.pVertexAttributeDescriptions = attributes.data(),
		};
		const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE,
		};
		const VkExtent2D extent{.width = resolution, .height = resolution};
		const VkViewport viewport{
			.x = 0.0F,
			.y = 0.0F,
			.width = static_cast<float>(extent.width),
			.height = static_cast<float>(extent.height),
			.minDepth = 0.0F,
			.maxDepth = 1.0F,
		};
		const VkRect2D scissor{.offset = {.x = 0, .y = 0}, .extent = extent};
		const VkPipelineViewportStateCreateInfo viewportState{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1U,
			.pViewports = &viewport,
			.scissorCount = 1U,
			.pScissors = &scissor,
		};
		const VkPipelineRasterizationStateCreateInfo rasterizer{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_NONE,			///< Shadow pass renders all faces because orthoVulkan Y-flip inverts winding.
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable = VK_TRUE,
			.depthBiasConstantFactor = 0.0F,			///< Constant depth bias is removed for visible contact shadows.
			.depthBiasSlopeFactor = 0.0F,			///< Slope depth bias is removed for visible contact shadows.
			.lineWidth = 1.0F,
		};
		const VkPipelineMultisampleStateCreateInfo multisampling{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE,
		};
		const VkPipelineColorBlendStateCreateInfo colorBlending{ ///< No color attachments or fragment shader are used.
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable = VK_FALSE,
			.attachmentCount = 0U,
			.pAttachments = nullptr,
		};
		const VkPipelineDepthStencilStateCreateInfo depthStencil{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
		};
		const VkGraphicsPipelineCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = 1U,
			.pStages = &shaderStage,
			.pVertexInputState = &vertexInputState,
			.pInputAssemblyState = &inputAssembly,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizer,
			.pMultisampleState = &multisampling,
			.pDepthStencilState = &depthStencil,
			.pColorBlendState = &colorBlending,
			.pDynamicState = nullptr,
			.layout = pipelineLayout,
			.renderPass = renderPass,
			.subpass = 0U,
			.basePipelineHandle = VK_NULL_HANDLE,
		};

		result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1U, &createInfo, nullptr, &pipeline);
		if (result != VK_SUCCESS) { cleanupPipeline(); }
		return result;
	}

} // namespace vve::simple
