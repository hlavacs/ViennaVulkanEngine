/**
* The Vienna Vulkan Engine
*
* (c) bei Alexander Fomin, University of Vienna
*
*/

#include "VEInclude.h"
#include "VKHelpers.h"
#include "RaytracingPipelineGenerator.h"

namespace ve
{
	// using m_enableShadows the shadows can be switched on/off. It uses a push constant to send this data to the closehit shader
	VESubrenderRayTracingNV_DN::VESubrenderRayTracingNV_DN(VERendererRayTracingNV &renderer)
		: VESubrenderRayTracingNV(renderer)
	{
		m_pushConstants.enableReflections = false;
		m_pushConstants.enableShadows = true;
	};

	/**
		* \brief Initialize the subrenderer
		*/
	void VESubrenderRayTracingNV_DN::initSubrenderer()
	{
		VESubrender::initSubrenderer();

		// all scene must be rendered at once, that means, that ressources of all entities must be linked at once

		VkDescriptorSetLayout perObjectLayout = m_renderer.getDescriptorSetLayoutPerObject();
		createRaytracingDescriptorSets();
		VkPushConstantRange pushRange = {};
		pushRange.offset = 0;
		pushRange.size = sizeof(m_pushConstants);
		pushRange.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;

		vh::vhPipeCreateGraphicsPipelineLayout(m_renderer.getDevice(),
			{ perObjectLayout, perObjectLayout, m_descriptorSetLayoutOutput,
			 m_descriptorSetLayoutAS, m_descriptorSetLayoutGeometry,
			 m_descriptorSetLayoutObjectUBOs, m_descriptorSetLayoutResources },
			{ pushRange }, &m_pipelineLayout);

		createRTGraphicsPipeline();
		createShaderBindingTable();

		if (m_maps.empty())
			m_maps.resize(2);
	};

