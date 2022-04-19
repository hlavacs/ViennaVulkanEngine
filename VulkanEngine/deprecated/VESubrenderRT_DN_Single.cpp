/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"
#include "RaytracingPipelineGenerator.h"
#include "VKHelpers.h"


namespace ve {

    /**
    * \brief Initialize the subrenderer
    */
    void VESubrenderRT_DN::initSubrenderer() {
        VESubrender::initSubrenderer();

        VkDescriptorSetLayout perObjectLayout = getRendererRTPointer()->getDescriptorSetLayoutPerObject();

        createRaytracingDescriptorSets();

        vh::vhPipeCreateGraphicsPipelineLayout(getRendererRTPointer()->getDevice(),
                                               {perObjectLayout, perObjectLayout, m_descriptorSetLayoutOutput,
                                                m_descriptorSetLayoutAS, m_descriptorSetLayoutGeometry, perObjectLayout,
                                                m_descriptorSetLayoutResources},
                                               {}, &m_pipelineLayout);

        createRTGraphicsPipeline();
        createShaderBindingTable();

        if (m_maps.empty()) m_maps.resize(2);
    };

    void VESubrenderRT_DN::createRaytracingDescriptorSets() {
        if (m_entities.size()) {
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
            commandBufferAllocateInfo.commandPool = getRendererRTPointer()->getCommandPool();
            commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            commandBufferAllocateInfo.commandBufferCount = 1;

            VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
            VkResult code = vkAllocateCommandBuffers(getRendererRTPointer()->getDevice(), &commandBufferAllocateInfo,
                                                     &commandBuffer);
            if (code != VK_SUCCESS) {
                throw std::logic_error("rt descriptor sets vkAllocateCommandBuffers failed");
            }

            VkCommandBufferBeginInfo beginInfo;
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.pNext = nullptr;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            beginInfo.pInheritanceInfo = nullptr;
            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            bmb.buffer = m_entities[0]->m_pMesh->m_vertexBuffer;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &bmb, 0, nullptr);
            bmb.buffer = m_entities[0]->m_pMesh->m_indexBuffer;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &bmb, 0, nullptr);

