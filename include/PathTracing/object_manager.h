#pragma once

namespace vve {
	class ObjectManager {
	private:
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<Object*> objects;

		DeviceBuffer<Vertex>* vertexBuffer;
		DeviceBuffer<uint32_t>* indexBuffer;

		std::vector<HostBuffer<Instance>*> instanceBuffers;


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

	public:

		ObjectManager(VkDevice& device, VkPhysicalDevice& physicalDevice, CommandManager* commandManager, VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProperties);

		Object* loadModel(std::string path, MaterialInfo* material);
		void createVertexBuffer();
		void createIndexBuffer();

		Instance* addInstance(Object* object);
		//Instance* addInstanceb(Object* object, glm::mat4 model);
		std::vector<Instance> accumulateInstances();
		void createInstanceBuffers();
		void updateInstanceBuffer(const int currentImage);

		std::vector<Object*> getobjects();
		DeviceBuffer<Vertex>* getVertexBuffer();
		DeviceBuffer<uint32_t>* getIndexBuffer();
		std::vector<HostBuffer<Instance>*> getInstanceBuffers();

		void loadRayTracingFunctions()
		{
			vkCmdBuildAccelerationStructuresKHR =
				(PFN_vkCmdBuildAccelerationStructuresKHR)
				vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");

			vkBuildAccelerationStructuresKHR =
				(PFN_vkBuildAccelerationStructuresKHR)
				vkGetDeviceProcAddr(device, "vkBuildAccelerationStructuresKHR");

			vkCreateAccelerationStructureKHR =
				(PFN_vkCreateAccelerationStructureKHR)
				vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");

			vkGetAccelerationStructureBuildSizesKHR =
				(PFN_vkGetAccelerationStructureBuildSizesKHR)
				vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");

			vkCmdTraceRaysKHR =
				(PFN_vkCmdTraceRaysKHR)
				vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");

			// (Load any others you use)
		}

		bool createAcceleration(
			AccelStructure& out,
			VkAccelerationStructureCreateInfoKHR& createInfo)
		{
			out.buffer = new RawDeviceBuffer(
				createInfo.size,
				VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
				VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
				commandManager,
				device,
				physicalDevice
			);

			createInfo.buffer = out.buffer->getBuffer();
			createInfo.offset = 0;

			if (vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &out.accel) != VK_SUCCESS)
			{
				delete out.buffer;
				out.buffer = nullptr;
				return false;
			}

			return true;
		}


		void createAccelerationStructure(VkAccelerationStructureTypeKHR asType,  // The type of acceleration structure (BLAS or TLAS)
			AccelStructure& accelStruct,
			VkAccelerationStructureGeometryKHR& asGeometry,  // The geometry to build the acceleration structure from
			VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo,  // The range info for building the acceleration structure
			VkBuildAccelerationStructureFlagsKHR flags  // Build flags (e.g. prefer fast trace)
		)
		{
			// Helper function to align a value to a given alignment
			auto alignUp = [](auto value, size_t alignment) noexcept { return ((value + alignment - 1) & ~(alignment - 1)); };

			// Fill the build information with the current information, the rest is filled later (scratch buffer and destination AS)
			VkAccelerationStructureBuildGeometryInfoKHR asBuildInfo{};
			asBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
			asBuildInfo.type = asType;  // The type of acceleration structure (BLAS or TLAS)
			asBuildInfo.flags = flags;   // Build flags (e.g. prefer fast trace)
			asBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;  // Build mode vs update
			asBuildInfo.geometryCount = 1;                                               // Deal with one geometry at a time
			asBuildInfo.pGeometries = &asGeometry;  // The geometry to build the acceleration structure from


			// One geometry at a time (could be multiple)
			std::vector<uint32_t> maxPrimCount(1);
			maxPrimCount[0] = asBuildRangeInfo.primitiveCount;

			// Find the size of the acceleration structure and the scratch buffer
			VkAccelerationStructureBuildSizesInfoKHR asBuildSize{};
			asBuildSize.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
			vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &asBuildInfo, maxPrimCount.data(), &asBuildSize);

			// Make sure the scratch buffer is properly aligned
			VkDeviceSize scratchSize = alignUp(asBuildSize.buildScratchSize, m_asProperties.minAccelerationStructureScratchOffsetAlignment);

			// Create the scratch buffer to store the temporary data for the build

			GenericBuffer* scratchBuffer = new GenericBuffer(
				scratchSize,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
				VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // must pass a real flag
				VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
				device,
				physicalDevice
			);

			// Create the acceleration structure
			VkAccelerationStructureCreateInfoKHR createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
			createInfo.size = asBuildSize.accelerationStructureSize;  // The size of the acceleration structure
			createInfo.type = asType;  // The type of acceleration structure (BLAS or TLAS)

			createAcceleration(accelStruct, createInfo);

