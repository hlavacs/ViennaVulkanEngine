module;
#include <compare>
#include <vulkan/vulkan.h>
#include <stb_image_write.h>
#include <vk_mem_alloc.h>

export module VEEngine.Simple.Vulkan:Readback;
import :Memory;
import std;

/**
	* @file
	* @brief GPU-to-CPU image readback and deterministic PNG encoding for captured frames and shadow maps.
	*
	* Functional objects:
	* - VulkanReadback copies one layer of a color or D32 depth image into a mapped host buffer.
	* - writeReadbackPng encodes VulkanReadback color bytes as an opaque RGBA8 PNG file.
	*/
export namespace vve::simple {

	/// @brief Host-visible copy of one image layer; works for 32-bit color formats and D32 depth.
	struct VulkanReadback {
		VkDevice device{VK_NULL_HANDLE};       ///< Borrowed device used for the one-time copy.
		VkQueue queue{VK_NULL_HANDLE};         ///< Borrowed queue used for the one-time copy.
		VkCommandPool commandPool{VK_NULL_HANDLE}; ///< Borrowed pool used for the one-time copy.
		VkExtent2D extent{};                   ///< Captured layer size in pixels.
		VkFormat format{VK_FORMAT_UNDEFINED};  ///< Source image format.
		VkImageAspectFlags aspect{};           ///< Color or depth aspect derived from the format.
		VulkanBuffer buffer{};                 ///< Owned mapped destination buffer holding the last capture.
		bool valid{false};                     ///< True after a successful capture.

		VulkanReadback() = default;
		VulkanReadback(const VulkanReadback &) = delete;
		VulkanReadback &operator=(const VulkanReadback &) = delete;
		~VulkanReadback() { cleanup(); }

