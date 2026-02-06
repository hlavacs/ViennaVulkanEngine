#pragma once

/**
 * @file helper_structs.h
 * @brief Shared POD structs and helper declarations for path tracing.
 */

namespace vve {
    /** Core Vulkan handles shared across systems. */
    struct VulkanCoreObjects {
        VkInstance instance;
        VkSurfaceKHR surface;
        VkPhysicalDevice physicalDevice;
        VkDevice device;
        VkQueue graphicsQueue;
        VkQueue presentQueue;
    };

    /** GPU light source data used by shaders. */
    struct LightSource {
        glm::vec4 position;
        glm::vec4 emission;
        glm::vec4 direction;
        float radius;
        float pdf;
        float accumulativeSampleWeight;
        int32_t lightType;
    };

    /** Texture handle and its index within the descriptor set. */
    struct Texture {
        Image* image;
        uint32_t textureIndex;
    };

    /** Vertex format used by the renderer. */
    struct Vertex {
        glm::vec4 pos;
        glm::vec4 normal;
        glm::vec4 texCoord;
        glm::vec4 tangent;

        bool operator==(const Vertex& other) const {
            return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
        }
    };

    /** Renderable object metadata and its instances. */
    struct Object {
        uint32_t firstIndex;
        uint32_t indexCount;

        uint32_t vertexCount;

        uint32_t firstInstance = 0;
        uint32_t instanceCount = 0;

        uint32_t materialIndex = 0;

        std::vector<vvh::Instance*> instances;
    };

    /** Per-frame camera and RNG state for shaders. */
    struct UniformBufferObject {
        glm::mat4 view;
        glm::mat4 viewInv;
        glm::mat4 proj;
        glm::mat4 projInv;

        uint32_t seed;
        float _pad0[3];
    };

    /** Material slot that can be a texture or constant RGB. */
    struct MaterialSlotRGB {
        int32_t textureIndex;
        glm::vec3 rgb = glm::vec4(1.0f);

        MaterialSlotRGB(Texture* texture) : textureIndex(texture->textureIndex), rgb(glm::vec4(1.0f)) {}
        MaterialSlotRGB(glm::vec3 rgb) : textureIndex(-1), rgb(rgb) {}
        MaterialSlotRGB() : textureIndex(-1), rgb(glm::vec4(1.0f)) {}
    };

    /** Material slot that can be a texture or constant scalar. */
    struct MaterialSlotF {
        int32_t textureIndex;
        float f;

        MaterialSlotF(Texture* texture) : textureIndex(texture->textureIndex), f(0.5) {}
        MaterialSlotF(float f) : textureIndex(-1), f(f) {}
        MaterialSlotF() : textureIndex(-1), f(0.5f) {}
    };

    /** Material data packed for GPU access. */
    struct Material {
        glm::vec4 albedo;
        //index of -1 means no texture in use 
        int32_t albedoTextureIndex;
        int32_t normalTextureIndex;
        int32_t roughnessTextureIndex;
        int32_t metalnessTextureIndex;
        int32_t iorTextureIndex;
        int32_t alphaTextureIndex;

        float roughness;
        float metalness;
        float ior;
        float alpha;

        float _pad0[2];      // ensures that struct is a multiple of 16 byte (64)      
    };

    /** Convenience pairing of material index and material data. */
    struct MaterialInfo {
        uint32_t materialIndex;
        Material material;
    };

    /** @return Vertex input bindings for the renderer. */
    std::array<VkVertexInputBindingDescription, 2> getBindingDescriptions();

    /** @return Vertex input attributes for the renderer. */
    std::array<VkVertexInputAttributeDescription, 15> getAttributeDescriptions();

    /** Acceleration structure with its backing buffer. */
    struct AccelStructure {
        VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
        RawDeviceBuffer* buffer = nullptr;
    };

}