	void VESubrenderRayTracingNV_DN::createRaytracingDescriptorSets()
	{
		if (m_entities.size())
		{
			// memory barrier for vertex and index buffers, can't render before buffers are loaded
			VkBufferMemoryBarrier bmb = {};
			bmb.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bmb.pNext = nullptr;
			bmb.srcAccessMask = 0;
			bmb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bmb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bmb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bmb.offset = 0;
			bmb.size = VK_WHOLE_SIZE;

			VkCommandBufferAllocateInfo commandBufferAllocateInfo;
			commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandBufferAllocateInfo.pNext = nullptr;
			commandBufferAllocateInfo.commandPool = m_renderer.getCommandPool();
			commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAllocateInfo.commandBufferCount = 1;

			VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
			VECHECKRESULT(vkAllocateCommandBuffers(m_renderer.getDevice(), &commandBufferAllocateInfo, &commandBuffer));

			VkCommandBufferBeginInfo beginInfo;
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.pNext = nullptr;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			beginInfo.pInheritanceInfo = nullptr;
			vkBeginCommandBuffer(commandBuffer, &beginInfo);

			for (auto &entity : m_entities)
			{
				bmb.buffer = entity->m_pMesh->m_vertexBuffer;
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &bmb, 0, nullptr);
				bmb.buffer = entity->m_pMesh->m_indexBuffer;
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &bmb, 0, nullptr);
			}
			//submit the command buffers
			VECHECKRESULT(vh::vhCmdEndSingleTimeCommands(m_renderer.getDevice(), m_renderer.getGraphicsQueue(),
				m_renderer.getCommandPool(), commandBuffer,
				VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE));
		}

		// Output image binding (set 2, binding 0)
		vh::vhRenderCreateDescriptorSetLayout(m_renderer.getDevice(),
			{ 1 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
			{ VK_SHADER_STAGE_RAYGEN_BIT_NV },
			&m_descriptorSetLayoutOutput);

		// Acceleration Structure binding (set 3, binding 0)
		vh::vhRenderCreateDescriptorSetLayout(m_renderer.getDevice(),
			{ 1 },
			{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV },
			{ VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV },
			&m_descriptorSetLayoutAS);

		// Vertex and Index binding (set 4, bindings 0 and 1)
		vh::vhRenderCreateDescriptorSetLayout(m_renderer.getDevice(),
			{ m_resourceArrayLength, m_resourceArrayLength },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
			{ VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV },
			&m_descriptorSetLayoutGeometry);

		//Per object UBO binding (set 5)
		vh::vhRenderCreateDescriptorSetLayout(m_renderer.getDevice(),
			{ m_resourceArrayLength },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
			{ VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV },
			&m_descriptorSetLayoutObjectUBOs);

		//Textures binding (set 6)
		vh::vhRenderCreateDescriptorSetLayout(m_renderer.getDevice(), //binding 0...array, binding 1...array
			{ m_resourceArrayLength, m_resourceArrayLength },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
			{ VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV },
			&m_descriptorSetLayoutResources);

		// Acceleration structure and geometry are the same for all swapchains
		vh::vhRenderCreateDescriptorSets(m_renderer.getDevice(), m_renderer.getSwapChainNumber(),
			m_descriptorSetLayoutAS, m_renderer.getDescriptorPool(), m_descriptorSetsAS);
		vh::vhRenderCreateDescriptorSets(m_renderer.getDevice(), m_renderer.getSwapChainNumber(),
			m_descriptorSetLayoutGeometry, m_renderer.getDescriptorPool(),
			m_descriptorSetsGeometry);
		// Output image and UBOPerEntity changes from Swapchain to Swapchain, for each swapchain create own descriptor set
		vh::vhRenderCreateDescriptorSets(m_renderer.getDevice(), m_renderer.getSwapChainNumber(),
			m_descriptorSetLayoutOutput, m_renderer.getDescriptorPool(),
			m_descriptorSetsOutput);
		vh::vhRenderCreateDescriptorSets(m_renderer.getDevice(), m_renderer.getSwapChainNumber(),
			m_descriptorSetLayoutObjectUBOs, m_renderer.getDescriptorPool(),
			m_descriptorSetsUBOs);
	}

	//loads RT shaders
	void VESubrenderRayTracingNV_DN::createRTGraphicsPipeline()
	{
		m_pipelines.resize(1);

		nv_helpers_vk::RayTracingPipelineGenerator pipelineGen;
		// We use only one ray generation, that will implement the camera model
		auto rayGenShaderCode = vh::vhFileRead("media/shader/RayTracing_NV/rgen.spv");
		VkShaderModule rayGenModule = vh::vhPipeCreateShaderModule(m_renderer.getDevice(), rayGenShaderCode);
		m_rayGenIndex = pipelineGen.AddRayGenShaderStage(rayGenModule);

		// The first miss shader is used to look-up the environment in case the rays
		// from the camera miss the geometry
		auto missShaderCode = vh::vhFileRead("media/shader/RayTracing_NV/rmiss.spv");
		VkShaderModule missModule = vh::vhPipeCreateShaderModule(m_renderer.getDevice(), missShaderCode);
		m_missIndex = pipelineGen.AddMissShaderStage(missModule);

		// If we hit an object the shadow_miss shader will be used to define if object must be lighted.
		auto shadowMissShaderCode = vh::vhFileRead("media/shader/RayTracing_NV/shadow_rmiss.spv");
		VkShaderModule shadowMissModule = vh::vhPipeCreateShaderModule(m_renderer.getDevice(), shadowMissShaderCode);
		m_shadowMissIndex = pipelineGen.AddMissShaderStage(shadowMissModule);

		// hit shader for all entiteis in a hit group with index 0
		m_hitGroupIndex = pipelineGen.StartHitGroup();
		auto closestHitShaderCode = vh::vhFileRead("media/shader/RayTracing_NV/rchit.spv");
		VkShaderModule closestHitModule = vh::vhPipeCreateShaderModule(m_renderer.getDevice(), closestHitShaderCode);
		pipelineGen.AddHitShaderStage(closestHitModule, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
		pipelineGen.EndHitGroup();

		// empty shader in a binding table.
		m_shadowHitGroupIndex = pipelineGen.StartHitGroup();
		pipelineGen.EndHitGroup();

		// define a depth of the ray. 2 because after we hit an object, we trace a shadow ray.
		pipelineGen.SetMaxRecursionDepth(2);
		pipelineGen.Generate(m_renderer.getDevice(), m_pipelineLayout, &m_pipelines[0]);

		vkDestroyShaderModule(m_renderer.getDevice(), rayGenModule, nullptr);
		vkDestroyShaderModule(m_renderer.getDevice(), missModule, nullptr);
		vkDestroyShaderModule(m_renderer.getDevice(), shadowMissModule, nullptr);
		vkDestroyShaderModule(m_renderer.getDevice(), closestHitModule, nullptr);
	}

	// Creates a binding table of the shaders. Binding table has one ray generation shader, two miss and two (close) hit shaders.
	void VESubrenderRayTracingNV_DN::createShaderBindingTable()
	{
		m_sbtGen.AddRayGenerationProgram(m_rayGenIndex, {});
		m_sbtGen.AddMissProgram(m_missIndex, {});
		m_sbtGen.AddMissProgram(m_shadowMissIndex, {});
		m_sbtGen.AddHitGroup(m_hitGroupIndex, {});
		m_sbtGen.AddHitGroup(m_shadowHitGroupIndex, {});

		auto props = m_renderer.getPhysicalDeviceRTProperties();
		VkDeviceSize shaderBindingTableSize = m_sbtGen.ComputeSBTSize(props);

		// Allocate mappable memory to store the SBT

		nv_helpers_vk::createBuffer(m_renderer.getPhysicalDevice(), m_renderer.getDevice(), shaderBindingTableSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &m_shaderBindingTableBuffer,
			&m_shaderBindingTableMem, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		m_sbtGen.Generate(m_renderer.getDevice(), m_pipelines[0], m_shaderBindingTableBuffer,
			m_shaderBindingTableMem);
	}

	/**
		* \brief If the window size changes then some resources have to be recreated to fit the new size.
		*/
	void VESubrenderRayTracingNV_DN::recreateResources()
	{
		m_sbtGen.Reset();
		closeSubrenderer();
		initSubrenderer();
		updateRTDescriptorSets();
		uint32_t size = (uint32_t)m_descriptorSetsResources.size();
		if (size > 0)
		{
			m_descriptorSetsResources.clear();
			vh::vhRenderCreateDescriptorSets(m_renderer.getDevice(),
				size, m_descriptorSetLayoutResources,
				m_renderer.getDescriptorPool(),
				m_descriptorSetsResources);

			for (uint32_t i = 0; i < size; i++)
			{
				vh::vhRenderUpdateDescriptorSetMaps(m_renderer.getDevice(),
					m_descriptorSetsResources[i],
					0, i * m_resourceArrayLength, m_resourceArrayLength, m_maps);
			}
		}
	}

	/**
		* \brief Close down the subrenderer and destroy all local resources.
		*/
	void VESubrenderRayTracingNV_DN::closeSubrenderer()
	{
		m_descriptorSetsAS.clear();
		m_descriptorSetsOutput.clear();
		m_descriptorSetsGeometry.clear();
		m_descriptorSetsUBOs.clear();
		for (auto pipeline : m_pipelines)
		{
			vkDestroyPipeline(m_renderer.getDevice(), pipeline, nullptr);
		}

		if (m_pipelineLayout != VK_NULL_HANDLE)
			vkDestroyPipelineLayout(m_renderer.getDevice(), m_pipelineLayout, nullptr);

		if (m_descriptorSetLayoutAS != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(m_renderer.getDevice(), m_descriptorSetLayoutAS, nullptr);
		if (m_descriptorSetLayoutOutput != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(m_renderer.getDevice(), m_descriptorSetLayoutOutput, nullptr);
		if (m_descriptorSetLayoutGeometry != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(m_renderer.getDevice(), m_descriptorSetLayoutGeometry, nullptr);
		if (m_descriptorSetLayoutResources != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(m_renderer.getDevice(), m_descriptorSetLayoutObjectUBOs, nullptr);
		if (m_descriptorSetLayoutResources != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(m_renderer.getDevice(), m_descriptorSetLayoutResources, nullptr);

		vkDestroyBuffer(m_renderer.getDevice(), m_shaderBindingTableBuffer, nullptr);
		vkFreeMemory(m_renderer.getDevice(), m_shaderBindingTableMem, nullptr);
	}

	/**
		* \brief Bind the subrenderer's pipeline to a commandbuffer
		*
		* \param[in] commandBuffer The command buffer to bind the pipeline to
		*
		*/
	void VESubrenderRayTracingNV_DN::bindPipeline(VkCommandBuffer
		commandBuffer)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_pipelines[0]); //bind the PSO
	}

	/**
	* \brief Bind per frame descriptor sets to the pipeline layout
	*
	* \param[in] commandBuffer The command buffer that is used for recording commands
	* \param[in] imageIndex The index of the swapchain image that is currently used
	* \param[in] pCamera Pointer to the current light camera
	* \param[in] pLight Pointer to the currently used light
	* \param[in] descriptorSetsShadow Shadow maps that are used for creating shadow
	*
	*/
	void VESubrenderRayTracingNV_DN::bindDescriptorSetsPerFrame(VkCommandBuffer
		commandBuffer,
		uint32_t imageIndex,
		VECamera *pCamera,
		VELight *pLight)
	{
		//set 0...cam UBO
		//set 1...light resources
		//set 2...output image
		//set 3...acceleration structure
		//set 4...vertex and index
		//set 5...per object UBOs
		//set 6...additional per object resources

		std::vector<VkDescriptorSet> set = {
			pCamera->m_memoryHandle.pMemBlock->descriptorSets[imageIndex],
			pLight->m_memoryHandle.pMemBlock->descriptorSets[imageIndex],
		};

		uint32_t offsets[2] = {
			(uint32_t)(pCamera->m_memoryHandle.entryIndex * sizeof(VECamera::veUBOPerCamera_t)),
			(uint32_t)(pLight->m_memoryHandle.entryIndex * sizeof(VELight::veUBOPerLight_t)),
		};

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_pipelineLayout,
			0, (uint32_t)set.size(), set.data(), 2, offsets);

		std::vector<VkDescriptorSet> set2 = {
			m_descriptorSetsOutput[imageIndex],
			m_descriptorSetsAS[imageIndex],
			m_descriptorSetsGeometry[imageIndex],
			m_descriptorSetsUBOs[imageIndex],
		};
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_pipelineLayout,
			2, (uint32_t)set2.size(), set2.data(), 0, {});
	}

	/**
	*
	* \brief Bind default descriptor sets
	*
	* The function binds the default descriptor sets. Can be overloaded.
	*
	* \param[in] commandBuffer The command buffer to record into all draw calls
	* \param[in] imageIndex Index of the current swap chain image
	* \param[in] entity Pointer to the entity to draw
	*
	*/
	void VESubrenderRayTracingNV_DN::bindDescriptorSetsPerEntity(VkCommandBuffer
		commandBuffer,
		uint32_t imageIndex,
		VEEntity *entity)
	{
		//set 0...cam UBO
		//set 1...light resources
		//set 2...output image
		//set 3...acceleration structure
		//set 4...vertex and index
		//set 5...per object UBO
		//set 6...additional per object resources
		std::vector<VkDescriptorSet> sets = {};

		if (m_descriptorSetsResources.size() > 0 && entity->getResourceIdx() % m_resourceArrayLength == 0)
		{
			sets.push_back(m_descriptorSetsResources[entity->getResourceIdx() / m_resourceArrayLength]);
		}

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_pipelineLayout,
			6, (uint32_t)sets.size(), sets.data(), 0, {});
	}

	// link buffers with a descriptor sets. If any of this ressources are changed, this method must be called.
	void VESubrenderRayTracingNV_DN::updateRTDescriptorSets()
	{
		std::vector<VkWriteDescriptorSet> descriptorWrites;

		std::vector<std::vector<VkDescriptorImageInfo>> imageInfos;
		std::vector<VkDescriptorBufferInfo> infoVertVector;
		std::vector<VkDescriptorBufferInfo> infoIndVector;
		std::vector<std::vector<VkDescriptorBufferInfo>> infoUboVector;

		VkWriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo;
		descriptorAccelerationStructureInfo.sType =
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
		descriptorAccelerationStructureInfo.pNext = nullptr;
		descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
		descriptorAccelerationStructureInfo.pAccelerationStructures = m_renderer.getTLASHandlePointer();

		// go through all entities and for each vertex and index buffer add a BufferInfo.
		for (auto &entity : m_entities)
		{
			VmaAllocationInfo info;
			vmaGetAllocationInfo(m_renderer.getVmaAllocator(), entity->m_pMesh->m_vertexBufferAllocation, &info);
			VkDescriptorBufferInfo dbiVert = {};
			dbiVert.buffer = entity->m_pMesh->m_vertexBuffer;
			dbiVert.range = VK_WHOLE_SIZE;
			dbiVert.offset = 0;
			infoVertVector.push_back(dbiVert);

			vmaGetAllocationInfo(m_renderer.getVmaAllocator(), entity->m_pMesh->m_indexBufferAllocation, &info);
			VkDescriptorBufferInfo dbiInd = {};
			dbiInd.buffer = entity->m_pMesh->m_indexBuffer;
			dbiInd.range = VK_WHOLE_SIZE;
			dbiInd.offset = 0;
			infoIndVector.push_back(dbiInd);
		}

		// all swapchains have the same acceleration structure
		for (size_t i = 0; i < m_descriptorSetsAS.size(); i++)
		{
			VkWriteDescriptorSet descriptorWrite = {};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = m_descriptorSetsAS[i];
			descriptorWrite.dstBinding = 0;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pNext = &descriptorAccelerationStructureInfo;

			descriptorWrites.push_back(descriptorWrite);
		}

		// we link an output image as a VK_DESCRIPTOR_TYPE_STORAGE_IMAGE and write the data using
		// ImageStore shader function
		for (size_t i = 0; i < m_descriptorSetsOutput.size(); i++)
		{
			VkDescriptorImageInfo descriptorOutputImageInfo = {};
			descriptorOutputImageInfo.sampler = VK_NULL_HANDLE;
			descriptorOutputImageInfo.imageView = m_renderer.m_swapChainImageViews[i];
			descriptorOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

			imageInfos.push_back({ descriptorOutputImageInfo });
			VkWriteDescriptorSet descriptorWrite = {};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = m_descriptorSetsOutput[i];
			descriptorWrite.dstBinding = 0;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			descriptorWrite.descriptorCount = static_cast<uint32_t>(imageInfos.back().size());
			descriptorWrite.pImageInfo = imageInfos.back().data();

			descriptorWrites.push_back(descriptorWrite);
		}

		if (m_entities.size())
		{
			// link a descriptor set with index and vertex array of buffers
			for (size_t i = 0; i < m_descriptorSetsGeometry.size(); i++)
			{
				VkWriteDescriptorSet descriptorWriteVert = {};
				descriptorWriteVert.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWriteVert.dstSet = m_descriptorSetsGeometry[i];
				descriptorWriteVert.dstBinding = 0;
				descriptorWriteVert.dstArrayElement = 0;
				descriptorWriteVert.descriptorCount = static_cast<uint32_t>(infoVertVector.size());
				descriptorWriteVert.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				descriptorWriteVert.pBufferInfo = infoVertVector.data();
				descriptorWrites.push_back(descriptorWriteVert);

				VkWriteDescriptorSet descriptorWriteInd = {};
				descriptorWriteInd.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWriteInd.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				descriptorWriteInd.dstSet = m_descriptorSetsGeometry[i];
				descriptorWriteInd.dstBinding = 1;
				descriptorWriteInd.dstArrayElement = 0;
				descriptorWriteInd.descriptorCount = static_cast<uint32_t>(infoIndVector.size());
				descriptorWriteInd.pBufferInfo = infoIndVector.data();
				descriptorWriteInd.pImageInfo = VK_NULL_HANDLE;
				descriptorWriteInd.pTexelBufferView = VK_NULL_HANDLE;
				descriptorWriteInd.pNext = VK_NULL_HANDLE;
				descriptorWrites.push_back(descriptorWriteInd);
			}
			// for each swapchain link a descriptor set with a array of buffers with UBOPerEntity
			for (size_t i = 0; i < m_descriptorSetsUBOs.size(); i++)
			{
				std::vector<VkDescriptorBufferInfo> infoUboVectorLocal;
				for (auto &entity : m_entities)
				{
					VkDescriptorBufferInfo dbiUBO = {};
					dbiUBO.buffer = entity->m_memoryHandle.pMemBlock->buffers[i];
					dbiUBO.range = sizeof(VEEntity::veUBOPerEntity_t);
					dbiUBO.offset = entity->m_memoryHandle.entryIndex * sizeof(VEEntity::veUBOPerEntity_t);
					infoUboVectorLocal.push_back(dbiUBO);
				}
				infoUboVector.push_back({ infoUboVectorLocal });
				VkWriteDescriptorSet descriptorWriteUBO = {};
				descriptorWriteUBO.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				descriptorWriteUBO.descriptorCount = static_cast<uint32_t>(infoUboVector.back().size());
				descriptorWriteUBO.dstBinding = 0;
				descriptorWriteUBO.dstArrayElement = 0;
				descriptorWriteUBO.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWriteUBO.dstSet = m_descriptorSetsUBOs[i];
				descriptorWriteUBO.pBufferInfo = infoUboVector.back().data();
				descriptorWrites.push_back(descriptorWriteUBO);
			}
			vkUpdateDescriptorSets(m_renderer.getDevice(), static_cast<uint32_t>(descriptorWrites.size()),
				descriptorWrites.data(), 0, nullptr);
		}
	}

	/**
	* \brief Draw all associated entities.
	*
	* The subrenderer maintains a list of all associated entities. In this function it goes through all of them
	* and draws them. A vector is used in order to be able to parallelize this in case thousands or objects are in the list
	*
	* \param[in] commandBuffer The command buffer to record into all draw calls
	* \param[in] imageIndex Index of the current swap chain image
	* \param[in] numPass The number of the light that has been rendered
	* \param[in] pCamera Pointer to the current light camera
	* \param[in] pLight Pointer to the current light
	* \param[in] descriptorSetsShadow The shadow maps to be used.
	*
	*/
	void VESubrenderRayTracingNV_DN::draw(VkCommandBuffer
		commandBuffer,
		uint32_t imageIndex,
		uint32_t
		numPass,
		VECamera *pCamera,
		VELight *pLight,
		std::vector<VkDescriptorSet> descriptorSetsShadow)
	{
		if (m_entities.

			size()

			== 0)
			return;

		vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV,
			0, sizeof(m_pushConstants), &m_pushConstants);

		bindPipeline(commandBuffer);
		bindDescriptorSetsPerFrame(commandBuffer, imageIndex, pCamera, pLight);
		bindDescriptorSetsPerEntity(commandBuffer, imageIndex, m_entities[0]); //bind the entity's descriptor sets
		drawEntity(commandBuffer, imageIndex);
	}

	/**
	* \brief Set the danymic pipeline stat, i.e. the blend constants to be used
	*
	* \param[in] commandBuffer The currently used command buffer
	* \param[in] numPass The current pass number - in the forst pass, write over pixel colors, after this add pixel colors
	*
	*/
	void VESubrenderRayTracingNV_DN::setDynamicPipelineState(VkCommandBuffer
		commandBuffer,
		uint32_t numPass)
	{
		if (numPass == 0)
		{
			float blendConstants[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			vkCmdSetBlendConstants(commandBuffer, blendConstants);
			return;
		}

		float blendConstants[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		vkCmdSetBlendConstants(commandBuffer, blendConstants);
	}

	/**
	* \brief Remember the last recorded entity
	*
	* Needed for incremental recording.
	*
	*/
	void VESubrenderRayTracingNV_DN::afterDrawFinished()
	{
		m_idxLastRecorded = (uint32_t)
			m_entities.size() -
			1;
	}

	/**
	*
	* \brief Draw one entity
	*
	* The function binds the vertex buffer, index buffer, and descriptor set of the entity, then commits a draw call
	*
	* \param[in] commandBuffer The command buffer to record into all draw calls
	* \param[in] imageIndex Index of the current swap chain image
	* \param[in] entity Pointer to the entity to draw
	*
	*/
	void VESubrenderRayTracingNV_DN::drawEntity(VkCommandBuffer
		commandBuffer,
		uint32_t imageIndex)
	{
		VkDeviceSize rayGenOffset = m_sbtGen.GetRayGenOffset();
		VkDeviceSize missOffset = m_sbtGen.GetMissOffset();
		VkDeviceSize missStride = m_sbtGen.GetMissEntrySize();
		VkDeviceSize hitGroupOffset = m_sbtGen.GetHitGroupOffset();
		VkDeviceSize hitGroupStride = m_sbtGen.GetHitGroupEntrySize();
		auto swapChainExtent = m_renderer.getSwapChainExtent();
		vkCmdTraceRaysNV(commandBuffer,
			m_shaderBindingTableBuffer, rayGenOffset,
			m_shaderBindingTableBuffer, missOffset, missStride,
			m_shaderBindingTableBuffer, hitGroupOffset, hitGroupStride,
			VK_NULL_HANDLE,
			0, 0,
			swapChainExtent.width, swapChainExtent.height, 1);
	}

	/**
	* \brief Add an entity to the subrenderer
	*
	* Create a UBO for this entity, a descriptor set per swapchain image, and update the descriptor sets
	*
	*/
	void VESubrenderRayTracingNV_DN::addEntity(VEEntity *pEntity)
	{
		std::vector<VkDescriptorImageInfo> maps = {};
		
		maps.push_back(pEntity->m_pMaterial->mapDiffuse->m_imageInfo);
		// this renderer is responsible for entities with and without normal map.
		// here we need a placeholder to align all normal maps. That's why we use a diffuse map info. 
		// shader will decide, wether it has a normal map by using a dedicated flag
		maps.push_back(pEntity->m_pMaterial->mapNormal ? pEntity->m_pMaterial->mapNormal->m_imageInfo: pEntity->m_pMaterial->mapDiffuse->m_imageInfo);
		
		addMaps(pEntity, maps);

		VESubrender::addEntity(pEntity);
	}

} // namespace ve
