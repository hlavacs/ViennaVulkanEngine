#pragma once

namespace vvh::rt {
	// Per TLAS-instance material + geometry record. Must match InstanceData in
	// shaders/Raytracing/common.slang (std430 layout, 96 bytes).
	struct InstanceDataGpu {
		glm::vec4 ambient{0.0f};
		glm::vec4 diffuse{1.0f};
		glm::vec4 specular{0.0f};
		VkDeviceAddress vertexAddress{0};
		VkDeviceAddress indexAddress{0};
		glm::vec2 uvScale{1.0f};
		int32_t textureIndex{-1};
		uint32_t normalOffset{0};
		uint32_t uvOffset{0};
		uint32_t colorOffset{0};
		uint32_t flags{0};
		float reflectivity{0.0f};
	};

	enum class InstanceFlags : uint32_t {
		HasNormal = 0x1,
		HasUv = 0x2,
		HasVertexColor = 0x4,
		HasMaterialColor = 0x8,
	};

	/**
	 * @brief Camera data uploaded to the ray generation shader.
	 */
	struct CameraRT {
		glm::mat4 viewInverse;
		glm::mat4 projInverse;
		glm::vec4 cameraPos{0.0f}; // xyz = world space camera position
		glm::ivec4 numLights{0};   // x=point, y=directional, z=spot, w=total
	};
} // namespace vvh::rt
