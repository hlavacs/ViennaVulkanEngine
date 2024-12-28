#pragma once


namespace vh {



	void createFences(VkDevice device, size_t size, std::vector<VkFence>& fences);

	void destroyFences(VkDevice device, std::vector<VkFence>& fences);

    void createSemaphores(VkDevice device, size_t size, std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<Semaphores>& renderFinishedSemaphores);

    void destroySemaphores(VkDevice device, std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<Semaphores>& semaphores);

    
	
} // namespace vh

