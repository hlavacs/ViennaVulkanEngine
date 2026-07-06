module;
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#define VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL_vulkan.h>
#ifdef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif

export module VEEngine.Simple.Vulkan:Shadow;
import :Device;
import :OwnedHandle;
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
		const VulkanOwnedHandle<vk::raii::Device, VkDevice> *ownedDevice{}; ///< Borrowed RAII device for owned child resources.
		VkDevice device{VK_NULL_HANDLE};                           ///< Borrowed Vulkan logical device used to destroy resources.
		VulkanOwnedHandle<vk::raii::Image, VkImage> image{};       ///< Owned D32 depth image handle.
		VulkanOwnedHandle<vk::raii::DeviceMemory, VkDeviceMemory> memory{}; ///< Owned device-local memory backing the depth image.
		VulkanOwnedHandle<vk::raii::ImageView, VkImageView> imageView{}; ///< Owned depth image view for single-layer or whole-array sampling use.
		std::vector<VulkanOwnedHandle<vk::raii::ImageView, VkImageView>> ownedLayerViews{}; ///< Owned per-layer 2D depth views.
		VulkanOwnedHandle<vk::raii::Sampler, VkSampler> shadowSampler{}; ///< Owned clamp sampler for later shadow-map reads.
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
		[[nodiscard]] VkResult create(VkPhysicalDevice physicalDevice, const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice, std::uint32_t requestedLayerCount = 1U) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE || owningDevice == VK_NULL_HANDLE || requestedLayerCount == 0U) { return VK_ERROR_INITIALIZATION_FAILED; }

			ownedDevice = &owningDevice;
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

			VkImage rawImage{VK_NULL_HANDLE};
			VkResult result = vkCreateImage(device, &imageInfo, nullptr, &rawImage);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			image.handle = vk::raii::Image{(*ownedDevice).handle, rawImage};

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

			VkDeviceMemory rawMemory{VK_NULL_HANDLE};
			result = vkAllocateMemory(device, &allocateInfo, nullptr, &rawMemory);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			memory.handle = vk::raii::DeviceMemory{(*ownedDevice).handle, rawMemory};

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

			VkImageView rawView{VK_NULL_HANDLE};
			result = vkCreateImageView(device, &viewInfo, nullptr, &rawView);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			imageView.handle = vk::raii::ImageView{(*ownedDevice).handle, rawView};

			// Multi-layer shadow maps expose one 2D view per layer for later per-light framebuffers.
			if (layerCount > 1U) {
				ownedLayerViews.reserve(layerCount);
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
					VkImageView rawLayerView{VK_NULL_HANDLE}; ///< Per-layer 2D view owned by ownedLayerViews after creation.
					result = vkCreateImageView(device, &layerViewInfo, nullptr, &rawLayerView);
					if (result != VK_SUCCESS) { cleanup(); return result; }
					ownedLayerViews.emplace_back().handle = vk::raii::ImageView{(*ownedDevice).handle, rawLayerView};
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

			VkSampler rawSampler{VK_NULL_HANDLE};
			result = vkCreateSampler(device, &samplerInfo, nullptr, &rawSampler);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			shadowSampler.handle = vk::raii::Sampler{(*ownedDevice).handle, rawSampler};

			if (layerCount > 1U) {
				return VK_SUCCESS;
			}
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
			* @brief Destroys the owned shadow sampler, depth image view, image, and memory.
			*/
		void cleanup() {
			cleanupPipeline();
			shadowSampler.reset();
			for (auto &layerView : ownedLayerViews) { layerView.reset(); }
			ownedLayerViews.clear();
			imageView.reset();
			image.reset();
			memory.reset();
			ownedDevice = nullptr;
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
		if (ownedDevice == nullptr || device == VK_NULL_HANDLE || setLayout == VK_NULL_HANDLE || vertexEntry.empty()) { return VK_ERROR_INITIALIZATION_FAILED; }

		VulkanShaderModule shadowVertexModule{}; // Temporary module is only needed while creating the pipeline.
		VkResult result = shadowVertexModule.create(*ownedDevice, shadowVertexSpirvPath);
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
		const VkPipelineRenderingCreateInfo renderingInfo{ ///< Depth-only dynamic-rendering attachment format.
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 0U,
			.pColorAttachmentFormats = nullptr,
			.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
		};
		const VkGraphicsPipelineCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &renderingInfo,
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
			.renderPass = VK_NULL_HANDLE,
			.subpass = 0U,
			.basePipelineHandle = VK_NULL_HANDLE,
		};

		result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1U, &createInfo, nullptr, &pipeline);
		if (result != VK_SUCCESS) { cleanupPipeline(); }
		return result;
	}

} // namespace vve::simple
