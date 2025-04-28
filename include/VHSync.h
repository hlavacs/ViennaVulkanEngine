#pragma once


namespace vvh {

	//---------------------------------------------------------------------------------------------

	struct SynCreateFenceInfo {
		const VkDevice& 		m_device;
		const size_t& 			m_size; 
		std::vector<VkFence>& 	m_fences;
	};

	template<typename T = SynCreateFenceInfo>	
	void SynCreateFences(T&& info) {
		for( int i = 0; i < info.m_size; ++i ) {
			VkFence fence;
			VkFenceCreateInfo fenceInfo{};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			if (vkCreateFence(info.m_device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
				throw std::runtime_error("failed to create synchronization objects for a frame!");
			}
			info.m_fences.push_back(fence);
		}
	}

	//---------------------------------------------------------------------------------------------

	struct SynDestroyFencesInfo {
		const VkDevice& m_device;
		std::vector<VkFence>& m_fences;
	};

	template<typename T = SynDestroyFencesInfo>
	void SynDestroyFences(T&& info) {
		for( int i = 0; i < info.m_fences.size(); ++i ) {
			vkDestroyFence(info.m_device, info.m_fences[i], nullptr);
		}
	}

	//---------------------------------------------------------------------------------------------

	struct SynCreateSemaphoresInfo {
		const VkDevice&				m_device; 
		std::vector<VkSemaphore>& 	m_imageAvailableSemaphores;
		std::vector<VkSemaphore>& 	m_renderFinishedSemaphores; 
		const size_t& 				m_size;
		std::vector<Semaphores>& 	m_intermediateSemaphores;
	};

	template<typename T = SynCreateSemaphoresInfo>
	void SynCreateSemaphores(T&& info ) {

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		for( size_t i = info.m_intermediateSemaphores.size(); i < info.m_size; ++i ) {
			Semaphores Sem;
			for (size_t j = 0; j < MAX_FRAMES_IN_FLIGHT; j++) {
				VkSemaphore semaphore;
				if (vkCreateSemaphore(info.m_device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS != VK_SUCCESS) {
					throw std::runtime_error("failed to create synchronization objects for a frame!");
				}
				Sem.m_renderFinishedSemaphores.push_back(semaphore);
			}
			info.m_intermediateSemaphores.push_back(Sem);
		}

		for (size_t j = info.m_imageAvailableSemaphores.size(); j < MAX_FRAMES_IN_FLIGHT; j++) {
			VkSemaphore semaphore;
			if (vkCreateSemaphore(info.m_device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS ) {
				throw std::runtime_error("failed to create synchronization objects for a frame!");
			}
			info.m_imageAvailableSemaphores.push_back(semaphore);
		}

		for (size_t j = info.m_renderFinishedSemaphores.size(); j < MAX_FRAMES_IN_FLIGHT; j++) {
			VkSemaphore semaphore;
			if (vkCreateSemaphore(info.m_device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS ) {
				throw std::runtime_error("failed to create synchronization objects for a frame!");
			}
			info.m_renderFinishedSemaphores.push_back(semaphore);
		}
	}
		
	//---------------------------------------------------------------------------------------------

	struct SynDestroySemaphoresInfo {
		const VkDevice m_device;
		std::vector<VkSemaphore>& m_imageAvailableSemaphores;
		std::vector<VkSemaphore>& m_renderFinishedSemaphores; 
		std::vector<Semaphores>& m_intermediateSemaphores;
	};

	template<typename T = SynDestroySemaphoresInfo>
   	void SynDestroySemaphores(T&& info) {

		for( auto Sem : info.m_intermediateSemaphores ) {
			for ( auto sem : Sem.m_renderFinishedSemaphores ) {
				vkDestroySemaphore(info.m_device, sem, nullptr);
			}
		}

		for ( auto sem : info.m_imageAvailableSemaphores) {
			vkDestroySemaphore(info.m_device, sem, nullptr);
		}
		for ( auto sem : info.m_renderFinishedSemaphores) {
			vkDestroySemaphore(info.m_device, sem, nullptr);
		}
	}


    

} // namespace vh

