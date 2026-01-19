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
public:
    // GPU timestamp queries (in nanoseconds)
    // Reference: VKGS engine.cc:1039-1044
    struct FrameTimings {
        uint64_t rankTime = 0;           // Frustum culling + depth key generation
        uint64_t sortTime = 0;           // Radix sort (back-to-front ordering)
        uint64_t inverseIndexTime = 0;   // Inverse mapping creation
        uint64_t projectionTime = 0;     // Screen-space projection
        uint64_t renderTime = 0;         // Graphics pipeline (splat drawing)
        uint64_t totalTime = 0;          // End-to-end frame time
        float timestampPeriod = 0.0f;    // Nanoseconds per timestamp tick (from device limits)
        bool valid = false;              // True if timestamps were recorded
    };

    // IBL generation timing (thesis contribution measurement)
    struct IBLTimings {
        uint64_t cubemapTime = 0;        // Rendering 6 cubemap faces from gaussians
        uint64_t irradianceTime = 0;     // Irradiance filtering (convolution or box)
        uint64_t totalTime = 0;          // Total IBL generation time
        float timestampPeriod = 0.0f;    // Nanoseconds per timestamp tick
        bool valid = false;              // True if timestamps were recorded
    };

private:
    struct PushConstantGaussian {
        glm::mat4 model;
    };

    // GPU buffer storage for gaussian splatting pipeline
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

        VkBuffer instancesBuffer = VK_NULL_HANDLE;      // Instance data for rendering (N * 3 vec4: position, rot_scale, color/opacity)
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

    // Generate cubemap IBL from gaussian environment (for dynamic light probes)
    void GenerateCubemapIBL(size_t gaussianObjectIndex = 0);

    // Get irradiance cubemap resources for PBR lighting (called by deferred renderer)
    VkImageView GetIrradianceView() const { return m_irradianceView; }
    VkSampler GetIrradianceSampler() const { return m_envCubemapSampler; }

    // Configure cubemap resolutions (must be called before CreateCubemapResources in OnInit)
    // Warning: Calling after initialization has no effect until resources are recreated
    void SetCubemapResolutions(uint32_t envResolution, uint32_t irradianceResolution) {
        m_cubemapResolution = envResolution;
        m_irradianceResolution = irradianceResolution;
    }

    uint32_t GetCubemapResolution() const { return m_cubemapResolution; }
    uint32_t GetIrradianceResolution() const { return m_irradianceResolution; }

    // Configure cubemap clear color (ambient fallback when no gaussians visible)
    void SetCubemapClearColor(const glm::vec3& color) { m_cubemapClearColor = color; }

    // Configure irradiance filter type
    // TRUE: Convolution filter (cosine-weighted hemisphere sampling, slow)
    // FALSE: Box filter (hardware downsampling, fast)
    void SetUseConvolutionFilter(bool enable) { m_useConvolutionFilter = enable; }
    bool GetUseConvolutionFilter() const { return m_useConvolutionFilter; }

    // GPU timestamp queries for performance evaluation
    // Enable/disable timestamp recording (default: enabled)
    void SetTimestampQueriesEnabled(bool enable) { m_timestampQueriesEnabled = enable; }
    bool GetTimestampQueriesEnabled() const { return m_timestampQueriesEnabled; }
    // Get timing results from previous frame (read in OnPrepareNextFrame)
    const FrameTimings& GetFrameTimings() const { return m_frameTimings; }
    // Get IBL timing results (updated after each GenerateCubemapIBL call)
    const IBLTimings& GetIBLTimings() const { return m_iblTimings; }

private:
    // Message callbacks
    bool OnInit(const Message& message);
    bool OnPrepareNextFrame(const Message& message);
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

    // Cubemap IBL
    void CreateCubemapResources();
    void DestroyCubemapResources();
    void RenderEnvironmentToCubemap(VkCommandBuffer cmdBuffer, size_t gaussianObjectIndex);
    void ConvolveIrradianceMap(VkCommandBuffer cmdBuffer);