			// Build the acceleration structure
			{
				VkCommandBuffer cmd = commandManager->beginSingleTimeCommand();

				// Fill with new information for the build,scratch buffer and destination AS
				asBuildInfo.dstAccelerationStructure = accelStruct.accel;
				asBuildInfo.scratchData.deviceAddress = scratchBuffer->getDeviceAddress();

				VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &asBuildRangeInfo;
				vkCmdBuildAccelerationStructuresKHR(cmd, 1, &asBuildInfo, &pBuildRangeInfo);

				commandManager->endSingleTimeCommand(cmd);
			}
			// Cleanup the scratch buffer
			delete scratchBuffer;
		}

		void convertToAcelGeometry(Object* object, VkAccelerationStructureGeometryKHR& geometry, VkAccelerationStructureBuildRangeInfoKHR& rangeInfo) {
			uint32_t triangleCount = object->indexCount / 3;

			// Triangles data
			VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
			triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
			//change format once propper vertex data is used
			triangles.vertexFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
			triangles.vertexStride = sizeof(Vertex);
			triangles.maxVertex = object->vertexCount;

			// GLOBAL vertex buffer – no per-object offset needed
			triangles.vertexData.deviceAddress = vertexBuffer->getDeviceAddress();

			// GLOBAL index buffer – we point into the correct range using firstIndex
			triangles.indexType = VK_INDEX_TYPE_UINT32;
			triangles.indexData.deviceAddress =
				indexBuffer->getDeviceAddress() + object->firstIndex * sizeof(uint32_t);

			triangles.transformData.deviceAddress = 0; // no per-primitive transforms

			// Wrap geometry
			geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
			geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
			geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
			geometry.geometry.triangles = triangles;

			// Build range
			rangeInfo.primitiveCount = triangleCount;
			rangeInfo.primitiveOffset = 0;
			rangeInfo.firstVertex = 0; // only used for VK_INDEX_TYPE_NONE_KHR
			rangeInfo.transformOffset = 0;
		}

		void createBottomLevelAS()
		{
			// Prepare geometry information for all meshes
			m_blasAccel.resize(objects.size());

			for (uint32_t blasId = 0; blasId < objects.size(); blasId++)
			{
				VkAccelerationStructureGeometryKHR       asGeometry{};
				VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};

				// Convert the primitive information to acceleration structure geometry
				convertToAcelGeometry(objects[blasId], asGeometry, asBuildRangeInfo);

				createAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, m_blasAccel[blasId], asGeometry,
					asBuildRangeInfo, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
			}
		}

		void createTopLevelAS()
		{
			// VkTransformMatrixKHR is row-major 3x4, glm::mat4 is column-major; transpose before memcpy.
			/*
			auto toTransformMatrixKHR = [](const glm::mat4& m) {
				VkTransformMatrixKHR t;
				memcpy(&t, glm::value_ptr(glm::transpose(m)), sizeof(t));
				return t;
				};
			*/
			


			auto toTransformMatrixKHR = [](const glm::mat4& m) {
				VkTransformMatrixKHR t{};
				glm::mat4 tm = glm::transpose(m);

				for (int row = 0; row < 3; ++row)
					for (int col = 0; col < 4; ++col)
						t.matrix[row][col] = tm[col][row];

				return t;
			};

			// Prepare instance data for TLAS
			std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
			for (int i = 0; i < objects.size(); i++) {

				for (Instance* instance : objects[i]->instances)
				{
					VkAccelerationStructureInstanceKHR asInstance{};
					asInstance.transform = toTransformMatrixKHR(instance->model);  // Position of the instance
					asInstance.instanceCustomIndex = i;                       // gl_InstanceCustomIndexEXT
					asInstance.accelerationStructureReference = m_blasAccel[i].buffer->getDeviceAddress();  // Will be set in Phase 3
					asInstance.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
					asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;  // No culling - double sided
					asInstance.mask = 0xFF;
					asInstance.instanceCustomIndex = objects[i]->materialIndex;
					tlasInstances.emplace_back(asInstance);
				}
			}

			// Then create the buffer with the instance data
			DeviceBuffer<VkAccelerationStructureInstanceKHR>* tlasInstancesBuffer = new DeviceBuffer<VkAccelerationStructureInstanceKHR>(tlasInstances.data(), tlasInstances.size(), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, commandManager, device, physicalDevice);

			// Then create the TLAS geometry
			{
				VkAccelerationStructureGeometryKHR asGeometry{};
				VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};

				// Convert the instance information to acceleration structure geometry, similar to primitiveToGeometry()
				VkAccelerationStructureGeometryInstancesDataKHR geometryInstances{};
				geometryInstances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
				geometryInstances.data.deviceAddress = tlasInstancesBuffer->getDeviceAddress();


				asGeometry = {};

				asGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
				asGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
				asGeometry.geometry.instances = geometryInstances;
				asBuildRangeInfo.primitiveCount = static_cast<uint32_t>(tlasInstances.size());

				createAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, m_tlasAccel, asGeometry, asBuildRangeInfo, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
			}

			delete tlasInstancesBuffer;
		}


		AccelStructure getTlas() {
			return m_tlasAccel;
		}



	};

}