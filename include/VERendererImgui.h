#pragma once

#include "VERendererVulkan.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    class Window;

   	template<ArchitectureType ATYPE>
    class RendererImgui : public Renderer<ATYPE>
    {
        using Renderer<ATYPE>::m_engine;
        using Renderer<ATYPE>::m_window;

    public:
        RendererImgui(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name = "VVE RendererImgui" );
        virtual ~RendererImgui();

    private:
        virtual void OnInit(Message message) override;
        virtual void OnPollEvents(Message message) override;
        virtual void OnPrepareNextFrame(Message message) override;
        virtual void OnRenderNextFrame(Message message) override;
        virtual void OnQuit(Message message) override;

   		VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;	

                //-------------------------------------------------------------------------

        ImGui_ImplVulkanH_Window m_mainWindowData;

        /*
        struct ImGui_ImplVulkanH_Frame
        {
            VkCommandPool       CommandPool;
            VkCommandBuffer     CommandBuffer;
            VkFence             Fence;
            VkImage             Backbuffer;
            VkImageView         BackbufferView;
            VkFramebuffer       Framebuffer;
        };

        struct ImGui_ImplVulkanH_FrameSemaphores
        {
            VkSemaphore         ImageAcquiredSemaphore;
            VkSemaphore         RenderCompleteSemaphore;
        };

        // Helper structure to hold the data needed by one rendering context into one OS window
        // (Used by example's main.cpp. Used by multi-viewport features. Probably NOT used by your own engine/app.)
        struct ImGui_ImplVulkanH_Window
        {
            int                 Width;
            int                 Height;
            VkSwapchainKHR      Swapchain;
            VkSurfaceKHR        Surface;
            VkSurfaceFormatKHR  SurfaceFormat;
            VkPresentModeKHR    PresentMode;
            VkRenderPass        RenderPass;
            VkPipeline          Pipeline;               // The window pipeline may uses a different VkRenderPass than the one passed in ImGui_ImplVulkan_InitInfo
            bool                UseDynamicRendering;
            bool                ClearEnable;
            VkClearValue        ClearValue;
            uint32_t            FrameIndex;             // Current frame being rendered to (0 <= FrameIndex < FrameInFlightCount)
            uint32_t            ImageCount;             // Number of simultaneous in-flight frames (returned by vkGetSwapchainImagesKHR, usually derived from min_image_count)
            uint32_t            SemaphoreCount;         // Number of simultaneous in-flight frames + 1, to be able to use it in vkAcquireNextImageKHR
            uint32_t            SemaphoreIndex;         // Current set of swapchain wait semaphores we're using (needs to be distinct from per frame data)
            ImGui_ImplVulkanH_Frame*            Frames;
            ImGui_ImplVulkanH_FrameSemaphores*  FrameSemaphores;

            ImGui_ImplVulkanH_Window()
            {
                memset((void*)this, 0, sizeof(*this));
                PresentMode = (VkPresentModeKHR)~0;     // Ensure we get an error if user doesn't set this.
                ClearEnable = true;
            }
        };
        */

    };

};   // namespace vve
