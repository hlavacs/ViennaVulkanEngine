

#include "VHInclude.h"


namespace vh {


	void SynCreateFences(VkDevice device, size_t size, std::vector<VkFence>& fences) {
		for( int i = 0; i < size; ++i ) {
			VkFence fence;
			VkFenceCreateInfo fenceInfo{};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
				throw std::runtime_error("failed to create synchronization objects for a frame!");
			}
			fences.push_back(fence);
		}
	}

	void SynDestroyFences(VkDevice device, std::vector<VkFence>& fences) {
		for( int i = 0; i < fences.size(); ++i ) {
			vkDestroyFence(device, fences[i], nullptr);
		}
	}

    void SynCreateSemaphores(VkDevice device, size_t size,  std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<Semaphores>& semaphores ) {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		for( size_t i = semaphores.size(); i < size; ++i ) {
			Semaphores Sem;
	        for (size_t j = 0; j < MAX_FRAMES_IN_FLIGHT; j++) {
				VkSemaphore semaphore;
	            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS != VK_SUCCESS) {
	                throw std::runtime_error("failed to create synchronization objects for a frame!");
	            }
				Sem.m_renderFinishedSemaphores.push_back(semaphore);
	        }
			semaphores.push_back(Sem);
		}

        for (size_t j = imageAvailableSemaphores.size(); j < MAX_FRAMES_IN_FLIGHT; j++) {
			VkSemaphore semaphore;
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS ) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
			imageAvailableSemaphores.push_back(semaphore);
        }

    }

    void SynDestroySemaphores(VkDevice device,  std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<Semaphores>& semaphores) {
		for( auto Sem : semaphores ) {
			for ( auto sem : Sem.m_renderFinishedSemaphores ) {
				vkDestroySemaphore(device, sem, nullptr);
			}
		}

		for ( auto sem : imageAvailableSemaphores) {
			vkDestroySemaphore(device, sem, nullptr);
		}
	}



} // namespace vh

