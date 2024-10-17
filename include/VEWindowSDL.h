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
    class WindowSDL : public Window<ATYPE> {

        using Window<ATYPE>::m_engine;
        using Window<ATYPE>::m_surface;
        using Window<ATYPE>::m_clearColor;
    
    public:
        WindowSDL(Engine<ATYPE>& engine, VkInstance instance, std::string windowName
            , int width, int height, std::vector<const char*>& instance_extensions);
        virtual ~WindowSDL();
        virtual void Init() override;
        virtual bool pollEvents();
        virtual void prepareNextFrame() override;
        virtual void renderNextFrame() override;
        virtual auto getSize() -> std::pair<int, int>;

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

