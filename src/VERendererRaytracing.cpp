                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   #include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

	//-------------------------------------------------------------------------------------------------------
	// Vulkan Ray Tracing Renderer
	//
	// NOTE: This is a bare skeleton that mirrors the structure of RendererVulkan but
	// targets a hardware accelerated ray tracing pipeline. The individual setup steps
	// (acceleration structures, RT pipeline, shader binding table) are stubbed out and
	// must be filled in. They document the intended sequence:
	//   1. Enable ray tracing extensions          -> OnExtensions / m_deviceExtensions
	//   2. Build acceleration structures           -> CreateAccelerationStructures
	//   3. Create ray tracing pipeline + SBT        -> CreateRayTracingPipeline / CreateShaderBindingTable
	//   4. Use ray tracing shaders written in Slang -> m_raygen/miss/closestHitShaderPath

    /**
     * @brief Constructs the ray tracing renderer and registers event callbacks.
     * @param systemName Name of the renderer system.
     * @param engine Reference to the engine instance.
     * @param windowName Name of the window to render to.
     */
    RendererRaytracing::RendererRaytracing(std::string systemName, Engine& engine, std::string windowName)
        : Renderer(systemName, engine, windowName) {

        engine.RegisterCallbacks( {
			{this,      0, "EXTENSIONS", [this](Message& message){ return OnExtensions(message);} },
			{this,   1000, "INIT", [this](Message& message){ return OnInit(message);} },
			{this,      0, "PREPARE_NEXT_FRAME", [this](Message& message){ return OnPrepareNextFrame(message);} },
			{this,      0, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} },
			{this,      0, "RENDER_NEXT_FRAME", [this](Message& message){ return OnRenderNextFrame(message);} },
			{this,   1000, "TEXTURE_CREATE",   [this](Message& message){ return OnTextureCreate(message);} },
			{this,      0, "TEXTURE_DESTROY",  [this](Message& message){ return OnTextureDestroy(message);} },
			{this,      0, "MESH_CREATE",  [this](Message& message){ return OnMeshCreate(message);} },
			{this,      0, "MESH_DESTROY", [this](Message& message){ return OnMeshDestroy(message);} },
			{this,   2000, "QUIT", [this](Message& message){ return OnQuit(message);} },
		} );
    }

    /**
     * @brief Destructor for the ray tracing renderer.
     */
    RendererRaytracing::~RendererRaytracing() {}

    /**
     * @brief (1) Append the ray tracing instance/device extensions to the requested set.
     * @param message Message containing extension requirements.
     * @return false to continue message propagation.
     */
    bool RendererRaytracing::OnExtensions(Message message) {
		auto msg = message.GetData<MsgExtensions>();
		m_instanceExtensions.insert(m_instanceExtensions.end(), msg.m_instExt.begin(), msg.m_instExt.end());
		m_deviceExtensions.insert(m_deviceExtensions.end(), msg.m_devExt.begin(), msg.m_devExt.end());
		return false;
	}

    /**
     * @brief Initializes the ray tracing renderer: device, acceleration structures and RT pipeline.
     * @param message Initialization message.
     * @return false to continue message propagation.
     */
    bool RendererRaytracing::OnInit(Message message) {
		Renderer::OnInit(message);

		// TODO: Instance / surface / physical-device / logical-device / VMA / swap chain
		//       creation is identical to RendererVulkan::OnInit and should be reused here
		//       (only the requested extensions and enabled features differ).

		// (1) Query ray tracing capabilities of the selected device.
		InitRayTracingProperties();

		// (2) Build bottom- and top-level acceleration structures from loaded meshes.
		CreateAccelerationStructures();

		// (3) Create the ray tracing pipeline and the shader binding table.
		CreateRayTracingPipeline();
		CreateShaderBindingTable();

		// Storage image the raygen shader writes into before presenting.
		CreateStorageImage();
		return false;
    }

    /**
     * @brief (1) Query ray tracing pipeline properties and acceleration structure features.
     */
    void RendererRaytracing::InitRayTracingProperties() {
		// TODO: chain m_rtPipelineProperties into VkPhysicalDeviceProperties2 and call
		// vkGetPhysicalDeviceProperties2 to obtain shaderGroupHandleSize / alignment, etc.
    }

    /**
     * @brief (2) Build the bottom- and top-level acceleration structures.
     */
    void RendererRaytracing::CreateAccelerationStructures() {
		// TODO:
		//  - For every mesh build a BLAS from its vertex/index buffers
		//    (VkAccelerationStructureGeometryKHR with VK_GEOMETRY_TYPE_TRIANGLES_KHR).
		//  - Build a single TLAS referencing the BLAS instances with their transforms.
		//  - Use vkGetAccelerationStructureBuildSizesKHR + vkCmdBuildAccelerationStructuresKHR.
    }

    /**
     * @brief (3) Create the ray tracing pipeline (raygen/miss/closest-hit shader groups).
     */
    void RendererRaytracing::CreateRayTracingPipeline() {
		// TODO:
		//  - Create descriptor set layout (TLAS binding + storage image binding + UBOs).
		//  - Load the Slang-compiled SPIR-V modules (m_raygen/miss/closestHitShaderPath).
		//  - Fill m_shaderGroups (general groups for raygen/miss, triangles-hit group for chit).
		//  - Call vkCreateRayTracingPipelinesKHR with maxPipelineRayRecursionDepth.
    }

    /**
     * @brief (3) Create the shader binding table for the ray tracing pipeline.
     */
    void RendererRaytracing::CreateShaderBindingTable() {
		// TODO:
		//  - Query handles via vkGetRayTracingShaderGroupHandlesKHR.
		//  - Copy raygen/miss/hit handles into device-address-accessible buffers.
		//  - Fill m_raygenSBT/m_missSBT/m_hitSBT.m_region (address + stride + size).
    }

    /**
     * @brief Create the storage image the ray generation shader writes into.
     */
    void RendererRaytracing::CreateStorageImage() {
		// TODO: create a VK_IMAGE_USAGE_STORAGE_BIT image matching the swap chain extent,
		// transitioned to VK_IMAGE_LAYOUT_GENERAL, and bound to the RT descriptor set.
    }

    /**
     * @brief Prepares the next frame by acquiring the next swap chain image.
     * @param message Frame preparation message.
     * @return false to continue message propagation.
     */
    bool RendererRaytracing::OnPrepareNextFrame(Message message) {
		// TODO: acquire next swap chain image (see RendererVulkan::OnPrepareNextFrame).
		return false;
    }

    /**
     * @brief Records ray tracing commands for the next frame.
     * @param message Frame recording message.
     * @return false to continue message propagation.
     */
    bool RendererRaytracing::OnRecordNextFrame(Message message) {
		// TODO:
		//  - vkCmdBindPipeline(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline).
		//  - Bind descriptor sets, then vkCmdTraceRaysKHR with the SBT regions.
		//  - Blit/copy the storage image into the current swap chain image.
		return false;
	}

    /**
     * @brief Submits command buffers and presents the rendered frame.
     * @param message Frame rendering message.
     * @return false to continue message propagation.
     */
    bool RendererRaytracing::OnRenderNextFrame(Message message) {
		// TODO: submit command buffers and present (see RendererVulkan::OnRenderNextFrame).
		return false;
    }

    /**
     * @brief Cleans up all ray tracing resources when shutting down the renderer.
     * @param message Quit message.
     * @return false to continue message propagation.
     */
    bool RendererRaytracing::OnQuit(Message message) {
		// TODO:
		//  - Destroy SBT buffers, RT pipeline + layout, descriptor pool/layout.
		//  - Destroy TLAS/BLAS (vkDestroyAccelerationStructureKHR + buffers).
		//  - Destroy storage image, command pool, sync objects, device, instance.
		return false;
    }

	//-------------------------------------------------------------------------------------------------------

	/**
	 * @brief Creates a texture from image data.
	 * @param message Message containing texture creation parameters.
	 * @return false to continue message propagation.
	 */
	bool RendererRaytracing::OnTextureCreate( Message message ) {
		// TODO: upload texture (identical to RendererVulkan::OnTextureCreate).
		return false;
	}

	/**
	 * @brief Destroys a texture and frees associated resources.
	 * @param message Message containing texture handle to destroy.
	 * @return false to continue message propagation.
	 */
	bool RendererRaytracing::OnTextureDestroy( Message message ) {
		// TODO: destroy texture (identical to RendererVulkan::OnTextureDestroy).
		return false;
	}

	/**
	 * @brief Creates vertex/index buffers for a mesh and flags BLAS rebuild.
	 * @param message Message containing mesh creation parameters.
	 * @return false to continue message propagation.
	 */
	bool RendererRaytracing::OnMeshCreate( Message message ) {
		// TODO: create vertex/index buffers (with VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT
		// and shader-device-address usage), then (re)build the corresponding BLAS/TLAS.
		return false;
	}

	/**
	 * @brief Destroys a mesh and frees its buffers / acceleration structures.
	 * @param message Message containing mesh handle to destroy.
	 * @return false to continue message propagation.
	 */
	bool RendererRaytracing::OnMeshDestroy( Message message ) {
		// TODO: destroy buffers and the associated BLAS, then rebuild the TLAS.
		return false;
	}


};   // namespace vve
