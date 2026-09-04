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
import VEEngine.Simple.Scene;
import VEEngine.Simple.Types;
import std;

/**
	* @file
	* @brief Vulkan pipeline objects for the simple forward renderer.
	*
	* Functional objects:
	* - ObjectPushConstants stores per-object draw data copied through Vulkan push constants.
	* - shaderBinding / kDescriptorSetBindings define the set-0 layout that simple_forward.slang must match.
	* - VulkanDescriptorSetLayout owns only VkDescriptorSetLayout creation and teardown for set 0.
	* - VulkanVertexInputDescription stores the fixed Vertex binding and attribute layout for the forward pipeline.
	* - VulkanPipelineLayout owns only VkPipelineLayout creation and teardown for one descriptor set and model push constants.
	* - VulkanShaderModule owns only VkShaderModule creation from SPIR-V bytes and teardown.
	* - VulkanGraphicsPipeline owns one dynamic-rendering VkPipeline: forward color+depth or depth-only shadow.
	*/
export namespace vve::simple {
	/// @brief Plain per-object push-constant data matching the Slang ObjectPushConstants block layout.
	struct ObjectPushConstants {
		Mat4 model{};                          ///< Object-local model matrix selected before each draw call.
		std::uint32_t baseColorTextureIndex{0xFFFFFFFFU}; ///< Slot in the base-color texture array, or 0xFFFFFFFF for an untextured object.
		std::uint32_t shadowMatrixIndex{0U};   ///< Index into FrameUniforms::shadowViewProjs used by the shadow vertex stage.
		std::uint32_t unlit{0U};               ///< Non-zero renders the object in its flat base color without any lighting.
		std::uint32_t padding{0U};             ///< Keeps the push-constant block a multiple of 16 bytes.
	};

	/// @brief Set-0 descriptor bindings of simple_forward.slang; every [[vk::binding(n, 0)]] in the shader must match this table.
	namespace shaderBinding {
		inline constexpr std::uint32_t frameUniforms{0U};      ///< ConstantBuffer<FrameUniforms> frame.
		inline constexpr std::uint32_t baseColorTextures{1U};  ///< Sampler2D baseColorTextures[kMaxSceneTextures].
		inline constexpr std::uint32_t spotShadowArray{2U};    ///< Sampler2DArrayShadow spotShadowArray.
		inline constexpr std::uint32_t dirShadowArray{3U};     ///< Sampler2DArrayShadow dirShadowArray.
		inline constexpr std::uint32_t pointShadowArray{4U};   ///< Sampler2DArrayShadow pointShadowArray.
	} // namespace shaderBinding

