#pragma once


namespace vh {


	void SynCreateFences(VkDevice device, size_t size, std::vector<VkFence>& fences);

	void SynDestroyFences(VkDevice device, std::vector<VkFence>& fences);

    void SynCreateSemaphores(VkDevice device, size_t size, std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<Semaphores>& renderFinishedSemaphores);

    void SynDestroySemaphores(VkDevice device, std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<Semaphores>& semaphores);

    

} // namespace vh