		/// @brief Creates the destination buffer for one layer of the given extent and format.
		[[nodiscard]] VkResult create(VmaAllocator allocator, VkDevice owningDevice, VkQueue graphicsQueue, VkCommandPool pool, VkExtent2D layerExtent, VkFormat imageFormat) {
			cleanup();
			const std::optional<VkDeviceSize> bytesPerPixel = formatByteSize(imageFormat);
			if (!bytesPerPixel || owningDevice == VK_NULL_HANDLE || graphicsQueue == VK_NULL_HANDLE || pool == VK_NULL_HANDLE || layerExtent.width == 0U || layerExtent.height == 0U) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			const VkResult result = buffer.create(allocator, static_cast<VkDeviceSize>(layerExtent.width) * layerExtent.height * *bytesPerPixel, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
			if (result != VK_SUCCESS) { return result; }
			device = owningDevice;
			queue = graphicsQueue;
			commandPool = pool;
			extent = layerExtent;
			format = imageFormat;
			aspect = imageFormat == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			return VK_SUCCESS;
		}

		/**
			* @brief Copies one image layer into the host buffer, transitioning it from and back to `currentLayout`.
			*/
		[[nodiscard]] VkResult capture(VkImage sourceImage, std::uint32_t layer, VkImageLayout currentLayout) {
			valid = false;
			if (sourceImage == VK_NULL_HANDLE || buffer.buffer == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }
			const VkResult result = submitOnce(device, queue, commandPool, [&](VkCommandBuffer commandBuffer) {
				transitionImage(commandBuffer, sourceImage, aspect, layer, 1U, currentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				const VkBufferImageCopy region{
					.imageSubresource = {.aspectMask = aspect, .mipLevel = 0U, .baseArrayLayer = layer, .layerCount = 1U},
					.imageExtent = {.width = extent.width, .height = extent.height, .depth = 1U},
				};
				vkCmdCopyImageToBuffer(commandBuffer, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer.buffer, 1U, &region);
				transitionImage(commandBuffer, sourceImage, aspect, layer, 1U, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, currentLayout);
			});
			valid = result == VK_SUCCESS;
			return result;
		}

		/// @brief Returns the last captured bytes in source format order.
		[[nodiscard]] std::span<const std::byte> pixelBytes() const { return valid ? buffer.bytes() : std::span<const std::byte>{}; }

		/// @brief Reads one float texel from the last captured D32 layer.
		[[nodiscard]] std::optional<float> depthAt(std::uint32_t x, std::uint32_t y) const {
			if (!valid || format != VK_FORMAT_D32_SFLOAT || x >= extent.width || y >= extent.height) { return std::nullopt; }
			float depth{};
			std::memcpy(&depth, buffer.bytes().data() + (static_cast<std::size_t>(y) * extent.width + x) * sizeof(float), sizeof(float));
			return depth;
		}

		void cleanup() {
			buffer.cleanup();
			device = VK_NULL_HANDLE;
			queue = VK_NULL_HANDLE;
			commandPool = VK_NULL_HANDLE;
			extent = {};
			format = VK_FORMAT_UNDEFINED;
			aspect = 0U;
			valid = false;
		}

	private:
		/// @brief Bytes per pixel of the formats the readback supports.
		[[nodiscard]] static std::optional<VkDeviceSize> formatByteSize(VkFormat imageFormat) {
			switch (imageFormat) {
			case VK_FORMAT_R8G8B8A8_UNORM:
			case VK_FORMAT_R8G8B8A8_SRGB:
			case VK_FORMAT_B8G8R8A8_UNORM:
			case VK_FORMAT_B8G8R8A8_SRGB:
			case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
			case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
			case VK_FORMAT_D32_SFLOAT:
				return 4U;
			default:
				return std::nullopt;
			}
		}
	};

	/**
		* @brief Encodes VulkanReadback color bytes as an opaque 8-bit RGBA PNG for deterministic analysis.
		*
		* @param pixels CPU-side image bytes in the exact Vulkan source format order.
		* @param extent Width and height of the readback image in pixels.
		* @param sourceFormat Vulkan source format; only 8-bit RGBA and BGRA UNORM/SRGB are supported.
		* @param outputPath Filesystem path passed to stb_image_write.
		* @return True when validation, conversion, and stbi_write_png succeeded.
		*/
	[[nodiscard]] inline bool writeReadbackPng(std::span<const std::byte> pixels, VkExtent2D extent, VkFormat sourceFormat, std::string_view outputPath) {
		const bool rgbaFormat = sourceFormat == VK_FORMAT_R8G8B8A8_UNORM || sourceFormat == VK_FORMAT_R8G8B8A8_SRGB;
		const bool bgraFormat = sourceFormat == VK_FORMAT_B8G8R8A8_UNORM || sourceFormat == VK_FORMAT_B8G8R8A8_SRGB;
		if ((!rgbaFormat && !bgraFormat) || extent.width == 0U || extent.height == 0U || outputPath.empty()) { return false; }

		constexpr std::size_t channels{4U}; // PNG output is always 8-bit RGBA.
		const std::size_t width = static_cast<std::size_t>(extent.width);
		const std::size_t height = static_cast<std::size_t>(extent.height);
		if (height > std::numeric_limits<std::size_t>::max() / width / channels) { return false; }
		const std::size_t byteCount = width * height * channels;
		if (pixels.size() != byteCount || width > static_cast<std::size_t>(std::numeric_limits<int>::max() / channels)) { return false; }

		auto rgbaPixels = std::vector<unsigned char>(byteCount); // Owns contiguous bytes required by stb_image_write.
		for (std::size_t offset{}; offset < byteCount; offset += channels) {
			const unsigned char first = std::to_integer<unsigned char>(pixels[offset + 0U]);
			const unsigned char second = std::to_integer<unsigned char>(pixels[offset + 1U]);
			const unsigned char third = std::to_integer<unsigned char>(pixels[offset + 2U]);
			rgbaPixels[offset + 0U] = bgraFormat ? third : first; // Convert B to R only for BGRA sources.
			rgbaPixels[offset + 1U] = second;
			rgbaPixels[offset + 2U] = bgraFormat ? first : third; // Convert R to B only for BGRA sources.
			rgbaPixels[offset + 3U] = 255U;                      // Analysis PNGs ignore source alpha.
		}

		const auto path = std::string{outputPath}; // stb requires a null-terminated path.
		const int stride = static_cast<int>(width * channels);
		return stbi_write_png(path.c_str(), static_cast<int>(extent.width), static_cast<int>(extent.height), 4, rgbaPixels.data(), stride) != 0;
	}

} // namespace vve::simple
