module;
#include <compare>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

export module VEEngine.Simple.Vulkan:Shadow;
import :Memory;
import std;

/**
	* @file
	* @brief Shadow-map depth arrays for the simple forward renderer.
	*
	* Functional objects:
	* - ShadowMap is a square D32 depth array with per-layer views and a comparison sampler; all shadow types share one pipeline.
	*/
export namespace vve::simple {

	/// @brief Square D32 shadow-map array: the VulkanImage base owns image, whole-array view, and one view per layer.
	struct ShadowMap : VulkanImage {
		static constexpr std::uint32_t resolution{1024U}; ///< Fixed square shadow-map side length in pixels.
		VkSampler shadowSampler{VK_NULL_HANDLE};          ///< Owned border-clamped comparison sampler.

		/// @brief Creates the depth array, its views, and the comparison sampler.
		[[nodiscard]] VkResult create(VmaAllocator allocator, VkDevice owningDevice, std::uint32_t layers) {
			cleanup();
			VkResult result = VulkanImage::create(allocator, owningDevice, VkExtent2D{.width = resolution, .height = resolution}, VK_FORMAT_D32_SFLOAT,
																VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																VK_IMAGE_ASPECT_DEPTH_BIT, layers, true);
			if (result != VK_SUCCESS) { return result; }
			const VkSamplerCreateInfo samplerInfo{
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter = VK_FILTER_LINEAR,
				.minFilter = VK_FILTER_LINEAR,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
				.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
				.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
				.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
				.compareEnable = VK_TRUE,
				.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
				.minLod = 0.0F,
				.maxLod = 1.0F,
				.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
			};
			result = vkCreateSampler(device, &samplerInfo, nullptr, &shadowSampler);
			if (result != VK_SUCCESS) { cleanup(); }
			return result;
		}

		/// @brief Destroys the sampler, views, and image.
		void cleanup() {
			if (shadowSampler != VK_NULL_HANDLE) { vkDestroySampler(device, shadowSampler, nullptr); }
			shadowSampler = VK_NULL_HANDLE;
			VulkanImage::cleanup();
		}

		~ShadowMap() { cleanup(); }
	};

} // namespace vve::simple
