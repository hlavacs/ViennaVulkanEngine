#include "VHInclude.h"
#include "VEInclude.h"



namespace vve {

	RendererShadow11::RendererShadow11( std::string systemName, Engine& engine, std::string windowName ) : Renderer(systemName, engine, windowName ) {

		engine.RegisterCallbacks( { 
			{this,  3500, "INIT", [this](Message& message){ return OnInit(message);} },
			{this,  1500, "PREPARE_NEXT_FRAME", [this](Message& message){ return OnPrepareNextFrame(message);} },
			{this,  1500, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} },
			{this,     0, "QUIT", [this](Message& message){ return OnQuit(message);} }
		} );
	};

	RendererShadow11::~RendererShadow11(){};

	bool RendererShadow11::OnInit(Message message) {
		Renderer::OnInit(message);

		vh::RenCreateRenderPass(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_swapChain, false, m_renderPass);

		vh::RenCreateDescriptorSetLayout( m_vkState().m_device, //Per frame
			{ 
				{ 	.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
				{ 	.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
			},
			m_descriptorSetLayoutPerFrame );

		vh::ComCreateCommandPool(m_vkState().m_surface, m_vkState().m_physicalDevice, m_vkState().m_device, m_commandPool);
		vh::ComCreateCommandBuffers(m_vkState().m_device, m_commandPool, m_commandBuffers);
		vh::RenCreateDescriptorPool(m_vkState().m_device, 1000, m_descriptorPool);

		vh::RenCreateDescriptorSet(m_vkState().m_device, m_descriptorSetLayoutPerFrame, m_descriptorPool, m_descriptorSetPerFrame);

		//Per frame uniform buffer
		vh::BufCreateBuffers(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(vh::UniformBufferFrame), m_uniformBuffersPerFrame);
		vh::RenUpdateDescriptorSet(m_vkState().m_device, m_uniformBuffersPerFrame, 0, 
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sizeof(vh::UniformBufferFrame), m_descriptorSetPerFrame);   

		//Per frame shadow index buffer
		vh::BufCreateBuffers(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, 
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, MAX_NUMBER_LIGHTS*sizeof(vh::ShadowIndex), m_uniformBuffersLights);
		vh::RenUpdateDescriptorSet(m_vkState().m_device, m_uniformBuffersLights, 1, 
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_NUMBER_LIGHTS*sizeof(vh::ShadowIndex), m_descriptorSetPerFrame);   

		return false;
	}


	void RendererShadow11::CheckShadowMaps( vecs::Handle handle, uint32_t number) {
		if(!m_registry.template Has<ShadowImage>(handle)) {
			ShadowImage shadowMap;

			for( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
				vh::Map map;
				vh::ImgCreateImage2(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator
					, SHADOW_MAP_DIMENSION, SHADOW_MAP_DIMENSION, 1, 1, number
					, VK_FORMAT_R32_SFLOAT, VK_IMAGE_TILING_OPTIMAL
					, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
					, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
					, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, map.m_mapImage, map.m_mapImageAllocation); 
				
				shadowMap.shadowImages.push_back(map);
			}
			m_registry.Put(handle, std::move(shadowMap));
		}
		auto shadowMap = m_registry.template Get<ShadowImage&>(handle);
	}

	template<typename T>
	void RendererShadow11::RegisterForLight(int& i) {
		for( auto [handle, light] : m_registry.template GetView<vecs::Handle, T&>() ) {
			m_engine.RegisterCallbacks( { 
				{this,  1500 + i*1000, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} }
			} );
			++i;
		};
	}

	/// @brief Prepare the next frame for shadow map rendering.
	/// Calculate number of lights and number of shadow maps. Calculate ShadowIndex values.
	/// Determine the number of passes and register record callback as many times. Create/adapt ShadowImage if necessaary.
	/// @param message 
	/// @return Returns false.
	bool RendererShadow11::OnPrepareNextFrame(Message message) {
		auto msg = message.template GetData<MsgPrepareNextFrame>();
		m_pass = 0;
		m_engine.DeregisterCallbacks(this, "RECORD_NEXT_FRAME");
		int i{0};
		RegisterForLight<PointLight>(i);
		RegisterForLight<DirectionalLight>(i);
		RegisterForLight<SpotLight>(i);
		return false;
	}

	bool RendererShadow11::OnRecordNextFrame(Message message) {
		auto msg = message.template GetData<MsgRecordNextFrame>();
		++m_pass;
		return false;
	}

	bool RendererShadow11::OnQuit(Message message) {
        vkDeviceWaitIdle(m_vkState().m_device);

		for( auto [handle, shadowMap] : m_registry.template GetView<vecs::Handle, ShadowImage&>() ) {
			for( auto& map : shadowMap().shadowImages ) {
				vh::ImgDestroyImage(m_vkState().m_device, m_vkState().m_vmaAllocator, map.m_mapImage, map.m_mapImageAllocation);
			}
		}

        vkDestroyCommandPool(m_vkState().m_device, m_commandPool, nullptr);

		//vkDestroyDescriptorSetLayout(m_vkState().m_device, m_descriptorSetLayoutPerObject, nullptr);
		//vkDestroyPipeline(m_vkState().m_device, m_graphicsPipeline.m_pipeline, nullptr);
		//vkDestroyPipelineLayout(m_vkState().m_device, m_graphicsPipeline.m_pipelineLayout, nullptr);
	
        vkDestroyDescriptorPool(m_vkState().m_device, m_descriptorPool, nullptr);
		vkDestroyRenderPass(m_vkState().m_device, m_renderPass, nullptr);
		vh::BufDestroyBuffer2(m_vkState().m_device, m_vkState().m_vmaAllocator, m_uniformBuffersPerFrame);
		vh::BufDestroyBuffer2(m_vkState().m_device, m_vkState().m_vmaAllocator, m_uniformBuffersLights);
		vkDestroyDescriptorSetLayout(m_vkState().m_device, m_descriptorSetLayoutPerFrame, nullptr);

		return false;
    }


};   // namespace vve