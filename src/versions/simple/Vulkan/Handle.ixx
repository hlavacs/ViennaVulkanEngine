module;
#include <compare>
#include <vulkan/vulkan_raii.hpp>

export module VEEngine.Simple.Vulkan:OwnedHandle;

/**
	* @file
	* @brief Transitional Vulkan RAII handle adapter shared by Vulkan module partitions.
	*
	* Functional objects:
	* - VulkanOwnedHandle owns or tracks one vk::raii Vulkan object while exposing the legacy raw-handle view.
	*/
export namespace vve::simple {

	/// @brief Small owning RAII handle adapter that still exposes the raw Vulkan handle at old call sites.
	template <typename Raii, typename Raw>
	struct VulkanOwnedHandle {
		Raii handle{nullptr}; ///< RAII object owning or tracking the Vulkan handle.

		[[nodiscard]] operator Raw() const { return static_cast<Raw>(*handle); }
		[[nodiscard]] bool valid() const { return static_cast<Raw>(*handle) != VK_NULL_HANDLE; }
		/**
			* @brief Wraps a successfully created raw Vulkan handle in the owning RAII object.
			*/
		[[nodiscard]] VkResult assign(const vk::raii::Device &device, VkResult createResult, Raw raw) {
			if (createResult == VK_SUCCESS) { handle = Raii{device, raw}; }
			return createResult;
		}
		void reset() { handle = nullptr; }
	};

	template <typename Raii, typename Raw>
	[[nodiscard]] bool operator==(const VulkanOwnedHandle<Raii, Raw> &handle, Raw raw) { return static_cast<Raw>(handle) == raw; }

	template <typename Raii, typename Raw>
	[[nodiscard]] bool operator!=(const VulkanOwnedHandle<Raii, Raw> &handle, Raw raw) { return !(handle == raw); }

} // namespace vve::simple
