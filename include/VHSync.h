#pragma once


namespace vh {


	void SynCreateFences(VkDevice device, size_t size, std::vector<VkFence>& fences);

	void SynDestroyFences(VkDevice device, std::vector<VkFence>& fences);

    void SynCreateSemaphores(VkDevice device, 
		std::vector<VkSemaphore>& imageAvailableSemaphores, 
		std::vector<VkSemaphore>& renderFinishedSemaphores, 
		size_t size, std::vector<Semaphores>& intermediateSemaphores );

    void SynDestroySemaphores(VkDevice device, 
		std::vector<VkSemaphore>& imageAvailableSemaphores, 
		std::vector<VkSemaphore>& renderFinishedSemaphores, 
		std::vector<Semaphores>& intermediateSemaphores);

    

} // namespace vh

