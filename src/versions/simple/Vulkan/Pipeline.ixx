module;
#include <compare>
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#define VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL_vulkan.h>
#ifdef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif

export module VEEngine.Simple.Vulkan:Pipeline;
import :Device;
import :OwnedHandle;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Vector;
import std;
import VEEngine.Simple.Math;

/**
	* @file
	* @brief Vulkan pipeline objects for the simple forward renderer.
	*
	* Functional objects:
	* - ObjectPushConstants stores per-object draw data copied through Vulkan push constants.
	* - VulkanDescriptorSetLayout owns only VkDescriptorSetLayout creation and teardown for frame uniforms, shadow-map sampling, and object-texture sampling.
	* - VulkanVertexInputDescription stores the fixed Vertex binding and attribute layout for the forward pipeline.
	* - VulkanPipelineLayout owns only VkPipelineLayout creation and teardown for one descriptor set and model push constants.
	* - VulkanShaderModule owns only VkShaderModule creation from SPIR-V bytes and teardown.
	* - VulkanGraphicsPipeline owns only VkPipeline creation for the simple forward pass and teardown.
	*/
export namespace vve::simple {
	/// @brief Plain per-object push-constant data matching the Slang ObjectPushConstants block layout.
	struct ObjectPushConstants {
		Mat4 model{};                          ///< Object-local model matrix selected before each draw call.
		std::uint32_t baseColorTextureIndex{0xFFFFFFFFU}; ///< Slot in the base-color texture array, or 0xFFFFFFFF for an untextured object.
		std::uint32_t spotLightIndex{0U};      ///< Shadow pass spot-light matrix index selected before each draw call.
		std::uint32_t dirLightIndex{0U};       ///< Shadow pass directional-light matrix index selected before each draw call.
		std::uint32_t unlit{0U};               ///< Non-zero renders the object in its flat base color without any lighting.
	};

	/// @brief Minimal Vulkan descriptor-set-layout owner for frame uniforms, shadow maps, and one object texture; no pipeline layout is created here.
	struct VulkanDescriptorSetLayout {
		VulkanOwnedHandle<vk::raii::DescriptorSetLayout, VkDescriptorSetLayout> descriptorSetLayout{}; ///< Owned descriptor-set layout for set 0 frame uniforms, shadow maps, and object texture.

		VulkanDescriptorSetLayout() = default;
		VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout &) = delete;
		VulkanDescriptorSetLayout &operator=(const VulkanDescriptorSetLayout &) = delete;

