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



namespace vh {
	struct Vertex;
}


namespace std {
    template<> struct hash<vh::Vertex> {
        size_t operator()(vh::Vertex const& vertex) const; 
    };

	template <typename T, typename... Rest>
	inline void hash_combine(std::size_t& seed, const T& v, const Rest&... rest) {
	    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	    (hash_combine(seed, rest), ...);
	}
}


namespace vh {
	
	//use this macro to check the function result, if its not VK_SUCCESS then return the error
    #define VHCHECKRESULT(x) { CheckResult(VkResult err) };
    void CheckResult(VkResult err);

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

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec3 color;
        glm::vec2 texCoord;
        glm::vec3 tangent;

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, pos);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, normal);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

            attributeDescriptions[3].binding = 0;
            attributeDescriptions[3].location = 3;
            attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[3].offset = offsetof(Vertex, color);

            attributeDescriptions[4].binding = 0;
            attributeDescriptions[4].location = 4;
            attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[4].offset = offsetof(Vertex, tangent);

            return attributeDescriptions;
        }

        bool operator==(const Vertex& other) const {
            return pos == other.pos && color == other.color && texCoord == other.texCoord 
				&& normal == other.normal && tangent == other.tangent;
        }
	};

	struct Color {
		glm::vec4 m_ambientColor{0.0f};
		glm::vec4 m_diffuseColor{0.0f};
		glm::vec4 m_specularColor{0.0f};
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

    struct Texture {
		int 			m_width;
		int				m_height;
		VkDeviceSize	m_size;
		void *			m_pixels{nullptr};
		VkImage         m_textureImage;
        VmaAllocation   m_textureImageAllocation;
        VkImageView     m_textureImageView;
        VkSampler       m_textureSampler;
    };

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

		std::vector<VkDeviceSize> getOffsets() {
			int offset=0;
			std::vector<VkDeviceSize> offsets{};
			if( int size = m_positions.size() * size_pos; size > 0 ) { offsets.push_back(offset); offset += size; }
			if( int size = m_normals.size()   * size_nor; size > 0 ) { offsets.push_back(offset); offset += size; }
			if( int size = m_texCoords.size() * size_tex; size > 0 ) { offsets.push_back(offset); offset += size; }
			if( int size = m_colors.size()    * size_col; size > 0 ) { offsets.push_back(offset); offset += size; }
			if( int size = m_tangents.size()  * size_tan; size > 0 ) { offsets.push_back(offset); offset += size; }
			return offsets;
		}

		void copyData( void* data ) {
			int offset=0, size = 0;
			size = m_positions.size() * size_pos; memcpy( data, m_positions.data(), size );                 offset += size;
			size = m_normals.size()   * size_nor; memcpy( (char*)data + offset, m_normals.data(), size );   offset += size;
			size = m_texCoords.size() * size_tex; memcpy( (char*)data + offset, m_texCoords.data(), size ); offset += size;
			size = m_colors.size()    * size_col; memcpy( (char*)data + offset, m_colors.data(), size );    offset += size;
			size = m_tangents.size()  * size_tan; memcpy( (char*)data + offset, m_tangents.data(), size );  offset += size;
		}

		void getBindingDescription( auto &vec, int &binding, int stride, auto& bdesc ) {
			if( vec.size() == 0 ) return;
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = binding++;
			bindingDescription.stride = stride;
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			bdesc.push_back( bindingDescription );
		}

		auto getBindingDescriptions() {
			std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
			
			int binding=0;
			getBindingDescription( m_positions, binding, size_pos, bindingDescriptions );
			getBindingDescription( m_normals,   binding, size_nor, bindingDescriptions );
			getBindingDescription( m_texCoords, binding, size_tex, bindingDescriptions );
			getBindingDescription( m_colors,    binding, size_col, bindingDescriptions );
			getBindingDescription( m_tangents,  binding, size_tan, bindingDescriptions );
			return bindingDescriptions;
		}

		void addAttributeDescription( auto &vec, int& binding, int& location, VkFormat format, auto& attd ) {
			if( vec.size() == 0 ) return;
			VkVertexInputAttributeDescription attributeDescription{};
			attributeDescription.binding = binding++;
			attributeDescription.location = location++;
			attributeDescription.format = format;
			attributeDescription.offset = 0;
			attd.push_back( attributeDescription );
		}

        auto getAttributeDescriptions() {
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

			int binding=0;
			int location=0;
			addAttributeDescription( m_positions, binding, location, VK_FORMAT_R32G32B32_SFLOAT, attributeDescriptions );
			addAttributeDescription( m_normals,   binding, location, VK_FORMAT_R32G32B32_SFLOAT, attributeDescriptions );
			addAttributeDescription( m_texCoords, binding, location, VK_FORMAT_R32G32_SFLOAT,    attributeDescriptions );
			addAttributeDescription( m_colors,    binding, location, VK_FORMAT_R32G32B32A32_SFLOAT, attributeDescriptions );
			addAttributeDescription( m_tangents,  binding, location, VK_FORMAT_R32G32B32_SFLOAT, attributeDescriptions );
            return attributeDescriptions;
        }

		VkResult CmdBindVertexBuffers( VkCommandBuffer commandBuffer, VkBuffer vertexBuffer ) {
			std::vector<VkDeviceSize> offsets = getOffsets();
			std::vector<VkBuffer> vertexBuffers(offsets.size(), vertexBuffer);
			vkCmdBindVertexBuffers( commandBuffer, 0, offsets.size(), vertexBuffers.data(), offsets.data() );
			return VK_SUCCESS;
		}
    };

    struct Mesh {
        std::vector<Vertex>     m_vertices;
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

    void MemCopy(VkDevice device, void* source, VmaAllocationInfo& allocInfo, VkDeviceSize size, VkDeviceSize offset=0);
    
    std::vector<char> readFile(const std::string& filename);

	void setupImgui(SDL_Window* sdlWindow, VkInstance instance, VkPhysicalDevice physicalDevice, QueueFamilyIndices queueFamilies
        , VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkDescriptorPool descriptorPool
        , VkRenderPass renderPass);

}


#include "VHBuffer.h"
#include "VHCommand.h"
#include "VHDevice.h"
#include "VHRender.h"
#include "VHVulkan.h"
#include "VHSync.h"
