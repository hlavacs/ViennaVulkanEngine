#pragma once

#include <vector>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
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
#define GLM_FORCE_CXX14
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <stb_image.h>
#include <tiny_obj_loader.h>
#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"

#define IMGUI_IMPL_VULKAN_USE_VOLK
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"



namespace vh {


	//use this macro to check the function result, if its not VK_SUCCESS then return the error
    #define VHCHECKRESULT(x) { CheckResult(VkResult err) };
    void CheckResult(VkResult err);

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, pos);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, color);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

            return attributeDescriptions;
        }

        bool operator==(const Vertex& other) const {
            return pos == other.pos && color == other.color && texCoord == other.texCoord;
        }
    };

    struct UniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    struct SwapChain {
        VkSwapchainKHR m_swapChain;
        std::vector<VkImage> m_swapChainImages;
        VkFormat m_swapChainImageFormat;
        VkExtent2D m_swapChainExtent;
        std::vector<VkImageView> m_swapChainImageViews;
        std::vector<VkFramebuffer> m_swapChainFramebuffers;
    };

    struct Pipeline {
        VkPipelineLayout m_pipelineLayout;
        VkPipeline m_pipeline;
    };

    struct DepthImage {
        VkImage         m_depthImage;
        VmaAllocation   m_depthImageAllocation;
        VkImageView     m_depthImageView;
    };

    struct Texture {
        VkImage         m_textureImage;
        VmaAllocation   m_textureImageAllocation;
        VkImageView     m_textureImageView;
        VkSampler       m_textureSampler;
    };

    struct Geometry {
        std::vector<Vertex>     m_vertices;
        std::vector<uint32_t>   m_indices;
        VkBuffer                m_vertexBuffer;
        VmaAllocation           m_vertexBufferAllocation;
        VkBuffer                m_indexBuffer;
        VmaAllocation           m_indexBufferAllocation;
    };

    struct UniformBuffers {
        std::vector<VkBuffer>       m_uniformBuffers;
        std::vector<VmaAllocation>  m_uniformBuffersAllocation;
        std::vector<void*>          m_uniformBuffersMapped;
    };

    struct SyncObjects {
        std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::vector<VkFence>     m_inFlightFences;
    };

    void createInstance(const std::vector<const char*>& validationLayers
        , const std::vector<const char *>& extensions, VkInstance &instance);
    void initVMA(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator& allocator);
    void cleanupSwapChain(VkDevice device, VmaAllocator vmaAllocator, SwapChain& swapChain, DepthImage& depthImage);
    void recreateSwapChain(SDL_Window* window, VkSurfaceKHR surface
        , VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, SwapChain& swapChain
        , DepthImage& depthImage, VkRenderPass renderPass);
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    void setupDebugMessenger(VkInstance instance);
    void createSurface(VkInstance instance, SDL_Window *sdlWindow, VkSurfaceKHR& surface);
    void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice& physicalDevice);
    void createLogicalDevice(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, QueueFamilyIndices& queueFamilies
        , VkDevice& device, VkQueue& graphicsQueue, VkQueue& presentQueue);
    void createSwapChain(SDL_Window* window, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, SwapChain& swapChain);
    void createImageViews(VkDevice device, SwapChain& swapChain);
    void createRenderPass(VkPhysicalDevice physicalDevice, VkDevice device, SwapChain& swapChain, VkRenderPass& renderPass);
    void createDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout& descriptorSetLayout);
    void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass
        , VkDescriptorSetLayout descriptorSetLayout, Pipeline& graphicsPipeline);
    void createFramebuffers(VkDevice device, SwapChain& swapChain, DepthImage& depthImage, VkRenderPass renderPass);
    void createCommandPool(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool& commandPool);
    void createDepthResources(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , SwapChain& swapChain, DepthImage& depthImage);
    VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates
        , VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);
    bool hasStencilComponent(VkFormat format);
    void MemCopy(VkDevice device, void* source, VmaAllocationInfo& allocInfo, VkDeviceSize size);
    void createTextureImage(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , VkQueue graphicsQueue, VkCommandPool commandPool, Texture& texture);
    void createTextureImageView(VkDevice device, Texture& texture);
    void createTextureSampler(VkPhysicalDevice physicalDevice, VkDevice device, Texture &texture);
    VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    void createImage(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, uint32_t width, uint32_t height
        , VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties
        , VkImage& image, VmaAllocation& imageAllocation);
    void destroyImage(VkDevice device, VmaAllocator vmaAllocator, VkImage image, VmaAllocation& imageAllocation);
    void transitionImageLayout(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool
        , VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool
        , VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void loadModel( Geometry& geometry);
    void createVertexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , VkQueue graphicsQueue, VkCommandPool commandPool, Geometry& geometry);
    void createIndexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , VkQueue graphicsQueue, VkCommandPool commandPool, Geometry& geometry);
    void createUniformBuffers(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator& vmaAllocator
            , UniformBuffers &uniformBuffers);
    void createDescriptorPool(VkDevice device, VkDescriptorPool& descriptorPool);
    void createDescriptorSets(VkDevice device, Texture& texture
        , VkDescriptorSetLayout descriptorSetLayout, UniformBuffers& uniformBuffers, VkDescriptorPool descriptorPool
        , std::vector<VkDescriptorSet>& descriptorSets);
    void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties
        , VmaAllocationCreateFlags vmaFlags, VkBuffer& buffer
        , VmaAllocation& allocation, VmaAllocationInfo* allocationInfo = nullptr);
    void destroyBuffer(VkDevice device, VmaAllocator vmaAllocator, VkBuffer buffer, VmaAllocation& allocation);
    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);
    void endSingleTimeCommands(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer commandBuffer);
    void copyBuffer(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkBuffer srcBuffer
        , VkBuffer dstBuffer, VkDeviceSize size);
    void createCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers);
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex
        , SwapChain& swapChain, VkRenderPass renderPass, Pipeline& graphicsPipeline
        , Geometry& geometry, std::vector<VkDescriptorSet>& descriptorSets, uint32_t currentFrame);
    void createSyncObjects(VkDevice device, SyncObjects& syncObjects);
    void updateUniformBuffer(uint32_t currentImage, SwapChain& swapChain, UniformBuffers& uniformBuffers);
    void drawFrame(SDL_Window* window, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice
        , VkDevice device, VmaAllocator vmaAllocator, VkQueue graphicsQueue, VkQueue presentQueue
        , SwapChain& swapChain, DepthImage& depthImage, VkRenderPass renderPass
        , Pipeline& graphicsPipeline, Geometry& geometry, std::vector<VkCommandBuffer>& commandBuffers
        , UniformBuffers& uniformBuffers, std::vector<VkDescriptorSet>& descriptorSets
        , SyncObjects& syncObjects, uint32_t& currentFrame, bool& framebufferResized);
    
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* sdlWindow);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    bool isDeviceSuitable(VkPhysicalDevice device, const std::vector<const char *>&extensions , VkSurfaceKHR surface);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    bool checkValidationLayerSupport(const std::vector<const char*>& validationLayers);
    std::vector<char> readFile(const std::string& filename);
    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity
        , VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData
        , void* pUserData);
    void setupImgui(VkInstance instance, VkPhysicalDevice physicalDevice, QueueFamilyIndices queueFamilies
        , VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkDescriptorPool descriptorPool
        , VkRenderPass renderPass);

}
