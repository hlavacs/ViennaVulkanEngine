#include "VHInclude.h"
#include "VEInclude.h"



namespace vve {

    Renderer::Renderer(std::string systemName, Engine& engine, std::string windowName ) : 
		System{systemName, engine }, m_windowName(windowName) {	};

    Renderer::~Renderer(){};

	auto Renderer::GetState(vecs::Registry& registry) -> std::tuple<vecs::Handle, vecs::Ref<VulkanState>> {
        return *registry.template GetView<vecs::Handle, VulkanState&>().begin();
    }

	bool Renderer::OnInit(Message message) {
		auto [handle, stateW, stateSDL] = WindowSDL::GetState(m_registry);
		m_windowState = stateW;
		m_windowSDLState = stateSDL;

		auto view = m_registry.template GetView<vecs::Handle, VulkanState&>();
		auto iterBegin = view.begin();
		auto iterEnd = view.end();
		if( !(iterBegin != iterEnd)) {
			m_vulkanStateHandle = m_registry.Insert(VulkanState{});
			m_vkState = m_registry.template Get<VulkanState&>(m_vulkanStateHandle);
			return false;
		}
		auto [handleV, stateV] = *iterBegin;
		m_vulkanStateHandle = handleV;
		m_vkState = stateV;
		return false;
	}

	void Renderer::SubmitCommandBuffer( VkCommandBuffer commandBuffer ) { 
		m_vkState().m_commandBuffersSubmit.push_back(commandBuffer); 
	};

	void Renderer::getBindingDescription( std::string type, std::string C, int &binding, int stride, auto& bdesc ) {
		if( type.find(C) == std::string::npos ) return;
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = binding++;
		bindingDescription.stride = stride;
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		bdesc.push_back( bindingDescription );
	}

	auto Renderer::getBindingDescriptions( std::string type ) -> std::vector<VkVertexInputBindingDescription> {
		std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
			
		int binding=0;
		getBindingDescription( type, "P", binding, size_pos, bindingDescriptions );
		getBindingDescription( type, "N", binding, size_nor, bindingDescriptions );
		getBindingDescription( type, "U", binding, size_tex, bindingDescriptions );
		getBindingDescription( type, "C", binding, size_col, bindingDescriptions );
		getBindingDescription( type, "T", binding, size_tan, bindingDescriptions );
		return bindingDescriptions;
	}

	void Renderer::addAttributeDescription( std::string type, std::string C, int& binding, int& location, VkFormat format, auto& attd ) {
		if( type.find(C) == std::string::npos ) return;
		VkVertexInputAttributeDescription attributeDescription{};
		attributeDescription.binding = binding++;
		attributeDescription.location = location++;
		attributeDescription.format = format;
		attributeDescription.offset = 0;
		attd.push_back( attributeDescription );
	}

    auto Renderer::getAttributeDescriptions(std::string type) -> std::vector<VkVertexInputAttributeDescription> {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

		int binding=0;
		int location=0;
		addAttributeDescription( type, "P", binding, location, VK_FORMAT_R32G32B32_SFLOAT, attributeDescriptions );
		addAttributeDescription( type, "N", binding, location, VK_FORMAT_R32G32B32_SFLOAT, attributeDescriptions );
		addAttributeDescription( type, "U", binding, location, VK_FORMAT_R32G32_SFLOAT,    attributeDescriptions );
		addAttributeDescription( type, "C", binding, location, VK_FORMAT_R32G32B32A32_SFLOAT, attributeDescriptions );
		addAttributeDescription( type, "T", binding, location, VK_FORMAT_R32G32B32_SFLOAT, attributeDescriptions );
        return attributeDescriptions;
    }

	template<typename T>
	int Renderer::RegisterLight(float type, std::vector<vvh::Light>& lights, int& total) {
		int n=0;
		for( auto [handle, light, lToW] : m_registry.template GetView<vecs::Handle, T&, LocalToWorldMatrix&>() ) {
			++n;
			//m_engine.RegisterCallbacks( { {this,  2000 + total*1000, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} }} );
			light().params.x = type;
			lights[total] = { .positionW = glm::vec3{lToW()[3]}, .directionW = glm::vec3{lToW()[1]}, .lightParams = light() };
			if( ++total >= MAX_NUMBER_LIGHTS ) return n;
		};
		return n;
	}

	template auto Renderer::RegisterLight<PointLight>(float type, std::vector<vvh::Light>& lights, int& i) -> int;
	template auto Renderer::RegisterLight<DirectionalLight>(float type, std::vector<vvh::Light>& lights, int& i) -> int;
	template auto Renderer::RegisterLight<SpotLight>(float type, std::vector<vvh::Light>& lights, int& i) -> int;

};  // namespace vve