	/// @brief Descriptor-set layout of set 0, shared by the forward and shadow pipelines.
	inline constexpr std::array<VkDescriptorSetLayoutBinding, 5U> kDescriptorSetBindings{{
		{.binding = shaderBinding::frameUniforms, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1U, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
		{.binding = shaderBinding::baseColorTextures, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = static_cast<std::uint32_t>(kMaxSceneTextures), .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
		{.binding = shaderBinding::spotShadowArray, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1U, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
		{.binding = shaderBinding::dirShadowArray, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1U, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
		{.binding = shaderBinding::pointShadowArray, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1U, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
	}};

	/// @brief Minimal Vulkan descriptor-set-layout owner for frame uniforms, shadow maps, and one object texture; no pipeline layout is created here.
	struct VulkanDescriptorSetLayout {
		VulkanOwnedHandle<vk::raii::DescriptorSetLayout, VkDescriptorSetLayout> descriptorSetLayout{}; ///< Owned descriptor-set layout for set 0 frame uniforms, shadow maps, and object texture.

		VulkanDescriptorSetLayout() = default;
		VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout &) = delete;
		VulkanDescriptorSetLayout &operator=(const VulkanDescriptorSetLayout &) = delete;

		/**
			* @brief Creates set 0 from kDescriptorSetBindings.
			*
			* @param owningDevice Logical device that owns the created descriptor-set layout.
			* @return VK_SUCCESS when the descriptor-set layout is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }
			const VkDescriptorSetLayoutCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.bindingCount = static_cast<std::uint32_t>(kDescriptorSetBindings.size()),
				.pBindings = kDescriptorSetBindings.data(),
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

	/// @brief One dynamic-rendering graphics pipeline: forward color+depth when a fragment module is given, depth-only shadow pipeline otherwise.
	struct VulkanGraphicsPipeline {
		VulkanOwnedHandle<vk::raii::Pipeline, VkPipeline> pipeline{}; ///< Owned graphics pipeline.

		VulkanGraphicsPipeline() = default;
		VulkanGraphicsPipeline(const VulkanGraphicsPipeline &) = delete;
		VulkanGraphicsPipeline &operator=(const VulkanGraphicsPipeline &) = delete;

		/**
			* @brief Creates the fixed-function pipeline for the given attachments.
			*
			* @param owningDevice Logical device that owns the created pipeline.
			* @param pipelineLayout Borrowed pipeline layout used by the shader stages.
			* @param vertexModule Borrowed vertex shader module.
			* @param vertexEntry Vertex entry point name.
			* @param fragmentModule Borrowed fragment shader module (entry fragmentMain), or VK_NULL_HANDLE for a depth-only shadow pipeline.
			* @param vertexInput Borrowed vertex binding and attribute description used by the pipeline.
			* @param extent Attachment extent used for the static viewport and scissor.
			* @param colorAttachmentFormat Color format, VK_FORMAT_UNDEFINED for depth-only pipelines.
			* @param depthAttachmentFormat Depth format of the depth attachment.
			* @return VK_SUCCESS when the graphics pipeline is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice, VkPipelineLayout pipelineLayout, VkShaderModule vertexModule, const char *vertexEntry,
											VkShaderModule fragmentModule, const VulkanVertexInputDescription &vertexInput, VkExtent2D extent, VkFormat colorAttachmentFormat, VkFormat depthAttachmentFormat) {
			cleanup();
			const bool depthOnly{fragmentModule == VK_NULL_HANDLE};
			if (owningDevice == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE || vertexModule == VK_NULL_HANDLE || depthAttachmentFormat == VK_FORMAT_UNDEFINED ||
				 (!depthOnly && colorAttachmentFormat == VK_FORMAT_UNDEFINED)) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{{
				{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vertexModule, .pName = vertexEntry},
				{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fragmentModule, .pName = "fragmentMain"},
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
			const VkPipelineInputAssemblyStateCreateInfo inputAssembly{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
			const VkViewport viewport{.x = 0.0F, .y = 0.0F, .width = static_cast<float>(extent.width), .height = static_cast<float>(extent.height), .minDepth = 0.0F, .maxDepth = 1.0F};
			const VkRect2D scissor{.offset = {.x = 0, .y = 0}, .extent = extent};
			const VkPipelineViewportStateCreateInfo viewportState{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1U, .pViewports = &viewport, .scissorCount = 1U, .pScissors = &scissor};
			const VkPipelineRasterizationStateCreateInfo rasterizer{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				.polygonMode = VK_POLYGON_MODE_FILL,
				.cullMode = depthOnly ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT,	///< Shadow pass renders all faces because orthoVulkan Y-flip inverts winding.
				.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
				.depthBiasEnable = depthOnly ? VK_TRUE : VK_FALSE,
				.depthBiasConstantFactor = 1.25F,		///< Constant raster bias suppresses shadow depth quantization acne.
				.depthBiasSlopeFactor = 1.75F,			///< Slope raster bias protects surfaces viewed obliquely by the light.
				.lineWidth = 1.0F,
			};
			const VkPipelineMultisampleStateCreateInfo multisampling{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
			const VkPipelineColorBlendAttachmentState colorBlendAttachment{
				.blendEnable = VK_FALSE,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			};
			const VkPipelineColorBlendStateCreateInfo colorBlending{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
				.attachmentCount = depthOnly ? 0U : 1U,
				.pAttachments = &colorBlendAttachment,
			};
			const VkPipelineDepthStencilStateCreateInfo depthStencil{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
				.depthTestEnable = VK_TRUE,
				.depthWriteEnable = VK_TRUE,
				.depthCompareOp = VK_COMPARE_OP_LESS,
			};
			const VkPipelineRenderingCreateInfo renderingInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
				.colorAttachmentCount = depthOnly ? 0U : 1U,
				.pColorAttachmentFormats = &colorAttachmentFormat,
				.depthAttachmentFormat = depthAttachmentFormat,
			};
			const VkGraphicsPipelineCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.pNext = &renderingInfo,
				.stageCount = depthOnly ? 1U : 2U,
				.pStages = shaderStages.data(),
				.pVertexInputState = &vertexInputState,
				.pInputAssemblyState = &inputAssembly,
				.pViewportState = &viewportState,
				.pRasterizationState = &rasterizer,
				.pMultisampleState = &multisampling,
				.pDepthStencilState = &depthStencil,
				.pColorBlendState = &colorBlending,
				.layout = pipelineLayout,
			};
			VkPipeline rawPipeline{VK_NULL_HANDLE};
			const VkResult result = vkCreateGraphicsPipelines(owningDevice, VK_NULL_HANDLE, 1U, &createInfo, nullptr, &rawPipeline);
			return pipeline.assign(owningDevice.handle, result, rawPipeline);
		}

		/// @brief Releases the owned graphics pipeline through its RAII wrapper.
		void cleanup() { pipeline.reset(); }
	};


} // namespace vve::simple
