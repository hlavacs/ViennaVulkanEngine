#pragma once

#include "VEGaussianAssetLoader.h"

// Forward declare VrdxSorter to avoid including vk_radix_sort.h in header.
// The actual header is included in .cpp file with VRDX_USE_VOLK to ensure
// vulkan_radix_sort uses volk's dynamically loaded function pointers.
struct VrdxSorter_T;
typedef VrdxSorter_T* VrdxSorter;

namespace vve
{

class RendererGaussian : public Renderer {
private:
    struct PushConstantGaussian {
        glm::mat4 model;
    };

    // GPU buffer storage for gaussian splatting pipeline
    // Per-frame buffers (indexed by currentFrame) prevent cross-frame corruption
    struct GaussianBuffers {
        // READ-ONLY buffers (shared across frames, initialized once)
        VkBuffer plyBuffer = VK_NULL_HANDLE;            // Raw PLY data + offsets
        VmaAllocation plyAllocation = VK_NULL_HANDLE;

        VkBuffer positionBuffer = VK_NULL_HANDLE;       // Parsed positions (N * 3 floats)
        VmaAllocation positionAllocation = VK_NULL_HANDLE;

        VkBuffer cov3dBuffer = VK_NULL_HANDLE;          // Covariance 3D (N * 6 floats)
        VmaAllocation cov3dAllocation = VK_NULL_HANDLE;

        VkBuffer opacityBuffer = VK_NULL_HANDLE;        // Opacity (N floats)
        VmaAllocation opacityAllocation = VK_NULL_HANDLE;

        VkBuffer shBuffer = VK_NULL_HANDLE;             // Spherical harmonics (N * 48 f16)
        VmaAllocation shAllocation = VK_NULL_HANDLE;

        VkBuffer infoBuffer = VK_NULL_HANDLE;           // Info uniform (point_count)
        VmaAllocation infoAllocation = VK_NULL_HANDLE;

        VkBuffer splatIndexBuffer = VK_NULL_HANDLE;     // Index buffer for quad rendering (6 indices per splat)
        VmaAllocation splatIndexAllocation = VK_NULL_HANDLE;

        // Write buffers (updated each frame with vkQueueWaitIdle synchronization)
        // Educational approach: simpler than per-frame buffer sets
        VkBuffer keyBuffer = VK_NULL_HANDLE;            // Depth keys for sorting (N uint32)
        VmaAllocation keyAllocation = VK_NULL_HANDLE;

        VkBuffer indexBuffer = VK_NULL_HANDLE;          // Sorted indices (N uint32)
        VmaAllocation indexAllocation = VK_NULL_HANDLE;

        VkBuffer drawIndirectBuffer = VK_NULL_HANDLE;   // Draw indirect commands
        VmaAllocation drawIndirectAllocation = VK_NULL_HANDLE;

        VkBuffer instancesBuffer = VK_NULL_HANDLE;      // Instance data for rendering (N * 12 vec4)
        VmaAllocation instancesAllocation = VK_NULL_HANDLE;

        VkBuffer inverseMapBuffer = VK_NULL_HANDLE;     // Inverse index map (N int32)
        VmaAllocation inverseMapAllocation = VK_NULL_HANDLE;

        VkBuffer visibleCountBuffer = VK_NULL_HANDLE;   // Visible point count (1 uint32)
        VmaAllocation visibleCountAllocation = VK_NULL_HANDLE;

        uint32_t pointCount = 0;
    };

public:
    RendererGaussian(const std::string& systemName, Engine& engine, const std::string& windowName);
    virtual ~RendererGaussian();

private:
    // Message callbacks
    bool OnInit(const Message& message);
    bool OnPrepareNextFrame(const Message& message);
    bool OnRecordNextFrame(const Message& message);
    bool OnRenderNextFrame(const Message& message);
    bool OnObjectCreate(Message& message);
    bool OnObjectDestroy(Message& message);
    bool OnQuit(const Message& message);

    // Pipeline creation
    void CreateComputePipelines();
    void CreateGraphicsPipeline();
    void DestroyPipelines();

