#pragma once

namespace vve {

	template <typename Derived>
	class RendererDeferredCommon : public Renderer{

	protected:

		enum GBufferIndex { POSITION = 0, NORMAL = 1, ALBEDO = 2, DEPTH = 3 };

		struct PipelinePerType {
			std::string m_type;
			VkDescriptorSetLayout m_descriptorSetLayoutPerObject{};
			vvh::Pipeline m_graphicsPipeline{};
		};

		vvh::Buffer m_uniformBuffersPerFrame{};
		vvh::Buffer m_storageBuffersLights{};

		VkSampler m_sampler{ VK_NULL_HANDLE };
		std::array<vvh::GBufferImage, 3> m_gBufferAttachments{};

		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame{ VK_NULL_HANDLE };
		VkDescriptorSetLayout m_descriptorSetLayoutComposition{ VK_NULL_HANDLE };
		VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
		vvh::DescriptorSet m_descriptorSetPerFrame{};
		vvh::DescriptorSet m_descriptorSetComposition{};

		std::map<int, PipelinePerType> m_geomPipesPerType;
		vvh::Pipeline m_lightingPipeline{};

		std::vector<VkCommandPool> m_commandPools{};
		std::vector<VkCommandBuffer> m_commandBuffers{};

		//glm::ivec3 m_numberLightsPerType{ 0, 0, 0 };
		//int m_pass{ 0 };

		// ---------------------------------------------------------------------------------

	public:
		RendererDeferredCommon(std::string systemName, Engine& engine, std::string windowName) : Renderer(systemName, engine, windowName) {

			engine.RegisterCallbacks({
				{this,  3500, "INIT",				[this](Message& message) { return OnInit(message); } },
				});
		}

		virtual ~RendererDeferredCommon() {};

	private:

