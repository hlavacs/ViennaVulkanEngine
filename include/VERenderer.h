#pragma once

#include <memory>
#include <any>


namespace vve {

	class RendererVulkan;

	static const int size_pos = sizeof(glm::vec3);
	static const int size_nor = sizeof(glm::vec3);
	static const int size_tex = sizeof(glm::vec2);
	static const int size_col = sizeof(glm::vec4);
	static const int size_tan = sizeof(glm::vec3);

    enum class RendererType {
        FORWARD,
        DEFERRED,
        RAYTRACING
    };

	struct VulkanState {
		uint32_t 		m_apiVersionInstance{VK_API_VERSION_1_1};
		uint32_t 		m_apiVersionDevice{VK_API_VERSION_1_1};
		VkInstance 		m_instance{VK_NULL_HANDLE};
		VkSurfaceKHR 	m_surface{VK_NULL_HANDLE};
		VmaAllocator 	m_vmaAllocator{nullptr};
		VkDebugUtilsMessengerEXT m_debugMessenger;
		
		VkPhysicalDevice 			m_physicalDevice{VK_NULL_HANDLE};
		VkPhysicalDeviceFeatures 	m_physicalDeviceFeatures;
		VkPhysicalDeviceProperties 	m_physicalDeviceProperties;

		VkDevice 		m_device{VK_NULL_HANDLE};
		vvh::QueueFamilyIndices m_queueFamilies;
		VkQueue 		m_graphicsQueue{VK_NULL_HANDLE};
		VkQueue 		m_presentQueue{VK_NULL_HANDLE};
		vvh::SwapChain 	m_swapChain;
		vvh::DepthImage 	m_depthImage;
		VkFormat		m_depthMapFormat{VK_FORMAT_UNDEFINED};
		VkCommandPool 	m_commandPool{VK_NULL_HANDLE};
		std::vector<VkCommandBuffer> m_commandBuffersSubmit;

		uint32_t m_currentFrame = MAX_FRAMES_IN_FLIGHT - 1;
		uint32_t m_imageIndex;
		bool m_framebufferResized = false;
	};

    class Renderer : public System {
        friend class Engine;

    public:
        Renderer(std::string systemName, Engine& m_engine, std::string windowName);
        virtual ~Renderer();
		static auto GetState(vecs::Registry& registry) -> std::tuple< vecs::Handle, vecs::Ref<VulkanState>>;

    protected:
		bool OnInit(Message message);
		void SubmitCommandBuffer( VkCommandBuffer commandBuffer );
		void getBindingDescription( std::string type, std::string C, int &binding, int stride, auto& bdesc );
		auto getBindingDescriptions(std::string type) -> std::vector<VkVertexInputBindingDescription>;
		void addAttributeDescription( std::string type, std::string C, int& binding, int& location, VkFormat format, auto& attd );
        auto getAttributeDescriptions(std::string type) -> std::vector<VkVertexInputAttributeDescription>;

		template<typename T> 
		auto RegisterLight(float type, std::vector<vvh::Light>& lights, int& i) -> int;

		std::string 				m_windowName;
		vecs::Ref<WindowState> 		m_windowState{};
		vecs::Ref<WindowSDLState> 	m_windowSDLState{};
		vecs::Handle 				m_vulkanStateHandle{};
		vecs::Ref<VulkanState> 		m_vkState{};
    };

};   // namespace vve