		/**
			* @brief Creates set 0 from reflected shader bindings for frame uniforms, shadow maps, and one object texture.
			*
			* @param owningDevice Logical device that owns the created descriptor-set layout.
			* @param bindings Descriptor-set layout bindings reflected from the Slang shader contract.
			* @return VK_SUCCESS when the descriptor-set layout is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice, const Vector<VkDescriptorSetLayoutBinding> &bindings) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE || bindings.empty()) { return VK_ERROR_INITIALIZATION_FAILED; }
			std::vector<VkDescriptorSetLayoutBinding> contiguousBindings{};
			contiguousBindings.reserve(bindings.size());
			for (const auto &binding : bindings) { contiguousBindings.push_back(binding); } // Vulkan consumes one contiguous binding array.
			const VkDescriptorSetLayoutCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.bindingCount = static_cast<std::uint32_t>(contiguousBindings.size()),
				.pBindings = contiguousBindings.data(),
			};

			VkDescriptorSetLayout rawLayout{VK_NULL_HANDLE};
			const VkResult result = vkCreateDescriptorSetLayout(owningDevice, &createInfo, nullptr, &rawLayout);
			return descriptorSetLayout.assign(owningDevice.handle, result, rawLayout);
		}

		/**
			* @brief Releases the owned descriptor-set layout through its RAII wrapper.
			*/
		void cleanup() { descriptorSetLayout.reset(); }
	};

	/// @brief Plain vertex input layout value matching the simple forward renderer Vertex attributes.
	struct VulkanVertexInputDescription {
		VkVertexInputBindingDescription binding{
			.binding = 0U,
			.stride = sizeof(vve::simple::Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		}; ///< Binding 0 consumes one complete Vertex per input vertex.
		std::array<VkVertexInputAttributeDescription, 3> attributes{{
			{.location = 0U, .binding = 0U, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(vve::simple::Vertex, position)},
			{.location = 1U, .binding = 0U, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(vve::simple::Vertex, color)},
			{.location = 2U, .binding = 0U, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(vve::simple::Vertex, texCoord)},
		}}; ///< Position, color, and texture-coordinate attributes consumed by the vertex shader.
	};

	/// @brief Minimal Vulkan pipeline-layout owner; no graphics pipeline, commands, or sync are created here.
	struct VulkanPipelineLayout {
		VulkanOwnedHandle<vk::raii::PipelineLayout, VkPipelineLayout> pipelineLayout{}; ///< Owned pipeline layout for set 0 and model push constants.

		VulkanPipelineLayout() = default;
		VulkanPipelineLayout(const VulkanPipelineLayout &) = delete;
		VulkanPipelineLayout &operator=(const VulkanPipelineLayout &) = delete;

		/**
			* @brief Creates a pipeline layout with one descriptor set and one vertex-visible object push constant range.
			*
			* @param owningDevice Logical device that owns the created pipeline layout.
			* @param setLayout Descriptor-set layout used as set 0 by the graphics pipeline.
			* @return VK_SUCCESS when the pipeline layout is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice, VkDescriptorSetLayout setLayout) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE || setLayout == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkPushConstantRange modelPushConstants{
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				.offset = 0U,
				.size = sizeof(ObjectPushConstants),
			};
			const VkPipelineLayoutCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				.setLayoutCount = 1U,
				.pSetLayouts = &setLayout,
				.pushConstantRangeCount = 1U,
				.pPushConstantRanges = &modelPushConstants,
			};

			VkPipelineLayout rawLayout{VK_NULL_HANDLE};
			const VkResult result = vkCreatePipelineLayout(owningDevice, &createInfo, nullptr, &rawLayout);
			return pipelineLayout.assign(owningDevice.handle, result, rawLayout);
		}

		/**
			* @brief Releases the owned pipeline layout through its RAII wrapper.
			*/
		void cleanup() { pipelineLayout.reset(); }
	};

	/// @brief Minimal Vulkan shader-module owner; no layouts, pipelines, commands, or sync are created here.
	struct VulkanShaderModule {
		VulkanOwnedHandle<vk::raii::ShaderModule, VkShaderModule> shaderModule{}; ///< Owned shader module created from a SPIR-V binary.

		VulkanShaderModule() = default;
		VulkanShaderModule(const VulkanShaderModule &) = delete;
		VulkanShaderModule &operator=(const VulkanShaderModule &) = delete;

		/**
			* @brief Loads a SPIR-V binary and creates a Vulkan shader module.
			*
			* @param owningDevice Logical device that owns the created shader module.
			* @param spirvPath Path to a binary SPIR-V file whose size is a multiple of 32-bit words.
			* @return VK_SUCCESS when the shader module is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice, std::string_view spirvPath) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			auto file = std::ifstream{std::string{spirvPath}, std::ios::binary | std::ios::ate};
			if (!file.is_open()) { return VK_ERROR_INITIALIZATION_FAILED; }

			const auto fileSize = static_cast<std::streamoff>(file.tellg());
			if (fileSize <= 0) { return VK_ERROR_INITIALIZATION_FAILED; }

			const auto byteCount = static_cast<std::size_t>(fileSize);
			if (byteCount % sizeof(std::uint32_t) != 0U) { return VK_ERROR_INITIALIZATION_FAILED; }
			auto code = std::vector<std::uint32_t>(byteCount / sizeof(std::uint32_t)); // Stores aligned 32-bit SPIR-V words.
			file.seekg(0, std::ios::beg);
			file.read(reinterpret_cast<char *>(code.data()), static_cast<std::streamsize>(byteCount));
			if (!file) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkShaderModuleCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.codeSize = byteCount,
				.pCode = code.data(),
			};

			VkShaderModule rawShaderModule{};
			const VkResult result = vkCreateShaderModule(owningDevice, &createInfo, nullptr, &rawShaderModule);
			return shaderModule.assign(owningDevice.handle, result, rawShaderModule);
		}

		/**
			* @brief Releases the owned shader module through its RAII wrapper.
			*/
		void cleanup() { shaderModule.reset(); }

		/**
			* @brief Destroys the owned shader module on scope exit.
			*/
		~VulkanShaderModule() { cleanup(); }
	};

	/// @brief Minimal Vulkan graphics-pipeline owner for the simple forward color pass.
	struct VulkanGraphicsPipeline {
		VulkanOwnedHandle<vk::raii::Pipeline, VkPipeline> pipeline{}; ///< Owned graphics pipeline for the simple forward pass.

		VulkanGraphicsPipeline() = default;
		VulkanGraphicsPipeline(const VulkanGraphicsPipeline &) = delete;
		VulkanGraphicsPipeline &operator=(const VulkanGraphicsPipeline &) = delete;

		/**
			* @brief Creates the fixed-function graphics pipeline for the simple forward render pass.
			*
			* @param owningDevice Logical device that owns the created pipeline.
			* @param renderPass Borrowed render pass compatible with one color attachment.
			* @param pipelineLayout Borrowed pipeline layout used by the shader stages.
			* @param vertexModule Borrowed vertex shader module with entry point vertexMain.
			* @param fragmentModule Borrowed fragment shader module with entry point fragmentMain.
			* @param vertexInput Borrowed vertex binding and attribute description used by the pipeline.
			* @param extent Swapchain extent used for the static viewport and scissor.
			* @param colorAttachmentFormat Swapchain color format used only when dynamic rendering replaces a render pass.
			* @param depthAttachmentFormat Depth format used only when dynamic rendering replaces a render pass.
			* @return VK_SUCCESS when the graphics pipeline is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(
			const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice,
			VkRenderPass renderPass,
			VkPipelineLayout pipelineLayout,
			VkShaderModule vertexModule,
			VkShaderModule fragmentModule,
			const VulkanVertexInputDescription &vertexInput,
			VkExtent2D extent,
			VkFormat colorAttachmentFormat = VK_FORMAT_UNDEFINED,
			VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED
		) {
			cleanup();
			if (
				owningDevice == VK_NULL_HANDLE ||
				pipelineLayout == VK_NULL_HANDLE ||
				vertexModule == VK_NULL_HANDLE ||
				fragmentModule == VK_NULL_HANDLE
			) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			if (renderPass == VK_NULL_HANDLE && (colorAttachmentFormat == VK_FORMAT_UNDEFINED || depthAttachmentFormat == VK_FORMAT_UNDEFINED)) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			constexpr char vertexEntry[]{"vertexMain"};
			constexpr char fragmentEntry[]{"fragmentMain"};
			const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{{
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_SHADER_STAGE_VERTEX_BIT,
					.module = vertexModule,
					.pName = vertexEntry,
				},
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
					.module = fragmentModule,
					.pName = fragmentEntry,
				},
			}};

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
			const VkViewport viewport{
				.x = 0.0F,
				.y = 0.0F,
				.width = static_cast<float>(extent.width),
				.height = static_cast<float>(extent.height),
				.minDepth = 0.0F,
				.maxDepth = 1.0F,
			};
			const VkRect2D scissor{
				.offset = {.x = 0, .y = 0},
				.extent = extent,
			};
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
				.cullMode = VK_CULL_MODE_BACK_BIT,
				.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
				.depthBiasEnable = VK_FALSE,
				.lineWidth = 1.0F,
			};
			const VkPipelineMultisampleStateCreateInfo multisampling{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
				.sampleShadingEnable = VK_FALSE,
			};
			const VkPipelineColorBlendAttachmentState colorBlendAttachment{
				.blendEnable = VK_FALSE,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			};
			const VkPipelineColorBlendStateCreateInfo colorBlending{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
				.logicOpEnable = VK_FALSE,
				.attachmentCount = 1U,
				.pAttachments = &colorBlendAttachment,
			};
			const VkPipelineDepthStencilStateCreateInfo depthStencil{ ///< Enables nearest visible fragments to write depth.
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
				.depthTestEnable = VK_TRUE,
				.depthWriteEnable = VK_TRUE,
				.depthCompareOp = VK_COMPARE_OP_LESS,
				.depthBoundsTestEnable = VK_FALSE,
				.stencilTestEnable = VK_FALSE,
			};
			const auto createPipeline = [&](const void *nextInfo, VkRenderPass compatibleRenderPass) {
				const VkGraphicsPipelineCreateInfo createInfo{
					.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
					.pNext = nextInfo,
					.stageCount = static_cast<std::uint32_t>(shaderStages.size()),
					.pStages = shaderStages.data(),
					.pVertexInputState = &vertexInputState,
					.pInputAssemblyState = &inputAssembly,
					.pViewportState = &viewportState,
					.pRasterizationState = &rasterizer,
					.pMultisampleState = &multisampling,
					.pDepthStencilState = &depthStencil,
					.pColorBlendState = &colorBlending,
					.pDynamicState = nullptr,
					.layout = pipelineLayout,
					.renderPass = compatibleRenderPass,
					.subpass = 0U,
					.basePipelineHandle = VK_NULL_HANDLE,
				};

				VkPipeline rawPipeline{VK_NULL_HANDLE};
				const VkResult result = vkCreateGraphicsPipelines(owningDevice, VK_NULL_HANDLE, 1U, &createInfo, nullptr, &rawPipeline);
				return pipeline.assign(owningDevice.handle, result, rawPipeline);
			};

			if (renderPass != VK_NULL_HANDLE) { return createPipeline(nullptr, renderPass); }

			const VkPipelineRenderingCreateInfo renderingInfo{ ///< Dynamic rendering attachment formats replace render-pass compatibility.
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
				.colorAttachmentCount = 1U,
				.pColorAttachmentFormats = &colorAttachmentFormat,
				.depthAttachmentFormat = depthAttachmentFormat,
			};
			return createPipeline(&renderingInfo, VK_NULL_HANDLE);
		}

		/**
			* @brief Releases the owned graphics pipeline through its RAII wrapper.
			*/
		void cleanup() { pipeline.reset(); }
	};


} // namespace vve::simple
