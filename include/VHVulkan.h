#pragma once

#include <vector>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>
#include <unordered_map>



namespace std {
	template <typename T, typename... Rest>
	inline void hash_combine(std::size_t& seed, const T& v, const Rest&... rest) {
	    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	    (hash_combine(seed, rest), ...);
	}
}


namespace vh {
	
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

	struct Color {
		alignas(16) glm::vec4 m_ambientColor{0.0f}; 
		alignas(16) glm::vec4 m_diffuseColor{0.0f};	
		alignas(16) glm::vec4 m_specularColor{0.0f};
	};

	struct UniformBufferObject {
	    alignas(16) glm::mat4 model;
	    alignas(16) glm::mat4 modelInverseTranspose;
	};

	struct UniformBufferObjectColor {
	    alignas(16) glm::mat4 model;
	    alignas(16) glm::mat4 modelInverseTranspose;
		alignas(16) vh::Color color{}; 		
	};

	struct UniformBufferObjectTexture {
	    alignas(16) glm::mat4 model;
	    alignas(16) glm::mat4 modelInverseTranspose;
		alignas(16) glm::vec2 uvScale; 		
	};

	struct CameraMatrix {
	    alignas(16) glm::mat4 view;
	    alignas(16) glm::mat4 proj;
	};

	//param.x==1...point, param.x==2...directional, param.x==3...spotlight
	struct LightParams {
		alignas(16) glm::vec3 	color{1.0f, 1.0f, 1.0f}; 
		alignas(16) glm::vec4 	param{1.0f, 1.0f, 1.0, 1.0f}; //x=type, y=intensity, z=power, w=ambient
		alignas(16) glm::vec3 	attenuation{1.0f, 0.0f, 0.0f}; //x=constant, y=linear, z=quadratic
	};

	struct PointLight {
	    alignas(16) glm::vec3 		positionW{100.0f, 100.0f, 100.0f};
		alignas(16) LightParams 	params;
	};

	struct DirectionalLight {
	    alignas(16) glm::vec3 		directionW{100.0f, 100.0f, -100.0f};
		alignas(16) LightParams 	params;
	};

	struct SpotLight {
	    alignas(16) glm::vec3 		positionW{100.0f, 100.0f, 100.0f};
	    alignas(16) glm::vec3 		directionW{100.0f, 100.0f, -100.0f};
		alignas(16) LightParams 	params;
	};

	//params.param.x==1...point, params.param.x==2...directional, params.param.x==3...spotlight
	struct Light {
	    alignas(16) glm::vec3 	positionW{100.0f, 100.0f, 100.0f};
	    alignas(16) glm::vec3 	directionW{1.0f, 1.0f, 1.0f}; //always local y-axis
	    alignas(16) LightParams params;
		alignas(16) glm::mat4 	lightSpaceMatrix{1.0f};
	};

	struct UniformBufferFrame {
	    alignas(16) CameraMatrix camera;
		alignas(16) glm::ivec3 numLights{1,0,0}; //x=number point lights, y=number directional lights, z=number spotlights
	};

	struct UniformBuffers {
		VkDeviceSize 				m_bufferSize{0};
        std::vector<VkBuffer>       m_uniformBuffers;
        std::vector<VmaAllocation>  m_uniformBuffersAllocation;
        std::vector<void*>          m_uniformBuffersMapped;
    };

	struct DescriptorSet {
		int m_set{0};
		std::vector<VkDescriptorSet> m_descriptorSetPerFrameInFlight;
	};

    struct SwapChain {
        VkSwapchainKHR m_swapChain;
        std::vector<VkImage> m_swapChainImages;
        VkFormat m_swapChainImageFormat;
        VkExtent2D m_swapChainExtent;
        std::vector<VkImageView> m_swapChainImageViews;
        std::vector<VkFramebuffer> m_swapChainFramebuffers;
    };

    struct Pipeline {
        VkPipelineLayout m_pipelineLayout;
        VkPipeline m_pipeline;
    };

    struct DepthImage {
        VkImage         m_depthImage;
        VmaAllocation   m_depthImageAllocation;
        VkImageView     m_depthImageView;
    };

    struct Map {
		int 			m_width;
		int				m_height;
		VkDeviceSize	m_size;
		void *			m_pixels{nullptr};
		VkImage         m_mapImage;
        VmaAllocation   m_mapImageAllocation;
        VkImageView     m_mapImageView;
        VkSampler       m_mapSampler;
    };


	/// Pipeline code:
	/// P...Vertex data contains positions
	/// N...Vertex data contains normals
	/// T...Vertex data contains tangents
	/// C...Vertex data contains colors
	/// U...Vertex data contains texture UV coordinates
	struct VertexData {

		static const int size_pos = sizeof(glm::vec3);
		static const int size_nor = sizeof(glm::vec3);
		static const int size_tex = sizeof(glm::vec2);
		static const int size_col = sizeof(glm::vec4);
		static const int size_tan = sizeof(glm::vec3);

		std::vector<glm::vec3> m_positions;
		std::vector<glm::vec3> m_normals;
		std::vector<glm::vec2> m_texCoords;
		std::vector<glm::vec4> m_colors;
		std::vector<glm::vec3> m_tangents;

