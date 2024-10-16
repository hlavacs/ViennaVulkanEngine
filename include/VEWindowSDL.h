#pragma once

#include <vector>
#include <vulkan/vulkan.h>
//#include "imgui.h"
//#include "imgui_impl_sdl2.h"
//#include "imgui_impl_vulkan.h"
//#include <SDL.h>
//#include <SDL_vulkan.h>
#include "VEWindow.h"
#include "VHImgui.h"



namespace vve {

    class VeEngine;

    class VeWindowSDL : public VeWindow {
    
    public:
        VeWindowSDL(VeEngine& engine, VkInstance instance, std::string windowName
            , int width, int height, std::vector<const char*>& instance_extensions);
        virtual ~VeWindowSDL();
        virtual void Init() override;
        virtual bool pollEvents();
        virtual void renderNextFrame() override;
        virtual std::pair<int, int> getSize();

    private:
        bool InitSDL(VkInstance instance);
        inline static bool sdl_initialized{false};
        SDL_Window* m_window{nullptr};
        ImGui_ImplVulkanH_Window m_mainWindowData;
        int m_minImageCount = 2;
   		VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;	
        ImGuiIO* m_io;
        bool m_swapChainRebuild = false;
    };


};  // namespace vve

