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
#include <span>

#define MAX_FRAMES_IN_FLIGHT 2
#define MAXINFLIGHT 2



namespace vvh {

	/**
	 * @brief Convert vector of strings to vector of C-style string pointers
	 * @param vec Vector of strings
	 * @return Vector of const char pointers
	 */
	inline auto ToCharPtr(const std::vector<std::string>& vec) -> std::vector<const char*> {
		std::vector<const char*> res;
		for (auto& str : vec) res.push_back(str.c_str());
		return res;
	}

	/**
	 * @brief Read file contents into a character vector
	 * @param filename Path to the file
	 * @return Vector containing file contents
	 */
	inline std::vector<char> ReadFile(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			std::cout << "failed to open file: " << filename << std::endl;
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}



	//--------------------------------------------------------------------
	//Shader resources
	//make sure that their size is a multiple of 16!

	/**
	 * @brief Color structure with ambient, diffuse, and specular components
	 */
	struct Color {
		glm::vec4 m_ambientColor{ 0.0f };
		glm::vec4 m_diffuseColor{ 0.0f };
		glm::vec4 m_specularColor{ 0.0f };
	};

	struct BufferPerObject {
		glm::mat4 model;
		glm::mat4 modelInverseTranspose;
	};

	struct BufferPerObjectColor {
		glm::mat4 model;
		glm::mat4 modelInverseTranspose;
		vvh::Color color{};
	};

	struct BufferPerObjectTexture {
		glm::mat4 model;
		glm::mat4 modelInverseTranspose;
		glm::vec2 uvScale;
		glm::vec2 padding;
	};

	struct CameraMatrix {
		glm::mat4 view;
		glm::mat4 proj;
		glm::vec3 positionW;
		float padding;
	};

	struct ShadowIndex {
		glm::ivec2 	mapResolution;
		uint32_t 	layerIndex;
		uint32_t 	viewportIndex;
		glm::ivec2	layerOffset;
		glm::mat4 	lightSpaceMatrix;
	};

	struct ShadowOffset {
		int shadowIndexOffset;
		int numberShadows;
	};

	// Helper struct for easier padding
	struct alignas(16) PadVec3 {
		glm::vec3 m_value;

		constexpr PadVec3() : m_value(0.0f) {}
		constexpr PadVec3(float x, float y, float z) : m_value(x, y, z) {}
		constexpr PadVec3(const glm::vec3& v) : m_value(v) {}

		constexpr operator glm::vec3() const { return m_value; }

		constexpr PadVec3& operator=(const glm::vec3& v) {
			m_value = v;
			return *this;
		}

	private:
		float m_pad{ 0.0f };
	};

	//param.x==0...no light, param.x==1...point, param.x==2...directional, param.x==3...spotlight
	struct alignas(16) LightParams {
		PadVec3 color{ 1.0f, 0.0f, 0.0f };
		alignas(16) glm::vec4 params{ 0.0f, 1.0f, 10.0, 0.15f }; //x=type, y=intensity, z=power, w=ambient
		PadVec3 attenuation{ 1.0f, 0.01f, 0.005f }; //x=constant, y=linear, z=quadratic
	};

	//params.param.x==0...no light, params.param.x==1...point, params.param.x==2...directional, params.param.x==3...spotlight
	struct alignas(16) Light {
		PadVec3 	positionW{ 100.0f, 100.0f, 100.0f };
		PadVec3 	directionW{ -1.0f, -1.0f, -1.0f };
		LightParams lightParams;
	};

	struct LightOffset {
		int lightOffset;
		int numberLights;
	};

	struct UniformBufferFrame {
		alignas(16) CameraMatrix camera;
		alignas(16) glm::ivec3 numLights{ 1,0,0 }; //x=number point lights, y=number directional lights, z=number spotlights
		int padding; // Align to 16 bytes (matches Common.slang)
	};