		std::string getType() {
			std::string name;
			if( m_positions.size() > 0 ) name = name + "P";
			if( m_normals.size() > 0 )   name = name + "N";
			if( m_texCoords.size() > 0 ) name = name + "U";
			if( m_colors.size() > 0 )    name = name + "C";
			if( m_tangents.size() > 0 )  name = name + "T";
			return name;
		}

		VkDeviceSize getSize() {
			return 	m_positions.size() * sizeof(glm::vec3) + 
					m_normals.size()   * sizeof(glm::vec3) + 
					m_texCoords.size() * sizeof(glm::vec2) + 
					m_colors.size()    * sizeof(glm::vec4) + 
					m_tangents.size()  * sizeof(glm::vec3);
		}

		VkDeviceSize getSize( std::string type ) {
			return 	type.find("P") != std::string::npos ? m_positions.size() * sizeof(glm::vec3) : 0 + 
					type.find("N") != std::string::npos ? m_normals.size()   * sizeof(glm::vec3) : 0 + 
					type.find("U") != std::string::npos ? m_texCoords.size() * sizeof(glm::vec2) : 0 + 
					type.find("C") != std::string::npos ? m_colors.size()    * sizeof(glm::vec4) : 0 + 
					type.find("T") != std::string::npos ? m_tangents.size()  * sizeof(glm::vec3) : 0;
		}

		std::vector<VkDeviceSize> getOffsets() {
			size_t offset=0;
			std::vector<VkDeviceSize> offsets{};
			if( size_t size = m_positions.size() * size_pos; size > 0 ) { offsets.push_back(offset); offset += size; }
			if( size_t size = m_normals.size()   * size_nor; size > 0 ) { offsets.push_back(offset); offset += size; }
			if( size_t size = m_texCoords.size() * size_tex; size > 0 ) { offsets.push_back(offset); offset += size; }
			if( size_t size = m_colors.size()    * size_col; size > 0 ) { offsets.push_back(offset); offset += size; }
			if( size_t size = m_tangents.size()  * size_tan; size > 0 ) { offsets.push_back(offset); offset += size; }
			return offsets;
		}

		std::vector<VkDeviceSize> getOffsets( std::string type ) {
			size_t offset=0;
			std::vector<VkDeviceSize> offsets{};
			if( type.find("P") != std::string::npos ) { offsets.push_back(offset); offset += m_positions.size() * size_pos; }
			if( type.find("N") != std::string::npos ) { offsets.push_back(offset); offset += m_normals.size()   * size_nor; }
			if( type.find("U") != std::string::npos ) { offsets.push_back(offset); offset += m_texCoords.size() * size_tex; }
			if( type.find("C") != std::string::npos ) { offsets.push_back(offset); offset += m_colors.size()    * size_col; }
			if( type.find("T") != std::string::npos ) { offsets.push_back(offset); offset += m_tangents.size()  * size_tan; }
			return offsets;
		}

		void copyData( void* data ) {
			size_t offset=0, size = 0;
			size = m_positions.size() * size_pos; memcpy( data, m_positions.data(), size );                 offset += size;
			size = m_normals.size()   * size_nor; memcpy( (char*)data + offset, m_normals.data(), size );   offset += size;
			size = m_texCoords.size() * size_tex; memcpy( (char*)data + offset, m_texCoords.data(), size ); offset += size;
			size = m_colors.size()    * size_col; memcpy( (char*)data + offset, m_colors.data(), size );    offset += size;
			size = m_tangents.size()  * size_tan; memcpy( (char*)data + offset, m_tangents.data(), size );  offset += size;
		}

		void copyData( void* data, std::string type  ) {
			size_t offset=0, size = 0;
			if( type.find("P") != std::string::npos ) { size = m_positions.size() * size_pos; memcpy( data, m_positions.data(), size ); offset += size; }
			if( type.find("N") != std::string::npos ) { size = m_normals.size()   * size_nor; memcpy( (char*)data + offset, m_normals.data(), size ); offset += size; }
			if( type.find("U") != std::string::npos ) { size = m_texCoords.size() * size_tex; memcpy( (char*)data + offset, m_texCoords.data(), size ); offset += size; }
			if( type.find("C") != std::string::npos ) { size = m_colors.size()    * size_col; memcpy( (char*)data + offset, m_colors.data(), size ); offset += size; }
			if( type.find("T") != std::string::npos ) { size = m_tangents.size()  * size_tan; memcpy( (char*)data + offset, m_tangents.data(), size ); offset += size; }
		}
    };

    struct Mesh {
		VertexData				m_verticesData;
        std::vector<uint32_t>   m_indices;
        VkBuffer                m_vertexBuffer;
        VmaAllocation           m_vertexBufferAllocation;
        VkBuffer                m_indexBuffer;
        VmaAllocation           m_indexBufferAllocation;
    };


    /// @brief Semaphores for signalling that a command buffer has finished executing. Every buffer gets its own Semaphore.
    struct Semaphores {
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
    };

	#define MAX_FRAMES_IN_FLIGHT 2

    std::vector<char> VulReadFile(const std::string& filename);

	void VulSetupImgui(SDL_Window* sdlWindow, VkInstance instance, VkPhysicalDevice physicalDevice, QueueFamilyIndices queueFamilies
        , VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkDescriptorPool descriptorPool
        , VkRenderPass renderPass);

}


#include "VHBuffer.h"
#include "VHCommand.h"
#include "VHDevice.h"
#include "VHRender.h"
#include "VHVulkan.h"
#include "VHSync.h"
