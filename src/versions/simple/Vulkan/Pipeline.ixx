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

export module VEEngine.Simple.Vulkan:Pipeline;
import :Device;
import VEEngine.Simple.Mesh;
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
		std::uint32_t useBaseColorTexture{0U}; ///< Non-zero when the object wants the optional base-color texture.
		std::uint32_t spotLightIndex{0U};      ///< Shadow pass spot-light matrix index selected before each draw call.
		std::uint32_t dirLightIndex{0U};       ///< Shadow pass directional-light matrix index selected before each draw call.
	};

	/// @brief Minimal Vulkan descriptor-set-layout owner for frame uniforms, shadow maps, and one object texture; no pipeline layout is created here.
	struct VulkanDescriptorSetLayout {
		VkDevice device{VK_NULL_HANDLE};                                  ///< Borrowed Vulkan logical device used to destroy the layout.
		VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};         ///< Owned descriptor-set layout for set 0 frame uniforms, shadow maps, and object texture.

		VulkanDescriptorSetLayout() = default;
		VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout &) = delete;
		VulkanDescriptorSetLayout &operator=(const VulkanDescriptorSetLayout &) = delete;

		/**
			* @brief Creates set 0 with binding 0 as FrameUniforms, bindings 1, 4, 5, 6, and 7 as shadow maps, and binding 2 as one object texture.
			*
			* @param owningDevice Logical device that owns the created descriptor-set layout.
			* @return VK_SUCCESS when the descriptor-set layout is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkDevice owningDevice) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkDescriptorSetLayoutBinding frameUniformBinding{
				.binding = 0U,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1U,
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr,
			};
			const VkDescriptorSetLayoutBinding shadowMapBinding{
				.binding = 1U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1U,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr,
			}; ///< Binding 1 exposes the sampled shadow map to the fragment shader.
			const VkDescriptorSetLayoutBinding objectTextureBinding{
				.binding = 2U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1U,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr,
			}; ///< Binding 2 reserves one sampled object base-color texture for later fragment shading.
			const VkDescriptorSetLayoutBinding spotShadowMapBinding{
				.binding = 4U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1U,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr,
			}; ///< Binding 4 exposes the sampled spot shadow map to the fragment shader.
			const VkDescriptorSetLayoutBinding spotShadowArrayBinding{
				.binding = 5U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1U,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr,
			}; ///< Binding 5 exposes the sampled spot shadow-map array to the fragment shader.
			const VkDescriptorSetLayoutBinding dirShadowArrayBinding{
				.binding = 6U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1U,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr,
			}; ///< Binding 6 exposes the sampled directional shadow-map array to the fragment shader.
			const VkDescriptorSetLayoutBinding pointShadowArrayBinding{
				.binding = 7U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1U,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr,
			}; ///< Binding 7 exposes the sampled point shadow-map array to the fragment shader.
			const std::array bindings{frameUniformBinding, shadowMapBinding, objectTextureBinding, spotShadowMapBinding, spotShadowArrayBinding, dirShadowArrayBinding, pointShadowArrayBinding};
			const VkDescriptorSetLayoutCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.bindingCount = static_cast<std::uint32_t>(bindings.size()),
				.pBindings = bindings.data(),
			};

			device = owningDevice;
			const VkResult result = vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &descriptorSetLayout);
			if (result != VK_SUCCESS) {
				descriptorSetLayout = VK_NULL_HANDLE;
				device = VK_NULL_HANDLE;
			}
			return result;
		}

		/**
			* @brief Destroys the owned descriptor-set layout and clears the borrowed device handle.
			*/
		void cleanup() {
			if (descriptorSetLayout != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr); }
			descriptorSetLayout = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned descriptor-set layout on scope exit.
			*/
		~VulkanDescriptorSetLayout() { cleanup(); }
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
		VkDevice device{VK_NULL_HANDLE};                  ///< Borrowed Vulkan logical device used to destroy the pipeline layout.
		VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};   ///< Owned pipeline layout for set 0 and model push constants.

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
		[[nodiscard]] VkResult create(VkDevice owningDevice, VkDescriptorSetLayout setLayout) {
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

			device = owningDevice;
			const VkResult result = vkCreatePipelineLayout(device, &createInfo, nullptr, &pipelineLayout);
			if (result != VK_SUCCESS) {
				pipelineLayout = VK_NULL_HANDLE;
				device = VK_NULL_HANDLE;
			}
			return result;
		}

		/**
			* @brief Destroys the owned pipeline layout and clears the borrowed device handle.
			*/
		void cleanup() {
			if (pipelineLayout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, pipelineLayout, nullptr); }
			pipelineLayout = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned pipeline layout on scope exit.
			*/
		~VulkanPipelineLayout() { cleanup(); }
	};

	/// @brief Minimal Vulkan shader-module owner; no layouts, pipelines, commands, or sync are created here.
	struct VulkanShaderModule {
		VkDevice device{VK_NULL_HANDLE};              ///< Borrowed Vulkan logical device used to destroy the shader module.
		VkShaderModule shaderModule{VK_NULL_HANDLE};  ///< Owned shader module created from a SPIR-V binary.

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
		[[nodiscard]] VkResult create(VkDevice owningDevice, std::string_view spirvPath) {
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

			device = owningDevice;
			const VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
			if (result != VK_SUCCESS) {
				shaderModule = VK_NULL_HANDLE;
				device = VK_NULL_HANDLE;
			}
			return result;
		}

		/**
			* @brief Destroys the owned shader module and clears the borrowed device handle.
			*/
		void cleanup() {
			if (shaderModule != VK_NULL_HANDLE) { vkDestroyShaderModule(device, shaderModule, nullptr); }
			shaderModule = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned shader module on scope exit.
			*/
		~VulkanShaderModule() { cleanup(); }
	};

	/// @brief Minimal Vulkan graphics-pipeline owner for the simple forward color pass.
	struct VulkanGraphicsPipeline {
		VkDevice device{VK_NULL_HANDLE};      ///< Borrowed Vulkan logical device used to destroy the pipeline.
		VkPipeline pipeline{VK_NULL_HANDLE};  ///< Owned graphics pipeline for the simple forward pass.

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
			* @return VK_SUCCESS when the graphics pipeline is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(
			VkDevice owningDevice,
			VkRenderPass renderPass,
			VkPipelineLayout pipelineLayout,
			VkShaderModule vertexModule,
			VkShaderModule fragmentModule,
			const VulkanVertexInputDescription &vertexInput,
			VkExtent2D extent
		) {
			cleanup();
			if (
				owningDevice == VK_NULL_HANDLE ||
				renderPass == VK_NULL_HANDLE ||
				pipelineLayout == VK_NULL_HANDLE ||
				vertexModule == VK_NULL_HANDLE ||
				fragmentModule == VK_NULL_HANDLE
			) {
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
			const VkGraphicsPipelineCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
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
				.renderPass = renderPass,
				.subpass = 0U,
				.basePipelineHandle = VK_NULL_HANDLE,
			};

			device = owningDevice;
			const VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1U, &createInfo, nullptr, &pipeline);
			if (result != VK_SUCCESS) {
				pipeline = VK_NULL_HANDLE;
				device = VK_NULL_HANDLE;
			}
			return result;
		}

		/**
			* @brief Destroys the owned graphics pipeline and clears the borrowed device handle.
			*/
		void cleanup() {
			if (pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, pipeline, nullptr); }
			pipeline = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned graphics pipeline on scope exit.
			*/
		~VulkanGraphicsPipeline() { cleanup(); }
	};


} // namespace vve::simple