	struct PushConstants {
		VkPipelineLayout layout;
		VkShaderStageFlags stageFlags;
		int offset;
		int size;
		const void* pValues;
	};

	//--------------------------------------------------------------------
	//Structures used to communicate with the helper layer

	/**
	 * @brief Holds Vulkan queue family indices
	 */
	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		/**
		 * @brief Check if all required queue families are found
		 * @return True if all families are available
		 */
		bool isComplete() {
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	/**
	 * @brief Holds swap chain support details
	 */
	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	/**
	 * @brief Depth image resources
	 */
	struct DepthImage {
		VkImage         m_depthImage;
		VmaAllocation   m_depthImageAllocation;
		VkImageView     m_depthImageView;
	};

	/**
	 * @brief Image resources including pixels and Vulkan handles
	 */
	struct Image {
		int 			m_width;
		int				m_height;
		int				m_layers;
		VkDeviceSize	m_size;
		void* m_pixels{ nullptr };
		VkImage         m_mapImage;
		VmaAllocation   m_mapImageAllocation;
		VkImageView     m_mapImageView;
		VkSampler       m_mapSampler;
	};

	struct Buffer {
		VkDeviceSize 				m_bufferSize{ 0 };
		std::vector<VkBuffer>       m_uniformBuffers;
		std::vector<VmaAllocation>  m_uniformBuffersAllocation;
		std::vector<void*>          m_uniformBuffersMapped;
	};

	struct DescriptorSet {
		int m_set{ 0 };
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

	// Deferred Renderer helpers
	struct GBufferImage {
		VkImage         m_gbufferImage{ VK_NULL_HANDLE };
		VmaAllocation   m_gbufferImageAllocation{ VK_NULL_HANDLE };
		VkImageView     m_gbufferImageView{ VK_NULL_HANDLE };
		VkFormat		m_gbufferFormat;
		VkSampler       m_gbufferSampler{ VK_NULL_HANDLE };
	};

	struct Material {
		glm::vec4 m_material{ 0.0f, 1.0f, 0.0f, 0.0f }; // x = metallic, y = roughness, zw will be ao in future
	};

	/**
	 * @brief Vertex data structure with multiple attribute types
	 *
	 * Pipeline code:
	 * P...Vertex data contains positions
	 * N...Vertex data contains normals
	 * T...Vertex data contains tangents
	 * C...Vertex data contains colors
	 * U...Vertex data contains texture UV coordinates
	 */
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

		/**
		 * @brief Get vertex data type string (e.g., "PNUC")
		 * @return String describing which attributes are present
		 */
		std::string getType() const {
			std::string name;
			if (m_positions.size() > 0) name = name + "P";
			if (m_normals.size() > 0)   name = name + "N";
			if (m_texCoords.size() > 0) name = name + "U";
			if (m_colors.size() > 0)    name = name + "C";
			if (m_tangents.size() > 0)  name = name + "T";
			return name;
		}

		VkDeviceSize getSize() const {
			return 	m_positions.size() * sizeof(glm::vec3) +
				m_normals.size() * sizeof(glm::vec3) +
				m_texCoords.size() * sizeof(glm::vec2) +
				m_colors.size() * sizeof(glm::vec4) +
				m_tangents.size() * sizeof(glm::vec3);
		}

		VkDeviceSize getSize(std::string type) const {
			return 	type.find("P") != std::string::npos ? m_positions.size() * sizeof(glm::vec3) : 0 +
				type.find("N") != std::string::npos ? m_normals.size() * sizeof(glm::vec3) : 0 +
				type.find("U") != std::string::npos ? m_texCoords.size() * sizeof(glm::vec2) : 0 +
				type.find("C") != std::string::npos ? m_colors.size() * sizeof(glm::vec4) : 0 +
				type.find("T") != std::string::npos ? m_tangents.size() * sizeof(glm::vec3) : 0;
		}

		std::vector<VkDeviceSize> getOffsets() const {
			size_t offset = 0;
			std::vector<VkDeviceSize> offsets{};
			if (size_t size = m_positions.size() * size_pos; size > 0) { offsets.push_back(offset); offset += size; }
			if (size_t size = m_normals.size() * size_nor; size > 0) { offsets.push_back(offset); offset += size; }
			if (size_t size = m_texCoords.size() * size_tex; size > 0) { offsets.push_back(offset); offset += size; }
			if (size_t size = m_colors.size() * size_col; size > 0) { offsets.push_back(offset); offset += size; }
			if (size_t size = m_tangents.size() * size_tan; size > 0) { offsets.push_back(offset); offset += size; }
			return offsets;
		}

		std::vector<VkDeviceSize> getOffsets(std::string type) const {
			size_t offset = 0;
			std::vector<VkDeviceSize> offsets{};
			if (type.find("P") != std::string::npos) { offsets.push_back(offset); offset += m_positions.size() * size_pos; }
			if (type.find("N") != std::string::npos) { offsets.push_back(offset); offset += m_normals.size() * size_nor; }
			if (type.find("U") != std::string::npos) { offsets.push_back(offset); offset += m_texCoords.size() * size_tex; }
			if (type.find("C") != std::string::npos) { offsets.push_back(offset); offset += m_colors.size() * size_col; }
			if (type.find("T") != std::string::npos) { offsets.push_back(offset); offset += m_tangents.size() * size_tan; }
			return offsets;
		}

		void copyData(void* data) {
			size_t offset = 0, size = 0;
			size = m_positions.size() * size_pos; memcpy(data, m_positions.data(), size);                 offset += size;
			size = m_normals.size() * size_nor; memcpy((char*)data + offset, m_normals.data(), size);   offset += size;
			size = m_texCoords.size() * size_tex; memcpy((char*)data + offset, m_texCoords.data(), size); offset += size;
			size = m_colors.size() * size_col; memcpy((char*)data + offset, m_colors.data(), size);    offset += size;
			size = m_tangents.size() * size_tan; memcpy((char*)data + offset, m_tangents.data(), size);  offset += size;
		}

		void copyData(void* data, std::string type) {
			size_t offset = 0, size = 0;
			if (type.find("P") != std::string::npos) { size = m_positions.size() * size_pos; memcpy(data, m_positions.data(), size); offset += size; }
			if (type.find("N") != std::string::npos) { size = m_normals.size() * size_nor; memcpy((char*)data + offset, m_normals.data(), size); offset += size; }
			if (type.find("U") != std::string::npos) { size = m_texCoords.size() * size_tex; memcpy((char*)data + offset, m_texCoords.data(), size); offset += size; }
			if (type.find("C") != std::string::npos) { size = m_colors.size() * size_col; memcpy((char*)data + offset, m_colors.data(), size); offset += size; }
			if (type.find("T") != std::string::npos) { size = m_tangents.size() * size_tan; memcpy((char*)data + offset, m_tangents.data(), size); offset += size; }
		}
	};

	/**
	 * @brief Mesh data with vertices, indices, and Vulkan buffers
	 */
	struct Mesh {
		VertexData				m_verticesData;
		std::vector<uint32_t>   m_indices;
		VkBuffer                m_vertexBuffer;
		VmaAllocation           m_vertexBufferAllocation;
		VkBuffer                m_indexBuffer;
		VmaAllocation           m_indexBufferAllocation;
	};


	/**
	 * @brief Semaphores for signalling that a command buffer has finished executing
	 *
	 * Every buffer gets its own Semaphore.
	 */
	struct Semaphores {
		std::vector<VkSemaphore> m_renderFinishedSemaphores;
	};

}

#include "VHBuffer.h"
#include "VHImage.h"
#include "VHDevice.h"
#include "VHSync.h"
#include "VHCommand.h"
#include "VHRender.h"
