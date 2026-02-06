#pragma once

/**
 * @file swapchain.h
 * @brief Swapchain creation, acquisition, and presentation helpers.
 */

namespace vve {
    /** Manages the Vulkan swapchain and per-frame semaphores. */
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
        /**
         * @param physicalDevice Physical device for surface support queries.
         * @param device Logical device.
         * @param surface Window surface.
         * @param presentQueue Present queue handle.
         * @param commandManager Command manager for copy/transition.
         * @param window SDL window pointer.
         */
        SwapChain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkQueue presentQueue, CommandManager* commandManager, SDL_Window* window);

        /** Release swapchain resources. */
        ~SwapChain();

        /** @return Swapchain image extent. */
        VkExtent2D getExtent();

        /** @return Swapchain image format. */
        VkFormat getFormat();

        /** @return Swapchain handle. */
        VkSwapchainKHR getSwapchain();

        /** @return Swapchain image index for the frame. */
        uint32_t getImageIndex(int currentFrame);

        /** Recreate swapchain and dependent resources. */
        void recreateSwapChain();

        /** Acquire the next swapchain image. */
        VkResult acquireNextImage(int currentFrame);

        /** Record image transfer from a render target to the swapchain image. */
        void recordImageTransfer(int currentFrame, RenderTarget* target);

        /** Present the current swapchain image. */
        VkResult presentImage(int currentFrame, VkSemaphore renderCompletSemaphore);

        /** @return Image-available semaphore for the frame. */
        VkSemaphore getImageAvailableSemaphore(int currentFrame);
    };

}