    // Descriptor management
    void CreateDescriptorPool();
    void AllocateDescriptorSets();
    void UpdateDescriptorSets();
    void DestroyDescriptors();

    // Buffer management
    void CreateCameraBuffers();
    void DestroyCameraBuffers();
    void CreateGaussianBuffers(const PLYData& plyData, GaussianBuffers& outBuffers);
    void DestroyGaussianBuffers(GaussianBuffers& buffers);
    void AllocateGaussianDescriptorSets(size_t objectIndex);

    // Depth sort
    void InitializeDepthSort();
    void CleanupDepthSort();

    // Command buffers
    void CreateCommandBuffers();
    void DestroyCommandBuffers();


private:
    // Vulkan resources
    VkRenderPass m_renderPass{ VK_NULL_HANDLE };
    VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };

    // Descriptor set layouts (4 sets for VKGS shaders)
    VkDescriptorSetLayout m_cameraDescriptorLayout{ VK_NULL_HANDLE };      // Set 0: Camera
    VkDescriptorSetLayout m_gaussianDataDescriptorLayout{ VK_NULL_HANDLE }; // Set 1: Gaussian data
    VkDescriptorSetLayout m_outputDescriptorLayout{ VK_NULL_HANDLE };      // Set 2: Output buffers
    VkDescriptorSetLayout m_plyDescriptorLayout{ VK_NULL_HANDLE };         // Set 3: PLY raw data

    // Descriptor sets
    std::vector<VkDescriptorSet> m_cameraDescriptorSets;      // Set 0: Camera (per frame)
    std::vector<VkDescriptorSet> m_gaussianDataDescriptorSets; // Set 1: Gaussian data (per object, read-only)
    std::vector<VkDescriptorSet> m_outputDescriptorSets;      // Set 2: Output buffers (per object)
    std::vector<VkDescriptorSet> m_plyDescriptorSets;         // Set 3: PLY raw data (per object, read-only)

    // Compute pipelines (VKGS stages)
    VkPipeline m_parsePlyPipeline{ VK_NULL_HANDLE };
    VkPipeline m_rankPipeline{ VK_NULL_HANDLE };
    VkPipeline m_inverseIndexPipeline{ VK_NULL_HANDLE };
    VkPipeline m_projectionPipeline{ VK_NULL_HANDLE };
    VkPipelineLayout m_computePipelineLayout{ VK_NULL_HANDLE };

    // Graphics pipeline (splat rendering)
    VkPipeline m_splatPipeline{ VK_NULL_HANDLE };
    VkPipelineLayout m_graphicsPipelineLayout{ VK_NULL_HANDLE };

    // Shader modules
    VkShaderModule m_parsePlyShader{ VK_NULL_HANDLE };
    VkShaderModule m_rankShader{ VK_NULL_HANDLE };
    VkShaderModule m_inverseIndexShader{ VK_NULL_HANDLE };
    VkShaderModule m_projectionShader{ VK_NULL_HANDLE };
    VkShaderModule m_splatVertShader{ VK_NULL_HANDLE };
    VkShaderModule m_splatFragShader{ VK_NULL_HANDLE };

    // Command buffers
    std::vector<VkCommandPool> m_commandPools;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // Gaussian data
    std::vector<GaussianBuffers> m_gaussianObjects;
    std::unordered_map<ObjectHandle, size_t> m_objectToBufferIndex;  // Maps ObjectHandle to index in m_gaussianObjects

    // Camera uniform buffer (per frame-in-flight)
    std::vector<VkBuffer> m_cameraBuffers;
    std::vector<VmaAllocation> m_cameraAllocations;

    // Radix sort for depth ordering
    VrdxSorter m_radixSorter = VK_NULL_HANDLE;
    std::vector<VkBuffer> m_radixStorageBuffers;
    std::vector<VmaAllocation> m_radixStorageAllocations;

    // Fences for frame synchronization (prevents buffer overwrite while GPU is reading)
    std::vector<VkFence> m_renderFences;
};

}  // namespace vve