private:
    // Vulkan resources
    VkRenderPass m_renderPass{ VK_NULL_HANDLE };
    VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };

    // Descriptor set layouts (4 sets for VKGS shaders)
    VkDescriptorSetLayout m_cameraDescriptorLayout{ VK_NULL_HANDLE };      // Set 0: Camera
    VkDescriptorSetLayout m_gaussianDataDescriptorLayout{ VK_NULL_HANDLE }; // Set 1: Gaussian data
    VkDescriptorSetLayout m_outputDescriptorLayout{ VK_NULL_HANDLE };      // Set 2: Output buffers
    VkDescriptorSetLayout m_plyDescriptorLayout{ VK_NULL_HANDLE };         // Set 3: PLY raw data
    VkDescriptorSetLayout m_irradianceDescriptorLayout{ VK_NULL_HANDLE };  // Irradiance: env+irradiance cubemaps

    // Descriptor sets
    std::vector<VkDescriptorSet> m_cameraDescriptorSets;      // Set 0: Camera (per frame)
    std::vector<VkDescriptorSet> m_gaussianDataDescriptorSets; // Set 1: Gaussian data (per object, read-only)
    std::vector<VkDescriptorSet> m_outputDescriptorSets;      // Set 2: Output buffers (per object)
    std::vector<VkDescriptorSet> m_plyDescriptorSets;         // Set 3: PLY raw data (per object, read-only)
    VkDescriptorSet m_irradianceDescriptorSet{ VK_NULL_HANDLE }; // Irradiance: env+irradiance cubemaps (single set)

    // Compute pipelines (VKGS stages)
    VkPipeline m_parsePlyPipeline{ VK_NULL_HANDLE };
    VkPipeline m_rankPipeline{ VK_NULL_HANDLE };
    VkPipeline m_inverseIndexPipeline{ VK_NULL_HANDLE };
    VkPipeline m_projectionPipeline{ VK_NULL_HANDLE };
    VkPipeline m_irradiancePipeline{ VK_NULL_HANDLE };         // Convolution pipeline (slow, high quality)
    VkPipeline m_boxFilterPipeline{ VK_NULL_HANDLE };          // Box filter pipeline (fast, lower quality)
    VkPipelineLayout m_computePipelineLayout{ VK_NULL_HANDLE };
    VkPipelineLayout m_irradiancePipelineLayout{ VK_NULL_HANDLE };

    // Graphics pipeline (splat rendering)
    VkPipeline m_splatPipeline{ VK_NULL_HANDLE };
    VkPipelineLayout m_graphicsPipelineLayout{ VK_NULL_HANDLE };

    // Shader modules
    VkShaderModule m_parsePlyShader{ VK_NULL_HANDLE };
    VkShaderModule m_rankShader{ VK_NULL_HANDLE };
    VkShaderModule m_inverseIndexShader{ VK_NULL_HANDLE };
    VkShaderModule m_projectionShader{ VK_NULL_HANDLE };
    VkShaderModule m_irradianceShader{ VK_NULL_HANDLE };       // Convolution shader
    VkShaderModule m_boxFilterShader{ VK_NULL_HANDLE };        // Box filter shader
    VkShaderModule m_splatVertShader{ VK_NULL_HANDLE };
    VkShaderModule m_splatFragShader{ VK_NULL_HANDLE };

    // Command buffers
    std::vector<VkCommandPool> m_commandPools;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // Gaussian data
    std::vector<GaussianBuffers> m_gaussianObjects;
    std::vector<std::pair<ObjectHandle, size_t>> m_objectToBufferIndex;  // Maps ObjectHandle to index in m_gaussianObjects (linear search ok for small count)

    // Camera uniform buffer (per frame-in-flight)
    std::vector<VkBuffer> m_cameraBuffers;
    std::vector<VmaAllocation> m_cameraAllocations;
    glm::mat4 m_viewMatrix{1.0f};  // Stored from OnPrepareNextFrame for push constants

    // Radix sort for depth ordering
    VrdxSorter m_radixSorter = VK_NULL_HANDLE;
    std::vector<VkBuffer> m_radixStorageBuffers;
    std::vector<VmaAllocation> m_radixStorageAllocations;

    // Cubemap IBL resources (for dynamic light probe extraction)
    VkImage m_envCubemap{ VK_NULL_HANDLE };                    // Environment cubemap (6 faces, rendered from gaussians)
    VmaAllocation m_envCubemapAllocation{ VK_NULL_HANDLE };
    VkImageView m_envCubemapView{ VK_NULL_HANDLE };
    VkSampler m_envCubemapSampler{ VK_NULL_HANDLE };           // Sampler for environment cubemap
    std::vector<VkImageView> m_envCubemapFaceViews;            // Per-face views (6) for rendering

    VkImage m_irradianceMap{ VK_NULL_HANDLE };                 // Diffuse irradiance (convolved from env cubemap)
    VmaAllocation m_irradianceAllocation{ VK_NULL_HANDLE };
    VkImageView m_irradianceView{ VK_NULL_HANDLE };

    // Cubemap resolutions (configurable via SetCubemapResolutions before OnInit)
    uint32_t m_cubemapResolution = 512;
    uint32_t m_irradianceResolution = 32;
    glm::vec3 m_cubemapClearColor = glm::vec3(0.3f, 0.3f, 0.3f);
    bool m_useConvolutionFilter = true;                   // TRUE: convolution (slow), FALSE: box filter (fast)

    // GPU timestamp queries for performance evaluation
    // Reference: VKGS engine.cc:472-478 (query pool creation), 1031-1044 (timestamp reading)
    // Simplified to single pool since we use vkQueueWaitIdle (VKGS uses 2 pools with fences)
    static constexpr uint32_t TIMESTAMP_COUNT = 12;       // 12 timestamps like VKGS (indices 0-11)
    std::vector<VkQueryPool> m_timestampQueryPools;       // Single query pool (vector for consistency)
    FrameTimings m_frameTimings;                          // Latest timing results
    bool m_timestampQueriesEnabled = true;

    // IBL generation timing (thesis contribution)
    static constexpr uint32_t IBL_TIMESTAMP_COUNT = 4;    // 4 timestamps: start, after cubemap, after irradiance, end
    VkQueryPool m_iblQueryPool = VK_NULL_HANDLE;
    IBLTimings m_iblTimings;
};
}  // namespace vve