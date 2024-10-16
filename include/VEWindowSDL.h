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

    class Engine;

    class WindowSDL : public Window {
    
    public:
        WindowSDL(Engine& engine, VkInstance instance, std::string windowName
            , int width, int height, std::vector<const char*>& instance_extensions);
        virtual ~WindowSDL();
        virtual void Init() override;
        virtual bool pollEvents();
        virtual void prepareNextFrame() override;
        virtual void renderNextFrame() override;
        virtual std::pair<int, int> getSize();

    private:
        bool InitSDL(VkInstance instance);
        void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data);
        void FramePresent(ImGui_ImplVulkanH_Window* wd);

        inline static bool sdl_initialized{false};
        SDL_Window* m_window{nullptr};
        ImGui_ImplVulkanH_Window m_mainWindowData;
        int m_minImageCount = 2;
   		VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;	
        ImGuiIO* m_io;
        bool m_swapChainRebuild = false;
        std::set<SDL_Scancode> m_keysDown;
        std::set<uint8_t> m_mouseButtonsDown;
    };


};  // namespace vve