            //submit the command buffers
            VkResult result = vh::vhCmdEndSingleTimeCommands(getRendererRTPointer()->getDevice(),
                                                             getRendererRTPointer()->getGraphicsQueue(),
                                                             getRendererRTPointer()->getCommandPool(), commandBuffer,
                                                             VK_NULL_HANDLE,
                                                             VK_NULL_HANDLE,
                                                             VK_NULL_HANDLE
            );
            assert(result == VK_SUCCESS);
        }

        // Output image binding (set 2, binding 0)
        vh::vhRenderCreateDescriptorSetLayout(getRendererRTPointer()->getDevice(),
                                              {1},
                                              {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
                                              {VK_SHADER_STAGE_RAYGEN_BIT_NV},

                                              &m_descriptorSetLayoutOutput);

        // Acceleration Structure binding (set 3, binding 0)
        vh::vhRenderCreateDescriptorSetLayout(getRendererRTPointer()->getDevice(),
                                              {1},
                                              {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV},
                                              {VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV},
                                              &m_descriptorSetLayoutAS);

        // Vertex and Index binding (set 4, bindings 0 and 1)
        vh::vhRenderCreateDescriptorSetLayout(getRendererRTPointer()->getDevice(),
                                              {1, 1},
                                              {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
                                              {VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV},
                                              &m_descriptorSetLayoutGeometry);

        //Per object UBO binding (set 5)

        //Textures binding (set 6)
        vh::vhRenderCreateDescriptorSetLayout(
                getRendererRTPointer()->getDevice(),    //binding 0...array, binding 1...array
                {m_resourceArrayLength, m_resourceArrayLength},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
                {VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV},
                &m_descriptorSetLayoutResources);

        vh::vhRenderCreateDescriptorSets(getRendererRTPointer()->getDevice(),
                                         getRendererRTPointer()->getSwapChainNumber(), m_descriptorSetLayoutAS,
                                         getRendererRTPointer()->getDescriptorPool(), m_descriptorSetsAS);
        vh::vhRenderCreateDescriptorSets(getRendererRTPointer()->getDevice(),
                                         getRendererRTPointer()->getSwapChainNumber(), m_descriptorSetLayoutGeometry,
                                         getRendererRTPointer()->getDescriptorPool(), m_descriptorSetsGeometry);
        vh::vhRenderCreateDescriptorSets(getRendererRTPointer()->getDevice(),
                                         getRendererRTPointer()->getSwapChainNumber(), m_descriptorSetLayoutOutput,
                                         getRendererRTPointer()->getDescriptorPool(), m_descriptorSetsOutput);
    }

    void VESubrenderRT_DN::createRTGraphicsPipeline() {
        m_pipelines.resize(1);

        nv_helpers_vk::RayTracingPipelineGenerator pipelineGen;
        // We use only one ray generation, that will implement the camera model
        auto rayGenShaderCode = vh::vhFileRead("media/shader/RT/rgen.spv");
        VkShaderModule rayGenModule = vh::vhPipeCreateShaderModule(getRendererRTPointer()->getDevice(),
                                                                   rayGenShaderCode);
        m_rayGenIndex = pipelineGen.AddRayGenShaderStage(rayGenModule);
        // The first miss shader is used to look-up the environment in case the rays
        // from the camera miss the geometry

        auto missShaderCode = vh::vhFileRead("media/shader/RT/rmiss.spv");
        VkShaderModule missModule = vh::vhPipeCreateShaderModule(getRendererRTPointer()->getDevice(), missShaderCode);
        m_missIndex = pipelineGen.AddMissShaderStage(missModule);

        m_hitGroupIndex = pipelineGen.StartHitGroup();
        auto closestHitShaderCode = vh::vhFileRead("media/shader/RT/rchit.spv");
        VkShaderModule closestHitModule = vh::vhPipeCreateShaderModule(getRendererRTPointer()->getDevice(),
                                                                       closestHitShaderCode);
        pipelineGen.AddHitShaderStage(closestHitModule, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
        pipelineGen.EndHitGroup();

        pipelineGen.SetMaxRecursionDepth(1);
        pipelineGen.Generate(getRendererRTPointer()->getDevice(), m_pipelineLayout, &m_pipelines[0]);

        vkDestroyShaderModule(getRendererRTPointer()->getDevice(), rayGenModule, nullptr);
        vkDestroyShaderModule(getRendererRTPointer()->getDevice(), missModule, nullptr);
        vkDestroyShaderModule(getRendererRTPointer()->getDevice(), closestHitModule, nullptr);
    }

    void VESubrenderRT_DN::createShaderBindingTable() {

        m_sbtGen.AddRayGenerationProgram(m_rayGenIndex, {});
        m_sbtGen.AddMissProgram(m_missIndex, {});
        m_sbtGen.AddHitGroup(m_hitGroupIndex, {});

        auto props = getRendererRTPointer()->getPhysicalDeviceRTProperties();
        VkDeviceSize shaderBindingTableSize = m_sbtGen.ComputeSBTSize(props);

        // Allocate mappable memory to store the SBT

        nv_helpers_vk::createBuffer(getRendererRTPointer()->getPhysicalDevice(), getRendererRTPointer()->getDevice(),
                                    shaderBindingTableSize,
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &m_shaderBindingTableBuffer,
                                    &m_shaderBindingTableMem, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        m_sbtGen.Generate(getRendererRTPointer()->getDevice(), m_pipelines[0], m_shaderBindingTableBuffer,
                          m_shaderBindingTableMem);
    }

    /**
    * \brief If the window size changes then some resources have to be recreated to fit the new size.
    */
    void VESubrenderRT_DN::recreateResources() {
        closeSubrenderer();
        initSubrenderer();

        uint32_t
                size = (uint32_t)
                m_descriptorSetsResources.size();
        if (size > 0) {
            m_descriptorSetsResources.clear();
            vh::vhRenderCreateDescriptorSets(getRendererRTPointer()->getDevice(),
                                             size, m_descriptorSetLayoutResources,
                                             getRendererRTPointer()->getDescriptorPool(),
                                             m_descriptorSetsResources);

            for (uint32_t i = 0; i < size; i++) {
                vh::vhRenderUpdateDescriptorSetMaps(getRendererRTPointer()->getDevice(),
                                                    m_descriptorSetsResources[i],
                                                    0, i * m_resourceArrayLength, m_resourceArrayLength, m_maps);
            }
        }
    }

    /**
    * \brief Close down the subrenderer and destroy all local resources.
    */
    void VESubrenderRT_DN::closeSubrenderer() {

        for (auto pipeline : m_pipelines) {
            vkDestroyPipeline(getRendererRTPointer()->getDevice(), pipeline, nullptr);
        }

        if (m_pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(getRendererRTPointer()->getDevice(), m_pipelineLayout, nullptr);

        if (m_descriptorSetLayoutAS != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(getRendererRTPointer()->getDevice(), m_descriptorSetLayoutAS, nullptr);
        if (m_descriptorSetLayoutOutput != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(getRendererRTPointer()->getDevice(), m_descriptorSetLayoutOutput, nullptr);
        if (m_descriptorSetLayoutGeometry != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(getRendererRTPointer()->getDevice(), m_descriptorSetLayoutGeometry, nullptr);
        if (m_descriptorSetLayoutResources != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(getRendererRTPointer()->getDevice(), m_descriptorSetLayoutResources, nullptr);

        vkDestroyBuffer(getRendererRTPointer()->getDevice(), m_shaderBindingTableBuffer, nullptr);
        vkFreeMemory(getRendererRTPointer()->getDevice(), m_shaderBindingTableMem, nullptr);
    }

    /**
    * \brief Bind the subrenderer's pipeline to a commandbuffer
    *
    * \param[in] commandBuffer The command buffer to bind the pipeline to
    *
    */
    void VESubrenderRT_DN::bindPipeline(VkCommandBuffer
    commandBuffer) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_pipelines[0]
    );    //bind the PSO
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
void VESubrenderRT_DN::bindDescriptorSetsPerFrame(VkCommandBuffer
commandBuffer,
uint32_t imageIndex,
        VECamera
* pCamera,
VELight *pLight
) {

//set 0...cam UBO
//set 1...light resources
//set 2...output image
//set 3...acceleration structure
//set 4...vertex and index
//set 5...per object UBO
//set 6...additional per object resources

std::vector<VkDescriptorSet> set = {
        pCamera->m_memoryHandle.pMemBlock->descriptorSets[imageIndex],
        pLight->m_memoryHandle.pMemBlock->descriptorSets[imageIndex],
};

uint32_t offsets[2] = {
        pCamera->m_memoryHandle.entryIndex * sizeof(VECamera::veUBOPerCamera_t),
        pLight->m_memoryHandle.entryIndex * sizeof(VELight::veUBOPerLight_t),
};

vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_pipelineLayout,
0, (uint32_t)set.

size(), set

.

data(),

2, offsets);

std::vector<VkDescriptorSet> set2 = {
        m_descriptorSetsOutput[imageIndex],
        m_descriptorSetsAS[imageIndex],
        m_descriptorSetsGeometry[imageIndex],
};
vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_pipelineLayout,
2, (uint32_t)set2.

size(), set2

.

data(),

0, {
});
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
void VESubrenderRT_DN::bindDescriptorSetsPerEntity(VkCommandBuffer
commandBuffer,
uint32_t imageIndex, VEEntity
* entity) {

//set 0...cam UBO
//set 1...light resources
//set 2...output image
//set 3...acceleration structure
//set 4...vertex and index
//set 5...per object UBO
//set 6...additional per object resources


std::vector<VkDescriptorSet> sets = {};

sets.
push_back(entity
->m_memoryHandle.pMemBlock->descriptorSets[imageIndex]);
if (m_descriptorSetsResources.

size()

> 0 && entity->

getResourceIdx()

% m_resourceArrayLength == 0) {
sets.
push_back(m_descriptorSetsResources[entity->getResourceIdx() / m_resourceArrayLength]);
}

uint32_t offset = entity->m_memoryHandle.entryIndex * sizeof(VEEntity::veUBOPerEntity_t);
vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_pipelineLayout,
5, (uint32_t)sets.

size(), sets

.

data(),

1, &offset);

}

// TODO: think of better place to update Acceleration structures and vertex/index descriptor sets
void VESubrenderRT_DN::UpdateRTDescriptorSets() {

    for (size_t i = 0; i < m_descriptorSetsAS.size(); i++) {
        std::vector<VkWriteDescriptorSet> descriptorWrites = {};

        VkWriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo;
        descriptorAccelerationStructureInfo.sType =
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
        descriptorAccelerationStructureInfo.pNext = nullptr;
        descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
        descriptorAccelerationStructureInfo.pAccelerationStructures = &(getRendererRTPointer()->getTLAS()->structure);

        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptorSetsAS[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = VK_NULL_HANDLE;
        descriptorWrite.pImageInfo = VK_NULL_HANDLE;
        descriptorWrite.pTexelBufferView = VK_NULL_HANDLE;
        descriptorWrite.pNext = &descriptorAccelerationStructureInfo;

        descriptorWrites.push_back(descriptorWrite);
        vkUpdateDescriptorSets(getRendererRTPointer()->getDevice(), static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }

    for (size_t i = 0; i < m_descriptorSetsOutput.size(); i++) {
        std::vector<VkWriteDescriptorSet> descriptorWrites = {};

        VkDescriptorImageInfo descriptorOutputImageInfo;
        descriptorOutputImageInfo.sampler = nullptr;
        descriptorOutputImageInfo.imageView = getRendererRTPointer()->m_swapChainImageViews[i];
        descriptorOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::vector<VkDescriptorImageInfo> info = {descriptorOutputImageInfo};

        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptorSetsOutput[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorWrite.descriptorCount = static_cast<uint32_t>(info.size());
        descriptorWrite.pBufferInfo = VK_NULL_HANDLE;
        descriptorWrite.pImageInfo = info.data();
        descriptorWrite.pTexelBufferView = VK_NULL_HANDLE;
        descriptorWrite.pNext = VK_NULL_HANDLE;

        descriptorWrites.push_back(descriptorWrite);
        vkUpdateDescriptorSets(getRendererRTPointer()->getDevice(), static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }
    if (m_entities.size()) {
        for (size_t i = 0; i < m_descriptorSetsGeometry.size(); i++) {
            vh::vhRenderUpdateDescriptorSet(getRendererRTPointer()->getDevice(), m_descriptorSetsGeometry[i],
                                            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
                                            {m_entities[0]->m_pMesh->m_vertexBuffer,
                                             m_entities[0]->m_pMesh->m_indexBuffer},
                                            {VK_WHOLE_SIZE, VK_WHOLE_SIZE},
                                            {},
                                            {}
            );
        }
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
void VESubrenderRT_DN::draw(VkCommandBuffer
commandBuffer,
uint32_t imageIndex,
        uint32_t
numPass,
VECamera *pCamera, VELight
* pLight) {

if (m_entities.

size()

== 0) return;
m_idxLastRecorded = (uint32_t)
                            m_entities.

                                    size()

                    - 1;

if (numPass > 0 &&

getClass()

!= VE_SUBRENDERER_CLASS_OBJECT) return;

//UpdateRTDescriptorSets();

bindPipeline(commandBuffer);

setDynamicPipelineState(commandBuffer, numPass
);

bindDescriptorSetsPerFrame(commandBuffer, imageIndex, pCamera, pLight
);

bindDescriptorSetsPerEntity(commandBuffer, imageIndex, m_entities[0]
);    //bind the entity's descriptor sets
drawEntity(commandBuffer, imageIndex, m_entities[0]
);
}

/**
* \brief Set the danymic pipeline stat, i.e. the blend constants to be used
*
* \param[in] commandBuffer The currently used command buffer
* \param[in] numPass The current pass number - in the forst pass, write over pixel colors, after this add pixel colors
*
*/
void VESubrenderRT_DN::setDynamicPipelineState(VkCommandBuffer
commandBuffer,
uint32_t numPass
)
{
if(numPass == 0)
{
float blendConstants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
vkCmdSetBlendConstants(commandBuffer, blendConstants
);
return;
}

float blendConstants[4] = {1.0f, 1.0f, 1.0f, 1.0f};
vkCmdSetBlendConstants(commandBuffer, blendConstants
);
}

/**
* \brief Remember the last recorded entity
*
* Needed for incremental recording.
*
*/
void VESubrenderRT_DN::afterDrawFinished() {
    m_idxLastRecorded = (uint32_t)
                                m_entities.size() - 1;
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
void VESubrenderRT_DN::drawEntity(VkCommandBuffer
commandBuffer,
uint32_t imageIndex, VEEntity
* entity) {

VkDeviceSize rayGenOffset = m_sbtGen.GetRayGenOffset();
VkDeviceSize missOffset = m_sbtGen.GetMissOffset();
VkDeviceSize missStride = m_sbtGen.GetMissEntrySize();
VkDeviceSize hitGroupOffset = m_sbtGen.GetHitGroupOffset();
VkDeviceSize hitGroupStride = m_sbtGen.GetHitGroupEntrySize();
auto swapChainExtent = getRendererRTPointer()->getSwapChainExtent();
vkCmdTraceRaysNV(commandBuffer, m_shaderBindingTableBuffer, rayGenOffset,
        m_shaderBindingTableBuffer, missOffset, missStride,
        m_shaderBindingTableBuffer, hitGroupOffset, hitGroupStride,
        VK_NULL_HANDLE,
0, 0, swapChainExtent.width,
swapChainExtent.height, 1);
}


/**
*
* \brief Add some maps of an entity to the map list of this subrenderer
*
* m_maps is a list of map lists. Each entity may need 1-N maps, like diffuse, normal, etc.
* m_maps thus has N entries, one for such a map type. m_map[0] might hold all diffuse maps.
* m_map[1] might hold all normal maps. m_map[2] might hold all specular maps. etc.
* If a new entity is added, then its maps are just appended to the N lists.
* However, we have descriptor sets, each set having N slot bindings (one binding for each map type),
* each binding describing an array of e.g. K textures.
* So we break each list into chunks of size K, and bind each chunk to one descriptor.
* Variable offset denotes the start of such a chunk.
*
* \param[in] pEntity Pointer to the entity
* \param[in] newMaps List of 1..N maps to be added to this subrenderer, e.g. {diffuse texture}, or {diffuse tex, normal}
*
*/
void VESubrenderRT_DN::addMaps(VEEntity * pEntity, std::vector<VkDescriptorImageInfo> & newMaps) {
    pEntity->setResourceIdx((uint32_t)
                                    m_entities.size());                                    //index into the array of textures for this entity, shader will take remainder and not absolute value

    uint32_t offset = 0;                                                                    //offset into the list of maps - index where a particular array starts
    if (pEntity->getResourceIdx() % m_resourceArrayLength ==
        0) {                            //array is full or there is none yet? -> we need a new array
        vh::vhRenderCreateDescriptorSets(getRendererRTPointer()->getDevice(),
                                         1,
                                         m_descriptorSetLayoutResources,                    //layout contains arrays of this size
                                         getRendererRTPointer()->getDescriptorPool(),
                                         m_descriptorSetsResources);

        offset = (uint32_t)
                m_maps[0].size();                                                //number of maps of first bind slot, e.g. number of diffuse maps

        for (uint32_t i = 0; i <
                             m_maps.size(); i++) {                                        //go through all map bind slots , e.g. first diffuse, then normal, then specular, then ...
            m_maps[i].resize(offset +
                             m_resourceArrayLength);                                //make room for the new array elements
            for (uint32_t j = offset; j < offset +
                                          m_resourceArrayLength; j++)                //have to fill the whole new array, even if there is only one map yet
                m_maps[i][j] = newMaps[i];                                                    //fill the new array up with copies of the first elemenet
        }
    } else {                                                                                    //we are inside an array, size is ALWAYS a multiple of m_resourceArrayLength!
        offset = (uint32_t)
                         m_maps[0].size() / m_resourceArrayLength - 1;                    //get index of array
        offset *= m_resourceArrayLength;                                                    //get start index where the array starts
        for (uint32_t i = 0; i < m_maps.size(); i++) {                                        //go through all map types
            m_maps[i][m_entities.size()] = newMaps[i];                                        //copy the map into the array
        }
    }

    vh::vhRenderUpdateDescriptorSetMaps(
            getRendererRTPointer()->getDevice(),                    //update the descriptor that holds the map array
            m_descriptorSetsResources[m_descriptorSetsResources.size() - 1],
            0,
            offset,                                                //start offset of the current map arrays that should be updated
            m_resourceArrayLength,
            m_maps);

}

/**
*
* \brief Removes an entity from this subrenderer - does NOT delete it
*
* Since we use indices and each entity knows its onw index, this is an O(1) operation.
*
* \param[in] pEntity Pointer to the entity to be removed
*
*/
void VESubrenderRT_DN::removeEntity(VEEntity * pEntity) {

    uint32_t
            size = (uint32_t)
            m_entities.size();
    if (size == 0) return;

    for (uint32_t i = 0; i < size; i++) {
        if (m_entities[i] == pEntity) {

            //move the last entity and its maps to the place of the removed entity
            m_entities[i] = m_entities[size - 1];            //replace with former last entity (could be identical)

            if (m_maps.size() > 0) {                        //are there maps?
                m_entities[i]->setResourceIdx(i);                //new resource index
                for (uint32_t j = 0; j < m_maps.size(); j++) {    //move also the map entries
                    m_maps[j][i] = m_maps[j][size - 1];
                }

                //update the descriptor set where the entity was removed
                uint32_t arrayIndex = (uint32_t) (i / m_resourceArrayLength);
                vh::vhRenderUpdateDescriptorSetMaps(getRendererRTPointer()->getDevice(),
                                                    m_descriptorSetsResources[arrayIndex],
                                                    0,
                                                    arrayIndex * m_resourceArrayLength,
                                                    m_resourceArrayLength, m_maps);

                //shrink the lists
                m_entities.pop_back();                                        //remove the last
                if (m_entities.size() % m_resourceArrayLength == 0) {        //shrunk?
                    for (uint32_t j = 0; j < m_maps.size(); j++) {
                        m_maps[j].resize(m_entities.size());                //remove map entries
                    }
                    m_descriptorSetsResources.resize(m_entities.size());    //remove descriptor sets
                }
            } else {
                m_entities.pop_back();                                        //remove the last
            }
            return;
        }
    }
}

/**
* \brief Add an entity to the subrenderer
*
* Create a UBO for this entity, a descriptor set per swapchain image, and update the descriptor sets
*
*/
void VESubrenderRT_DN::addEntity(VEEntity * pEntity) {

    std::vector<VkDescriptorImageInfo> maps = {
            pEntity->m_pMaterial->mapDiffuse->m_imageInfo,
            pEntity->m_pMaterial->mapNormal->m_imageInfo
    };

    addMaps(pEntity, maps);

    VESubrender::addEntity(pEntity);

}

}

