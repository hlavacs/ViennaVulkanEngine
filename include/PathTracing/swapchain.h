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

        void createSwapchain() {
            // Build the swapchain
            vkb::SwapchainBuilder swapchainBuilder{ physicalDevice, device, surface };

            auto swapchainResult = swapchainBuilder
                .set_old_swapchain(VK_NULL_HANDLE)
                .use_default_format_selection()
                .use_default_present_mode_selection()
                .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR) // optional override
                .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .build();

            if (!swapchainResult) {
                throw std::runtime_error("Failed to create swapchain!");
            }

            vkb::Swapchain vkbSwapchain = swapchainResult.value();

            // Retrieve swapchain details
            swapChain = vkbSwapchain.swapchain;
            swapChainImages = vkbSwapchain.get_images().value();
            swapChainImageFormat = vkbSwapchain.image_format;
            swapChainExtent = vkbSwapchain.extent;

            for (VkImage vkimage : swapChainImages) {
                Image* image = new Image(vkimage, swapChainExtent.width, swapChainExtent.height, swapChainImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
                images.push_back(image);
            }
        }

        void cleanupSwapChain() {
            //I cant use the normal image destructor to get rid of the images. Other way is needed. 
            images = std::vector<Image*>{};

            vkDestroySwapchainKHR(device, swapChain, nullptr);
        }

        void createSyncObject() {

            imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);


            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS) {

                    throw std::runtime_error("failed to create synchronization objects for a frame!");
                }
            }

        }


    public:
        SwapChain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkQueue presentQueue, CommandManager* commandManager, SDL_Window* window) :
            physicalDevice(physicalDevice), device(device), surface(surface), presentQueue(presentQueue), commandManager(commandManager), window(window) {
            createSwapchain();
            imageIndices.resize(MAX_FRAMES_IN_FLIGHT);
            createSyncObject();
        }

        ~SwapChain() {
            for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            }
            vkDestroySwapchainKHR(device, swapChain, nullptr);
        }

        VkExtent2D getExtent() {
            return swapChainExtent;
        }

        VkFormat getFormat() {
            return swapChainImageFormat;
        }

        VkSwapchainKHR getSwapchain() {
            return swapChain;
        }

        uint32_t getImageIndex(int currentFrame) {
            return imageIndices[currentFrame];
        }

        void recreateSwapChain() {
            vkDeviceWaitIdle(device);
            cleanupSwapChain();
            createSwapchain();
        }

        VkResult acquireNextImage(int currentFrame) {
            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

            imageIndices[currentFrame] = imageIndex;

            return result;
        }

        void recordImageTransfer(int currentFrame, RenderTarget* target) {
            images[imageIndices[currentFrame]]->recordCopyFromImage(target->getImage(currentFrame), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, currentFrame);;
        }

        VkResult presentImage(int currentFrame, VkSemaphore renderCompletSemaphore) {
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            VkSemaphore signalSemaphores[] = { renderCompletSemaphore, imageAvailableSemaphores[currentFrame] };

            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;

            //VkSwapchainKHR swapChains[] = { swapChain };
            VkSwapchainKHR swapChains[] = { swapChain };
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &imageIndices[currentFrame];
            presentInfo.pResults = nullptr; // Optional

            return vkQueuePresentKHR(presentQueue, &presentInfo);
        }

        VkSemaphore getImageAvailableSemaphore(int currentFrame) {
            return imageAvailableSemaphores[currentFrame];
        }
    };

}