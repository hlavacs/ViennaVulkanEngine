/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"
#include "VESubrenderDF_Composer.h"
#include "VERendererDeferred.h"


namespace ve {

    /**
    * \brief Initialize the subrenderer
    *
    * Create descriptor set layout, pipeline layout and the PSO
    *
    */
    void VESubrenderDF_Composer::initSubrenderer() {
        VESubrender::initSubrenderer();

        //create descriptor for the GBuffer

        vh::vhRenderCreateDescriptorSetLayout(m_renderer.getDevice(),
            { 1,									1,                                    1 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,	VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,  VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT },
            { VK_SHADER_STAGE_FRAGMENT_BIT,		    VK_SHADER_STAGE_FRAGMENT_BIT,         VK_SHADER_STAGE_FRAGMENT_BIT },
            &m_descriptorSetLayoutGBuffer);


        VkDescriptorSetLayout perObjectLayout = m_renderer.getDescriptorSetLayoutPerObject();
        vh::vhPipeCreateGraphicsPipelineLayout(m_renderer.getDevice(),
            { perObjectLayout, perObjectLayout, m_renderer.getDescriptorSetLayoutShadow(), m_descriptorSetLayoutGBuffer },
            {  },
            &m_pipelineLayout);

        m_pipelines.resize(1);
        vh::vhPipeCreateGraphicsPipeline(m_renderer.getDevice(),
            { "media/shader/Deferred/Composition/vert.spv", "media/shader/Deferred/Composition/frag.spv" },
            m_renderer.getSwapChainExtent(),
            m_pipelineLayout, m_renderer.getRenderPass(),
            { VK_DYNAMIC_STATE_BLEND_CONSTANTS },
            &m_pipelines[0], 1, VK_CULL_MODE_FRONT_BIT, 1);

        vh::vhRenderCreateDescriptorSets(m_renderer.getDevice(),
            (uint32_t)m_renderer.getSwapChainNumber(),
            m_descriptorSetLayoutGBuffer,
            m_renderer.getDescriptorPool(),
            m_descriptorSetsGBuffer);

        for(uint32_t i = 0; i < m_descriptorSetsGBuffer.size(); i++) {
            vh::vhRenderUpdateDescriptorSet(m_renderer.getDevice(),
                m_descriptorSetsGBuffer[i],
                { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT },  // Descriptor Types
                { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE }, //UBOs
                { 0 },	//UBO sizes
                {
                    { m_renderer.getPositionMap()->m_imageInfo.imageView },
                    { m_renderer.getNormalMap()->m_imageInfo.imageView   },
                    { m_renderer.getAlbedoMap()->m_imageInfo.imageView   }
                },	//textureImageViews
                { { VK_NULL_HANDLE }, {VK_NULL_HANDLE }, {VK_NULL_HANDLE} } 	//samplers
            );

        }
    }

    /**
    *
    * \brief Add an entity to the list of associated entities.
    *
    * \param[in] pEntity Pointer to the entity to include into the list.
    *
    */
    void VESubrenderDF_Composer::addEntity(VEEntity *pEntity) {
        m_entities.push_back(pEntity);
    }

    /**
    *
    * \brief Bind default descriptor sets - 0...per object 1...per frame
    *
    * The function binds the default descriptor sets -  0...per object 1...per frame.
    * Can be overloaded.
    *
    * \param[in] commandBuffer The command buffer to record into all draw calls
    * \param[in] imageIndex Index of the current swap chain image
    * \param[in] entity Pointer to the entity to draw
    *
    */
    void VESubrenderDF_Composer::bindDescriptorSet(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        
        //set 3...per frame, includes cam and shadow matrices
        std::vector<VkDescriptorSet> sets =
        {
                m_descriptorSetsGBuffer[imageIndex],
        };

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 3, (uint32_t)sets.size(), sets.data(), 0, nullptr);
    }


    void VESubrenderDF_Composer::setDynamicPipelineState(VkCommandBuffer commandBuffer, uint32_t numPass) {
        float blendConstants[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        vkCmdSetBlendConstants(commandBuffer, blendConstants);

    }



    /**
    * \brief Draw all associated entities for the shadow pass
    *
    * The subrenderer maintains a list of all associated entities. In this function it goes through all of them
    * and draws them. A vector is used in order to be able to parallelize this in case thousands or objects are in the list
    *
    * \param[in] commandBuffer The command buffer to record into all draw calls
    * \param[in] imageIndex Index of the current swap chain image
    *
    */
    void VESubrenderDF_Composer::draw(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t numPass,
                                      VECamera *pCamera, VELight *pLight,
                                      std::vector<VkDescriptorSet> descriptorSetsShadow) {
        bindPipeline(commandBuffer);
        setDynamicPipelineState(commandBuffer, numPass);
        bindDescriptorSetsPerFrame(commandBuffer, imageIndex, pCamera, pLight, descriptorSetsShadow);
        bindDescriptorSet(commandBuffer, imageIndex);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }



}

