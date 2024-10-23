#pragma once

#include <vector>
#include <set>
#include <vulkan/vulkan.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include "VEWindow.h"



namespace vve {

   	template<ArchitectureType ATYPE>
    class Engine;

   	template<ArchitectureType ATYPE>
    class RendererImgui;

   	template<ArchitectureType ATYPE>
    class WindowSDL : public Window<ATYPE> {

        using Window<ATYPE>::m_engine;
        using Window<ATYPE>::m_surface;
        using Window<ATYPE>::m_clearColor;
        using Window<ATYPE>::m_renderer;

        friend class RendererImgui<ATYPE>;
    
    public:
        WindowSDL(Engine<ATYPE>* engine, VkInstance instance, std::string windowName
            , int width, int height, std::vector<const char*>& instance_extensions, std::string name = "VVE WindowSDL" );
        virtual ~WindowSDL();
        virtual auto GetSize() -> std::pair<int, int>;

    private:
        //bool InitSDL(VkInstance instance);
        virtual void OnInit(Message message) override;
        virtual void OnPollEvents(Message message) override;
        virtual void OnPrepareNextFrame(Message message) override;
        virtual void OnRenderNextFrame(Message message) override;
        virtual void OnQuit(Message message) override;

        inline static bool sdl_initialized{false};
        SDL_Window* m_window{nullptr};
        int m_minImageCount = 2;
        bool m_swapChainRebuild = false;
        std::set<SDL_Scancode> m_keysDown;
        std::set<uint8_t> m_mouseButtonsDown;

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


};  // namespace vve

