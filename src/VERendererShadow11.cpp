#include "VHInclude.h"
#include "VEInclude.h"



namespace vve {

	RendererShadow11::RendererShadow11( std::string systemName, Engine& engine, std::string windowName ) : Renderer(systemName, engine, windowName ) {

		engine.RegisterCallback( { 
			//{this,  3500, "INIT", [this](Message& message){ return OnInit(message);} },
			//{this,  1500, "PREPARE_NEXT_FRAME", [this](Message& message){ return OnPrepareNextFrame(message);} },
			//{this,  1500, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} }, 
			//{this,     0, "QUIT", [this](Message& message){ return OnQuit(message);} }
		} );
	};

	RendererShadow11::~RendererShadow11(){};

	bool RendererShadow11::OnInit(Message message) {
		Renderer::OnInit(message);

		vh::RenCreateRenderPass(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_swapChain, false, m_renderPass);

		vh::RenCreateDescriptorSetLayout( m_vulkanState().m_device, //Per frame
			{ 
				{ 	.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
				{ 	.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
			},
			m_descriptorSetLayoutPerFrame );

		vh::ComCreateCommandPool(m_vulkanState().m_surface, m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_commandPool);
		vh::ComCreateCommandBuffers(m_vulkanState().m_device, m_commandPool, m_commandBuffers);
		vh::RenCreateDescriptorPool(m_vulkanState().m_device, 1000, m_descriptorPool);

		vh::RenCreateDescriptorSet(m_vulkanState().m_device, m_descriptorSetLayoutPerFrame, m_descriptorPool, m_descriptorSetPerFrame);

		//Per frame uniform buffer
		vh::BufCreateBuffers(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(vh::UniformBufferFrame), m_uniformBuffersPerFrame);
		vh::RenUpdateDescriptorSet(m_vulkanState().m_device, m_uniformBuffersPerFrame, 0, 
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sizeof(vh::UniformBufferFrame), m_descriptorSetPerFrame);   

		//Per frame shadow index buffer
		vh::BufCreateBuffers(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, 
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_maxNumberLights*sizeof(vh::ShadowIndex), m_uniformBuffersLights);
		vh::RenUpdateDescriptorSet(m_vulkanState().m_device, m_uniformBuffersLights, 1, 
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_maxNumberLights*sizeof(vh::ShadowIndex), m_descriptorSetPerFrame);   

		return false;
	}


	void RendererShadow11::CheckShadowMaps( vecs::Handle handle, uint32_t number) {
		if(!m_registry.template Has<ShadowMap>(handle)) {
			ShadowMap shadowMap;

			for( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
				vh::Map map;
				vh::ImgCreateImage2(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator
					, MAP_DIMENSION, MAP_DIMENSION, 1, 1, number
					, VK_FORMAT_R32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
					, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, map.m_mapImage, map.m_mapImageAllocation); 
				
				vh::ImgTransitionImageLayout(m_vulkanState().m_device, m_vulkanState().m_graphicsQueue, m_vulkanState().m_commandPool
					, map.m_mapImage, VK_FORMAT_R32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

				shadowMap.m_shadowMaps.push_back(map);
			}
			m_registry.Put(handle, std::move(shadowMap));
		}
		auto shadowMap = m_registry.template Get<ShadowMap&>(handle);
		vh::ImgClearShadowMap(m_vulkanState().m_device, m_vulkanState().m_graphicsQueue, m_vulkanState().m_commandPool, m_vulkanState().m_vmaAllocator
			, shadowMap().m_shadowMaps[m_vulkanState().m_currentFrame], std::numeric_limits<float>::max());
	}

	/// @brief Prepare the next frame for shadow map rendering.
	/// Calculate number of lights and number of shadow maps. Calculate ShadowIndex values.
	/// Determine the number of passes and register record callback as many times. Create/adapt ShadowImage if necessaary.
	/// @param message 
	/// @return Returns false.
	bool RendererShadow11::OnPrepareNextFrame(Message message) {
		auto msg = message.template GetData<MsgPrepareNextFrame>();

		for( auto [handle, light] : m_registry.template GetView<vecs::Handle, PointLight&>() ) {
			CheckShadowMaps(handle, 6);
		}
		for( auto [handle, light] : m_registry.template GetView<vecs::Handle, DirectionalLight&>() ) {
			CheckShadowMaps(handle, 3);
		}
		for( auto [handle, light] : m_registry.template GetView<vecs::Handle, SpotLight&>() ) {
			CheckShadowMaps(handle, 1);
		}
		return false;
	}

	bool RendererShadow11::OnRecordNextFrame(Message message) {
		auto msg = message.template GetData<MsgRecordNextFrame>();
		return false;
	}

	bool RendererShadow11::OnQuit(Message message) {
        vkDeviceWaitIdle(m_vulkanState().m_device);

		for( auto [handle, shadowMap] : m_registry.template GetView<vecs::Handle, ShadowMap&>() ) {
			for( auto& map : shadowMap().m_shadowMaps ) {
				vh::ImgDestroyImage(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, map.m_mapImage, map.m_mapImageAllocation);
			}
		}

        vkDestroyCommandPool(m_vulkanState().m_device, m_commandPool, nullptr);

		//vkDestroyDescriptorSetLayout(m_vulkanState().m_device, m_descriptorSetLayoutPerObject, nullptr);
		//vkDestroyPipeline(m_vulkanState().m_device, m_graphicsPipeline.m_pipeline, nullptr);
		//vkDestroyPipelineLayout(m_vulkanState().m_device, m_graphicsPipeline.m_pipelineLayout, nullptr);
	
        vkDestroyDescriptorPool(m_vulkanState().m_device, m_descriptorPool, nullptr);
		vkDestroyRenderPass(m_vulkanState().m_device, m_renderPass, nullptr);
		vh::BufDestroyBuffer2(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_uniformBuffersPerFrame);
		vh::BufDestroyBuffer2(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_uniformBuffersLights);
		vkDestroyDescriptorSetLayout(m_vulkanState().m_device, m_descriptorSetLayoutPerFrame, nullptr);

		return false;
    }


};   // namespace vve