/**
 * @file object_manager.cpp
 * @brief ObjectManager implementation.
 */

#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

    ObjectManager::ObjectManager(std::string systemName, Engine& engine, VkDevice& device, VkPhysicalDevice& physicalDevice, CommandManager* commandManager, VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProperties) :
		System{ systemName, engine }, device(device), physicalDevice(physicalDevice), commandManager(commandManager), m_asProperties(m_asProperties) {
        loadRayTracingFunctions();
		createVertexBuffer();
		createIndexBuffer();
		createInstanceBuffers();

		std::cout << "MAX frames in flight = " << MAX_FRAMES_IN_FLIGHT << "\n";
		instanceChanged = std::vector<bool>(MAX_FRAMES_IN_FLIGHT, false);
		currentFrame = 0;

		engine.RegisterCallbacks({
			{this,  1999, "OBJECT_CREATE", [this](Message& message) { return OnMeshCreated(message); } },
			{this,  10000, "OBJECT_CHANGED", [this](Message& message) { return OnObjectChanged(message); } },
			{this,  10000, "MESH_CREATE", [this](Message& message) { return OnMeshCreated(message); } },
			{this,  1999, "RECORD_NEXT_FRAME", [this](Message& message) { return OnRecordNextFrame(message); } }
			});
    }
	ObjectManager::~ObjectManager() {
	}

	void ObjectManager::freeResources() {	
		freeBlas();
		freeTlas();

		for (HostBuffer<vvh::Instance>* instanceBuffer : instanceBuffers) {
			delete instanceBuffer;
		}
		delete vertexBuffer;
		delete indexBuffer;
	}

	void ObjectManager::freeBlas() {
		for (AccelStructure& accel : m_blasAccel) {
			vkDestroyAccelerationStructureKHR(device, accel.accel, nullptr);
			delete accel.buffer;
		}
	}

	void ObjectManager::freeTlas() {
		if (m_tlasAccel.accel != VK_NULL_HANDLE) {
			vkDestroyAccelerationStructureKHR(device, m_tlasAccel.accel, nullptr);
			delete m_tlasAccel.buffer;
		}
	}

	bool ObjectManager::OnMeshCreated(Message message) {
		objectCreated = true;
		return false;
	}
	bool ObjectManager::OnObjectChanged(Message message) {
		//Set instance changed to true for all frames in flight since all instances have to be updated
		const auto& msg = message.template GetData<MsgObjectChanged>();
		const auto& oHandle = msg.m_object();
		if (m_registry.Has<MeshHandle>(oHandle)) {
			for (int i = 0; i < instanceChanged.size(); i++) {
				instanceChanged[i] = true;
			}
		}
		return false;
	}
	bool ObjectManager::OnObjectCreated(Message message) {
		for (int i = 0; i < instanceChanged.size(); i++) {
			instanceChanged[i] = true;
		}
		return false;
	}

	void ObjectManager::prepareNextFrame() {

		if (objectCreated) {
			std::vector<Vertex> accumulatedVertexData;
			std::vector<uint32_t> accumulatedIndices;
			size_t firstVertex = 0;
			size_t firstIndex = 0;
			numMeshes = 0;

			for (auto [gHandle, mesh] : m_registry.GetView<vecs::Handle, vvh::Mesh&>()) {
				vvh::VertexData& vertexData = mesh().m_verticesData;
				std::cout << "tangents: " << vertexData.m_tangents.size() << "\n";
				firstVertex = accumulatedVertexData.size();
				firstIndex = accumulatedIndices.size();

				for (int i = 0; i < vertexData.m_positions.size(); i++) {
					Vertex vertex = Vertex();
					vertex.pos = glm::vec4(vertexData.m_positions[i], 1.0);
					vertex.normal = glm::vec4(vertexData.m_normals[i], 0.0);
					vertex.texCoord = glm::vec4(vertexData.m_texCoords[i], 0.0, 0.0);
					vertex.tangent = glm::vec4(vertexData.m_tangents[i], 0.0);

					accumulatedVertexData.push_back(vertex);
				}

				for (uint32_t index: mesh().m_indices) {
					accumulatedIndices.push_back(index + firstVertex);
				}

				mesh().firstIndex = firstIndex;
				mesh().indexCount = mesh().m_indices.size();
				mesh().firstVertex = firstVertex;
				mesh().vertexCount = vertexData.m_positions.size();
				mesh().index = numMeshes;
				
				numMeshes++;
			}
			vertexBuffer->updateBuffer(accumulatedVertexData.data(), accumulatedVertexData.size());
			indexBuffer->updateBuffer(accumulatedIndices.data(), accumulatedIndices.size());
			
			createBottomLevelAS();
		}

		if (instanceChanged[currentFrame]) {
			for (auto [gHandle, mesh] : m_registry.GetView<vecs::Handle, vvh::Mesh&>()) {
				mesh().instances = std::vector<vvh::Instance>(); //reset Instance list;
			}
			
			for (auto [oHandle, name, ghandle, mhandle, LtoW] : m_registry.GetView<vecs::Handle, Name, MeshHandle, MaterialHandle, LocalToWorldMatrix&>()) {

				vvh::Mesh& mesh = m_registry.Get<vvh::Mesh&>(ghandle);
				vvh::VRTMaterial& material = m_registry.Get<vvh::VRTMaterial&>(mhandle);

				vvh::Instance instance;

				instance.firstIndex = mesh.firstIndex;
				instance.materialIndex = material.index;
				instance.model = LtoW;
				instance.modelInverse = glm::inverse(instance.model);

				if (m_registry.Has<UVScale>(oHandle)) {
					UVScale uvScale = m_registry.Get<UVScale>(oHandle);
					instance.uvScale = uvScale;
				}
				else {
					instance.uvScale = glm::vec2(1.0f);
				}

				instance.uvScale *= material.uvScale;

				mesh.instances.push_back(instance);
			}

			std::vector<vvh::Instance> accumulatedInstances;

			for (auto [gHandle, mesh] : m_registry.GetView<vecs::Handle, vvh::Mesh&>()) {
				mesh().firstInstance = accumulatedInstances.size();
				mesh().instanceCount = mesh().instances.size();
				accumulatedInstances.insert(accumulatedInstances.end(), mesh().instances.begin(), mesh().instances.end());
			}
			
			instanceBuffers[currentFrame]->updateBuffer(accumulatedInstances.data(), accumulatedInstances.size());
			freeTlas();
			createTopLevelAS();
		}
	}

	bool ObjectManager::OnRecordNextFrame(Message message) {
		objectCreated = false;
		instanceChanged[currentFrame] = false;
		return false;
	}

    void ObjectManager::createVertexBuffer() {
        vertexBuffer = new DeviceBuffer<Vertex>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, commandManager, device, physicalDevice);
    }


    void ObjectManager::createIndexBuffer() {
        indexBuffer = new DeviceBuffer<uint32_t>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, commandManager, device, physicalDevice);
    }


    void ObjectManager::createInstanceBuffers() {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            HostBuffer<vvh::Instance>* instanceBuffer = new HostBuffer<vvh::Instance>(maxInstances, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0, device, physicalDevice);
            instanceBuffers.push_back(instanceBuffer);
        }
    }

    DeviceBuffer<Vertex>* ObjectManager::getVertexBuffer() {
        return vertexBuffer;
    }

    DeviceBuffer<uint32_t>* ObjectManager::getIndexBuffer() {
        return indexBuffer;
    }

    std::vector<HostBuffer<vvh::Instance>*> ObjectManager::getInstanceBuffers() {
        return instanceBuffers;
    }


	void ObjectManager::loadRayTracingFunctions()
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

	bool ObjectManager::createAcceleration(
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


	void ObjectManager::createAccelerationStructure(VkAccelerationStructureTypeKHR asType,  // The type of acceleration structure (BLAS or TLAS)
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

	void ObjectManager::convertToAcelGeometry(vvh::Mesh& mesh, VkAccelerationStructureGeometryKHR& geometry, VkAccelerationStructureBuildRangeInfoKHR& rangeInfo) {
		uint32_t triangleCount = mesh.indexCount / 3;

		// Triangles data
		VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
		triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		//change format once propper vertex data is used
		triangles.vertexFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
		triangles.vertexStride = sizeof(Vertex);
		triangles.maxVertex = mesh.firstVertex + mesh.vertexCount;

		// GLOBAL vertex buffer � no per-object offset needed
		triangles.vertexData.deviceAddress = vertexBuffer->getDeviceAddress();

		// GLOBAL index buffer � we point into the correct range using firstIndex
		triangles.indexType = VK_INDEX_TYPE_UINT32;
		triangles.indexData.deviceAddress =
			indexBuffer->getDeviceAddress() + mesh.firstIndex * sizeof(uint32_t);

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

	void ObjectManager::createBottomLevelAS()
	{
		//free previous accel
		freeBlas();
		// Prepare geometry information for all meshes
		m_blasAccel = std::vector<AccelStructure>();
		m_blasAccel.resize(numMeshes);

		for (auto [gHandle, mesh] : m_registry.GetView<vecs::Handle, vvh::Mesh&>()) {
			VkAccelerationStructureGeometryKHR       asGeometry{};
			VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};

			convertToAcelGeometry(mesh, asGeometry, asBuildRangeInfo);

			createAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, m_blasAccel[mesh().index], asGeometry,
				asBuildRangeInfo, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
		}
	}

	void ObjectManager::createTopLevelAS()
	{

		auto toTransformMatrixKHR = [](const glm::mat4& m) {
			VkTransformMatrixKHR t{};
			for (int row = 0; row < 3; ++row)
				for (int col = 0; col < 4; ++col)
					t.matrix[row][col] = m[col][row];
			return t;
		};

		// Prepare instance data for TLAS
		std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;

		//this has to match the order of the instance buffer otherwise things will not work!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		for (auto [gHandle, mesh] : m_registry.GetView<vecs::Handle, vvh::Mesh&>()) {
			for (vvh::Instance& instance : mesh().instances)
			{
				VkAccelerationStructureInstanceKHR asInstance{};
				asInstance.transform = toTransformMatrixKHR(instance.model);  // Position of the instance
				asInstance.accelerationStructureReference = m_blasAccel[mesh().index].buffer->getDeviceAddress();  
				asInstance.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
				asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;  // No culling - double sided
				asInstance.mask = 0xFF;
				asInstance.instanceCustomIndex = 0;
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


	AccelStructure ObjectManager::getTlas() {
		return m_tlasAccel;
	}

	void ObjectManager::updateCurrentFrame(int currentFrame) {
		this->currentFrame = currentFrame;
	}

	bool ObjectManager::instancesChanged() {
		return instanceChanged[currentFrame];
	}

	bool ObjectManager::meshesChanged() {
		return objectCreated;
	}




}
