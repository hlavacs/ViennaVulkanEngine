/**
* The Vienna Vulkan Engine
*
* (c) bei Alexander Fomin, University of Vienna
*
*/

#ifndef VE_SUBRENDER_DF_COMPOSER_H
#define VE_SUBRENDER_DF_COMPOSER_H

namespace ve {

    /**
    * \brief Subrenderer that manages entities that have a diffuse texture and a normal map
    */
    class VESubrenderDF_Composer : public VESubrenderDF {
    protected:
        VkDescriptorSetLayout m_descriptorSetLayoutGBuffer = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_descriptorSetsGBuffer;
    public:
        ///Constructor
        VESubrenderDF_Composer(VERendererDeferred &renderer) : VESubrenderDF(renderer) {};
        ///Destructor
        virtual ~VESubrenderDF_Composer() {};

        ///\returns the class of the subrenderer
        virtual veSubrenderClass getClass() { return VE_SUBRENDERER_CLASS_COMPOSER; };
        ///\returns the type of the subrenderer
        virtual veSubrenderType getType() { return VE_SUBRENDERER_TYPE_NONE; };

        virtual void initSubrenderer();
        virtual void addEntity(VEEntity *pEntity);
        void bindDescriptorSet(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        void setDynamicPipelineState(VkCommandBuffer commandBuffer, uint32_t numPass);
        void draw(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t numPass,
                  VECamera *pCamera, VELight *pLight,
                  std::vector<VkDescriptorSet> descriptorSetsShadow) override;
    };
}

#endif