		bool OnInit(Message message) {
			// TODO: maybe a reference will be enough here to not make a message copy?
			Renderer::OnInit(message);

			// Per Frame
			vvh::RenCreateDescriptorSetLayout({
				.m_device = m_vkState().m_device,
				.m_bindings = {
					{	// Binding 0 : Vertex and Fragment uniform buffer
						.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
						.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
					{	// Binding 1 : Light uniform buffer
						.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
						.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
				},
				.m_descriptorSetLayout = m_descriptorSetLayoutPerFrame
				});

			// Composition
			vvh::RenCreateDescriptorSetLayout({
				.m_device = m_vkState().m_device,
				.m_bindings = {
					{	// Binding 0 : Position
						.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
					{	// Binding 1 : Normal
						.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
					{	// Binding 2 : Albedo
						.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
					{	// Binding 3 : Depth
						.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				},
				.m_descriptorSetLayout = m_descriptorSetLayoutComposition
				});

			m_commandPools.resize(MAX_FRAMES_IN_FLIGHT);
			for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
				vvh::ComCreateCommandPool({
					.m_surface = m_vkState().m_surface,
					.m_physicalDevice = m_vkState().m_physicalDevice,
					.m_device = m_vkState().m_device,
					.m_commandPool = m_commandPools[i]
					});
			}

			m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
			vvh::ComCreateCommandBuffers({
					.m_device = m_vkState().m_device,
					.m_commandPool = m_commandPools[m_vkState().m_currentFrame],
					.m_commandBuffers = m_commandBuffers
				});

			// TODO: shrink pool to only what is needed - why 1000?
			vvh::RenCreateDescriptorPool({
				.m_device = m_vkState().m_device,
				.m_sizes = 1000,
				.m_descriptorPool = m_descriptorPool
				});
			vvh::RenCreateDescriptorSet({
				.m_device = m_vkState().m_device,
				.m_descriptorSetLayouts = m_descriptorSetLayoutPerFrame,
				.m_descriptorPool = m_descriptorPool,
				.m_descriptorSet = m_descriptorSetPerFrame
				});
			vvh::RenCreateDescriptorSet({
				.m_device = m_vkState().m_device,
				.m_descriptorSetLayouts = m_descriptorSetLayoutComposition,
				.m_descriptorPool = m_descriptorPool,
				.m_descriptorSet = m_descriptorSetComposition
				});

			// Per frame uniform buffer
			vvh::BufCreateBuffers({
				.m_device = m_vkState().m_device,
				.m_vmaAllocator = m_vkState().m_vmaAllocator,
				.m_usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				.m_size = sizeof(vvh::UniformBufferFrame),
				.m_buffer = m_uniformBuffersPerFrame
				});
			vvh::RenUpdateDescriptorSet({
				.m_device = m_vkState().m_device,
				.m_uniformBuffers = m_uniformBuffersPerFrame,
				.m_binding = 0,
				.m_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.m_size = sizeof(vvh::UniformBufferFrame),
				.m_descriptorSet = m_descriptorSetPerFrame
				});

			// Per frame light buffer
			vvh::BufCreateBuffers({
				.m_device = m_vkState().m_device,
				.m_vmaAllocator = m_vkState().m_vmaAllocator,
				.m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.m_size = MAX_NUMBER_LIGHTS * sizeof(vvh::Light),
				.m_buffer = m_storageBuffersLights
				});
			vvh::RenUpdateDescriptorSet({
				.m_device = m_vkState().m_device,
				.m_uniformBuffers = m_storageBuffersLights,
				.m_binding = 1,
				.m_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.m_size = MAX_NUMBER_LIGHTS * sizeof(vvh::Light),
				.m_descriptorSet = m_descriptorSetPerFrame
				});

			// Composition Sampler
			vvh::ImgCreateImageSampler({
				.m_physicalDevice = m_vkState().m_physicalDevice,
				.m_device = m_vkState().m_device,
				.m_sampler = m_sampler
				});

			CreateDeferredResources();
			static_cast<Derived*>(this)->OnInit(message);

			return false;
		}

		void CreateDeferredResources() {

			vvh::RenCreateGBufferResources({
				.m_physicalDevice = m_vkState().m_physicalDevice,
				.m_device = m_vkState().m_device,
				.m_vmaAllocator = m_vkState().m_vmaAllocator,
				.m_swapChain = m_vkState().m_swapChain,
				.m_gbufferImage = m_gBufferAttachments[POSITION],
				.m_format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.m_sampler = m_sampler
				});
			vvh::RenUpdateDescriptorSetGBufferAttachment({
				.m_device = m_vkState().m_device,
				.m_gbufferImage = m_gBufferAttachments[POSITION],
				.m_binding = POSITION,
				.m_descriptorSet = m_descriptorSetComposition
				});
			vvh::RenCreateGBufferResources({
				.m_physicalDevice = m_vkState().m_physicalDevice,
				.m_device = m_vkState().m_device,
				.m_vmaAllocator = m_vkState().m_vmaAllocator,
				.m_swapChain = m_vkState().m_swapChain,
				.m_gbufferImage = m_gBufferAttachments[NORMAL],
				.m_format = VK_FORMAT_R8G8B8A8_UNORM,
				.m_sampler = m_sampler
				});
			vvh::RenUpdateDescriptorSetGBufferAttachment({
				.m_device = m_vkState().m_device,
				.m_gbufferImage = m_gBufferAttachments[NORMAL],
				.m_binding = NORMAL,
				.m_descriptorSet = m_descriptorSetComposition
				});
			vvh::RenCreateGBufferResources({
				.m_physicalDevice = m_vkState().m_physicalDevice,
				.m_device = m_vkState().m_device,
				.m_vmaAllocator = m_vkState().m_vmaAllocator,
				.m_swapChain = m_vkState().m_swapChain,
				.m_gbufferImage = m_gBufferAttachments[ALBEDO],
				.m_format = VK_FORMAT_R8G8B8A8_SRGB,
				.m_sampler = m_sampler
				});
			vvh::RenUpdateDescriptorSetGBufferAttachment({
				.m_device = m_vkState().m_device,
				.m_gbufferImage = m_gBufferAttachments[ALBEDO],
				.m_binding = ALBEDO,
				.m_descriptorSet = m_descriptorSetComposition
				});

			vvh::RenUpdateDescriptorSetDepthAttachment({
				.m_device = m_vkState().m_device,
				.m_depthImage = m_vkState().m_depthImage,
				.m_binding = DEPTH,
				.m_descriptorSet = m_descriptorSetComposition,
				.m_sampler = m_sampler
				});
		}

	};

}	// namespace vve
