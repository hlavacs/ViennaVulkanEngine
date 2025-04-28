#pragma once

#include <iostream>
#include <iomanip>

#ifdef VIENNA_VULKAN_HELPER_IMPL
	#define STB_IMAGE_IMPLEMENTATION
	#define STB_IMAGE_WRITE_IMPLEMENTATION
	#define VOLK_IMPLEMENTATION
	#define VMA_IMPLEMENTATION
	#define IMGUI_IMPL_VULKAN_USE_VOLK
#endif

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3_mixer/SDL_mixer.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <stb_image.h>
#include <stb_image_write.h>
#include "volk.h"
#include <VkBootstrap.h>
#include "vma/vk_mem_alloc.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"

#include "VHVulkan.h"

namespace vvh {
	/*void SetupImgui(SDL_Window* sdlWindow, VkInstance instance, VkPhysicalDevice physicalDevice, QueueFamilyIndices queueFamilies
		, VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkDescriptorPool descriptorPool
		, VkRenderPass renderPass);

	bool SDL3Init( std::string name, int width, int height, std::vector<std::string>& extensions );
	*/

	//extern VkInstance volkInstance;

	inline auto LoadVolk(const char* name, void* context) {
   		return vkGetInstanceProcAddr(volkInstance, name);
	}

    //------------------------------------------------------------------------

    inline void SetupImgui(SDL_Window* sdlWindow, VkInstance instance, VkPhysicalDevice physicalDevice, QueueFamilyIndices queueFamilies
        , VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkDescriptorPool descriptorPool
        , VkRenderPass renderPass) {
            
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

        ImGui_ImplVulkan_LoadFunctions( VK_API_VERSION_1_1, &LoadVolk );

        // Setup Platform/Renderer backends
        ImGui_ImplSDL3_InitForVulkan(sdlWindow);
        
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = instance;
        init_info.PhysicalDevice = physicalDevice;
        init_info.Device = device;
        init_info.QueueFamily = queueFamilies.graphicsFamily.value();
        init_info.Queue = graphicsQueue;
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = descriptorPool;
        init_info.RenderPass = renderPass;
        init_info.Subpass = 0;
        init_info.MinImageCount = 3;
        init_info.ImageCount = 3;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = nullptr;
        init_info.CheckVkResultFn = nullptr;
        ImGui_ImplVulkan_Init(&init_info);
        // (this gets a bit more complicated, see example app for full reference)
        //ImGui_ImplVulkan_CreateFontsTexture(YOUR_COMMAND_BUFFER);
        // (your code submit a queue)
        //ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    
    inline bool SDL3Init( std::string name, int width, int height, std::vector<std::string>& extensions) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to init SDL: %s", SDL_GetError());
            SDL_Quit();
            return EXIT_FAILURE;
        }
    
        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        SDL_Window *window = SDL_CreateWindow( name.c_str(), width, height, window_flags);
        if (!window) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create SDL window: %s", SDL_GetError());
            SDL_Quit();
            return EXIT_FAILURE;
        }
    
        unsigned int sdlExtensionCount = 0;
        const char * const *instance_extensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
        if (instance_extensions == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get SDL Vulkan instance extensions: %s", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return EXIT_FAILURE;
        }
    
        return true;
    }

};

