#pragma once

namespace vve {
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

		ObjectManager(std::string systemName, Engine& engine, VkDevice& device, VkPhysicalDevice& physicalDevice, CommandManager* commandManager, VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProperties);

		bool OnMeshCreated(Message message);
		bool OnObjectChanged(Message message);
		bool OnObjectCreated(Message message);
		bool OnPrepareNextFrame(Message message);
		bool OnRecordNextFrame(Message message);

		void createVertexBuffer();
		void createIndexBuffer();
		void createInstanceBuffers();

		DeviceBuffer<Vertex>* getVertexBuffer();
		DeviceBuffer<uint32_t>* getIndexBuffer();
		std::vector<HostBuffer<vvh::Instance>*> getInstanceBuffers();

		void loadRayTracingFunctions();

		bool createAcceleration(
			AccelStructure& out,
			VkAccelerationStructureCreateInfoKHR& createInfo);


		void createAccelerationStructure(VkAccelerationStructureTypeKHR asType,  // The type of acceleration structure (BLAS or TLAS)
			AccelStructure& accelStruct,
			VkAccelerationStructureGeometryKHR& asGeometry,  // The geometry to build the acceleration structure from
			VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo,  // The range info for building the acceleration structure
			VkBuildAccelerationStructureFlagsKHR flags  // Build flags (e.g. prefer fast trace)
		);

		void convertToAcelGeometry(vvh::Mesh& mesh, VkAccelerationStructureGeometryKHR& geometry, VkAccelerationStructureBuildRangeInfoKHR& rangeInfo);

		void createBottomLevelAS();

		void createTopLevelAS();


		AccelStructure getTlas();

		void updateCurrentFrame(int currentFrame);

		bool instancesChanged();
		bool meshesChanged();


	};

}