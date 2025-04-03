/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>
#include <unordered_map>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define VOLK_IMPLEMENTATION
#include "volk/volk.h"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#define IMGUI_IMPL_VULKAN_USE_VOLK
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

#include "VHVulkan.h"


namespace vh
{
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;
    
    const std::string MODEL_PATH   = "assets/models/viking_room.obj";
    const std::string TEXTURE_PATH = "assets/textures/viking_room.png";
    
	extern VkInstance volkInstance;

	auto VulLoadVolk(const char* name, void* context) {
   		return vkGetInstanceProcAddr(volkInstance, name);
	}

    std::vector<char> VulReadFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            std::cout << "failed to open file: " << filename << std::endl;
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }


    //------------------------------------------------------------------------


    void VulSetupImgui(SDL_Window* sdlWindow, VkInstance instance, VkPhysicalDevice physicalDevice, QueueFamilyIndices queueFamilies
        , VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkDescriptorPool descriptorPool
        , VkRenderPass renderPass) {
            
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

        ImGui_ImplVulkan_LoadFunctions( VK_API_VERSION_1_1, &VulLoadVolk );

        // Setup Platform/Renderer backends
        ImGui_ImplSDL2_InitForVulkan(sdlWindow);

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

} // namespace vh



