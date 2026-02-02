#pragma once

namespace vve {
    class SwapChain {
    private:
        VkPhysicalDevice physicalDevice;
        VkDevice device;
        VkSurfaceKHR surface;
        CommandManager* commandManager;
        SDL_Window* window;

        VkSwapchainKHR swapChain;
        std::vector<VkImage> swapChainImages;
        std::vector<Image*> images;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        VkQueue presentQueue;

        std::vector<VkSemaphore> imageAvailableSemaphores;

        std::vector<uint32_t> imageIndices;

        void createSwapchain();

        void cleanupSwapChain();

        void createSyncObject();


    public:
        SwapChain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkQueue presentQueue, CommandManager* commandManager, SDL_Window* window);

        ~SwapChain();

        VkExtent2D getExtent();

        VkFormat getFormat();

        VkSwapchainKHR getSwapchain();

        uint32_t getImageIndex(int currentFrame);

        void recreateSwapChain();

        VkResult acquireNextImage(int currentFrame);

        void recordImageTransfer(int currentFrame, RenderTarget* target);

        VkResult presentImage(int currentFrame, VkSemaphore renderCompletSemaphore);

        VkSemaphore getImageAvailableSemaphore(int currentFrame);
    };

}