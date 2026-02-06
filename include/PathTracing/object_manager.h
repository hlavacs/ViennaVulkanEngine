#pragma once

/**
 * @file object_manager.h
 * @brief Geometry, instance, and acceleration structure management.
 */

namespace vve {
	/** Manages mesh buffers, instances, and ray tracing acceleration structures. */
	class ObjectManager : public System {
		friend class Engine;
	private:
		DeviceBuffer<Vertex>* vertexBuffer;
		DeviceBuffer<uint32_t>* indexBuffer;
		std::vector<HostBuffer<vvh::Instance>*> instanceBuffers;

		int maxInstances = 1000;

		VkDevice device;
		VkPhysicalDevice physicalDevice;
		CommandManager* commandManager;

		VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProperties;

		std::vector<AccelStructure> m_blasAccel;
		AccelStructure m_tlasAccel;

		PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
		PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR = nullptr;
		PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
		PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
		PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;

		bool objectCreated = false;
		std::vector<bool> instanceChanged = std::vector<bool>(MAX_FRAMES_IN_FLIGHT, false);

		size_t numMeshes = 0;

		int currentFrame;

	public:

		/**
		 * @param systemName System identifier.
		 * @param engine Engine reference for messaging.
		 * @param device Logical device.
		 * @param physicalDevice Physical device for memory queries.
		 * @param commandManager Command manager for staging copies.
		 * @param m_asProperties Acceleration structure device properties.
		 */
		ObjectManager(std::string systemName, Engine& engine, VkDevice& device, VkPhysicalDevice& physicalDevice, CommandManager* commandManager, VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProperties);
		/** Release owned Vulkan resources. */
		~ObjectManager();
		/** Explicitly free Vulkan resources (destructor-safe). */
		void freeResources();
		/** Free BLAS resources. */
		void freeBlas();
		/** Free TLAS resources. */
		void freeTlas();
		
		/**
		 * Handle mesh creation messages.
		 * @param message Message payload.
		 */
		bool OnMeshCreated(Message message);
		/**
		 * Handle object changed messages.
		 * @param message Message payload.
		 */
		bool OnObjectChanged(Message message);
		/**
		 * Handle object creation messages.
		 * @param message Message payload.
		 */
		bool OnObjectCreated(Message message);
		/** Prepare geometry data for the next frame. */
		void prepareNextFrame();
		/**
		 * Record geometry updates for the next frame.
		 * @param message Message payload.
		 */
		bool OnRecordNextFrame(Message message);

		/** Create or rebuild the vertex buffer. */
		void createVertexBuffer();
		/** Create or rebuild the index buffer. */
		void createIndexBuffer();
		/** Create or rebuild instance buffers. */
		void createInstanceBuffers();

		/** @return Pointer to the vertex buffer. */
		DeviceBuffer<Vertex>* getVertexBuffer();
		/** @return Pointer to the index buffer. */
		DeviceBuffer<uint32_t>* getIndexBuffer();
		/** @return Instance buffers per frame. */
		std::vector<HostBuffer<vvh::Instance>*> getInstanceBuffers();

		/** Load ray tracing function pointers. */
		void loadRayTracingFunctions();

		/**
		 * Create an acceleration structure.
		 * @param out Output structure wrapper.
		 * @param createInfo Vulkan create info.
		 * @return True on success.
		 */
		bool createAcceleration(
			AccelStructure& out,
			VkAccelerationStructureCreateInfoKHR& createInfo);


		/**
		 * Build an acceleration structure.
		 * @param asType Type of AS (BLAS/TLAS).
		 * @param accelStruct Target AS wrapper.
		 * @param asGeometry Geometry description.
		 * @param asBuildRangeInfo Build range info.
		 * @param flags Build flags.
		 */
		void createAccelerationStructure(VkAccelerationStructureTypeKHR asType,  // The type of acceleration structure (BLAS or TLAS)
			AccelStructure& accelStruct,
			VkAccelerationStructureGeometryKHR& asGeometry,  // The geometry to build the acceleration structure from
			VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo,  // The range info for building the acceleration structure
			VkBuildAccelerationStructureFlagsKHR flags  // Build flags (e.g. prefer fast trace)
		);

		/**
		 * Convert mesh data to AS geometry structs.
		 * @param mesh Source mesh.
		 * @param geometry Output geometry.
		 * @param rangeInfo Output range info.
		 */
		void convertToAcelGeometry(vvh::Mesh& mesh, VkAccelerationStructureGeometryKHR& geometry, VkAccelerationStructureBuildRangeInfoKHR& rangeInfo);

		/** Build bottom-level acceleration structures. */
		void createBottomLevelAS();

		/** Build top-level acceleration structure. */
		void createTopLevelAS();


		/** @return TLAS wrapper. */
		AccelStructure getTlas();

		/**
		 * Update the current frame index.
		 * @param currentFrame Frame index.
		 */
		void updateCurrentFrame(int currentFrame);

		/** @return True if instance data changed. */
		bool instancesChanged();
		/** @return True if mesh data changed. */
		bool meshesChanged();


	};

}
