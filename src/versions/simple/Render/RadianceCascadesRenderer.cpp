module;
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

module VVEngine.Simple.RadianceCascadesRenderer;
import std;
import VVEngine.Simple.Types;
import VVEngine.Simple.Vulkan;

/// @file
/// @brief RadianceCascadesRenderer lifecycle, frame loop and PNG capture.
///
/// Functional objects:
/// - init / cleanup: own Vulkan instance, ray tracing device, allocator, swapchain and frame synchronization.
/// - renderFrame / drawFrame / recordFrame: acquire, record, submit and present one frame.
/// - captureFrameToPng: renders one more frame and copies its swapchain image into a PNG.

namespace vve::simple {

	namespace {
		constexpr std::uint64_t frameFenceTimeoutNs{1'000'000'000ULL}; ///< A frame slot busy for longer skips the frame.
		constexpr std::uint64_t acquireTimeoutNs{100'000'000ULL};      ///< No swapchain image within this time skips the frame.
		constexpr VkFormat outputFormat{VK_FORMAT_R16G16B16A16_SFLOAT}; ///< Linear HDR; the blit to an sRGB swapchain encodes it.

		/// @brief Logs the failed stage with its Vulkan result and maps it to the engine error.
		[[nodiscard]] auto fail(const char *stage, VkResult result) -> std::unexpected<Error> {
			std::cerr << "[vve::simple] radiance cascades: " << stage << " failed: VkResult=" << static_cast<std::int32_t>(result) << '\n';
			return std::unexpected(Error::platform_error);
		}
	}

	/// @brief Creates the renderer's own Vulkan objects for one window and its first swapchain.
	/// @return Error::already_initialized, Error::invalid_argument for a null window, or Error::platform_error when
	/// Vulkan fails, for example because no device supports the ray tracing extensions.
	auto RadianceCascadesRenderer::init(SDL_Window *window) -> std::expected<void, Error> {
		if (initialized_) { return std::unexpected(Error::already_initialized); }
		if (window == nullptr) { return std::unexpected(Error::invalid_argument); }
		window_ = window;

		// Rays are traced with the ray tracing pipeline against acceleration structures; both need buffer device addresses.
		static constexpr std::array rayTracingExtensions{VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
			VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};
		VkPhysicalDeviceBufferDeviceAddressFeatures addressFeatures{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES, .bufferDeviceAddress = VK_TRUE};
		VkPhysicalDeviceAccelerationStructureFeaturesKHR structureFeatures{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, .pNext = &addressFeatures, .accelerationStructure = VK_TRUE};
		const VkPhysicalDeviceRayTracingPipelineFeaturesKHR pipelineFeatures{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, .pNext = &structureFeatures, .rayTracingPipeline = VK_TRUE};

		const char *stage{"instance"};
		VkResult result = instance_.create("VVE Radiance Cascades");
		if (result == VK_SUCCESS) { stage = "surface"; result = surface_.create(instance_.instance, window); }
		if (result == VK_SUCCESS) { stage = "device selection"; result = physical_device_.select(instance_.instance, surface_.surface, rayTracingExtensions); }
		if (result == VK_SUCCESS) { stage = "device"; result = device_.create(physical_device_, rayTracingExtensions, &pipelineFeatures); }
		if (result == VK_SUCCESS) {
			stage = "allocator";
			VkPhysicalDeviceProperties properties{};
			vkGetPhysicalDeviceProperties(physical_device_.physicalDevice, &properties);
			// VMA needs a version both instance and device support.
			result = allocator_.create(instance_.instance, physical_device_.physicalDevice, device_.device,
				std::min<std::uint32_t>(properties.apiVersion, VK_API_VERSION_1_3), VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);
		}
		if (result == VK_SUCCESS) { stage = "command pool"; result = command_pool_.create(device_.device, *physical_device_.graphicsQueueFamily); }
		if (result == VK_SUCCESS) { stage = "command buffers"; result = command_buffers_.create(device_.device, command_pool_.commandPool, framesInFlight); }
		if (result == VK_SUCCESS) { stage = "swapchain"; result = createSwapchainResources(windowPixelExtent()); }
		if (result != VK_SUCCESS) {
			cleanup();
			return fail(stage, result);
		}
		initialized_ = true;
		return {};
	}

	/// @brief Waits for the device, then releases every Vulkan object in reverse creation order.
	auto RadianceCascadesRenderer::cleanup() -> void {
		waitIdle();
		cascades_.clear();
		frame_sync_.cleanup();
		output_.cleanup();
		swapchain_.cleanup();
		command_buffers_.cleanup();
		command_pool_.cleanup();
		allocator_.cleanup();
		device_.cleanup();
		surface_.cleanup();
		instance_.cleanup();
		window_ = nullptr;
		current_frame_ = 0U;
		initialized_ = false;
	}

	/// @brief Waits until the renderer's own device has finished all submitted work.
	auto RadianceCascadesRenderer::waitIdle() -> void {
		if (device_.device != VK_NULL_HANDLE) { (void)vkDeviceWaitIdle(device_.device); }
	}

	/// @brief Reads the window's drawable pixel extent.
	VkExtent2D RadianceCascadesRenderer::windowPixelExtent() const {
		int width{};
		int height{};
		if (window_ != nullptr) { SDL_GetWindowSizeInPixels(window_, &width, &height); }
		return {.width = static_cast<std::uint32_t>(std::max(width, 0)), .height = static_cast<std::uint32_t>(std::max(height, 0))};
	}

	/// @brief Creates the swapchain and everything sized by it: the output image and the frame synchronization.
	VkResult RadianceCascadesRenderer::createSwapchainResources(VkExtent2D extent) {
		VkResult result = swapchain_.create(physical_device_.physicalDevice, device_.device, surface_.surface,
			*physical_device_.graphicsQueueFamily, *physical_device_.presentQueueFamily, extent.width, extent.height);
		if (result == VK_SUCCESS) {
			// Ray generation shaders will write it as a storage image; the frame clears it and blits it.
			result = output_.create(allocator_, device_.device, swapchain_.extent, outputFormat,
				VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
		}
		if (result == VK_SUCCESS) {
			result = frame_sync_.create(device_.device, framesInFlight, static_cast<std::uint32_t>(swapchain_.images.size()));
		}
		current_frame_ = 0U;
		return result;
	}

	/// @brief Replaces the swapchain resources when the window size differs from the one they were created for.
	/// A failure clears the requested extent, so the next frame tries again.
	VkResult RadianceCascadesRenderer::syncSwapchain(VkExtent2D extent) {
		if (extent.width == swapchain_.requestedExtent.width && extent.height == swapchain_.requestedExtent.height) { return VK_SUCCESS; }
		waitIdle();
		frame_sync_.cleanup();
		output_.cleanup();
		swapchain_.cleanup();
		const VkResult result = createSwapchainResources(extent);
		if (result != VK_SUCCESS) { swapchain_.requestedExtent = {}; }
		return result;
	}

	/// @brief Renders and presents one frame; a temporarily unavailable window or image is a successful skip.
	/// @return Error::not_initialized before init, Error::platform_error when Vulkan fails.
	auto RadianceCascadesRenderer::renderFrame() -> std::expected<void, Error> {
		if (!initialized_) { return std::unexpected(Error::not_initialized); }
		const auto presented = drawFrame(nullptr);
		if (!presented) { return std::unexpected(presented.error()); }
		if (*presented) { ++rendered_frames_; }
		return {};
	}

	/// @brief Acquires a swapchain image, records and submits the frame, optionally copies it out, and presents it.
	/// @return True when the frame was presented, false when it was skipped.
	auto RadianceCascadesRenderer::drawFrame(VulkanReadback *readback) -> std::expected<bool, Error> {
		const VkExtent2D windowExtent{windowPixelExtent()};
		if (windowExtent.width == 0U || windowExtent.height == 0U) { return false; } // Minimized.
		if (const VkResult synced = syncSwapchain(windowExtent); synced != VK_SUCCESS) { return fail("swapchain recreate", synced); }

		const VkFence inFlight{frame_sync_.inFlightFences[current_frame_]};
		const VkSemaphore imageAvailable{frame_sync_.imageAvailableSemaphores[current_frame_]};
		VkResult result = vkWaitForFences(device_.device, 1U, &inFlight, VK_TRUE, frameFenceTimeoutNs);
		if (result == VK_TIMEOUT) { return false; }
		if (result != VK_SUCCESS) { return fail("fence wait", result); }

		std::uint32_t imageIndex{};
		result = vkAcquireNextImageKHR(device_.device, swapchain_.swapchain, acquireTimeoutNs, imageAvailable, VK_NULL_HANDLE, &imageIndex);
		if (result == VK_TIMEOUT || result == VK_NOT_READY) { return false; }
		if (result == VK_ERROR_OUT_OF_DATE_KHR) { swapchain_.requestedExtent = {}; return false; }
		// A suboptimal image was still acquired and must be rendered and presented.
		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) { return fail("image acquire", result); }

		const VkCommandBuffer command{command_buffers_.ownedCommandBuffers[current_frame_]};
		const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
		result = vkResetCommandBuffer(command, 0U);
		if (result == VK_SUCCESS) { result = vkBeginCommandBuffer(command, &beginInfo); }
		if (result != VK_SUCCESS) { return fail("command begin", result); }
		recordFrame(command, imageIndex);
		result = vkEndCommandBuffer(command);
		if (result == VK_SUCCESS) { result = vkResetFences(device_.device, 1U, &inFlight); }
		if (result != VK_SUCCESS) { return fail("command end", result); }

		const VkSemaphore renderFinished{frame_sync_.renderFinishedSemaphores[imageIndex]}; // Follows the acquired image.
		const VkPipelineStageFlags waitStage{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};           // The barriers in recordFrame use the same stage.
		const VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1U,
			.pWaitSemaphores = &imageAvailable,
			.pWaitDstStageMask = &waitStage,
			.commandBufferCount = 1U,
			.pCommandBuffers = &command,
			.signalSemaphoreCount = 1U,
			.pSignalSemaphores = &renderFinished,
		};
		result = vkQueueSubmit(device_.graphicsQueue, 1U, &submitInfo, inFlight);
		if (result != VK_SUCCESS) { return fail("queue submit", result); }
		// The copy is submitted to the same queue, so it runs after the frame and before presentation releases the image.
		if (readback != nullptr) {
			result = readback->capture(swapchain_.images[imageIndex], 0U, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			if (result != VK_SUCCESS) { return fail("frame readback", result); }
		}

		const VkSwapchainKHR presentSwapchain{swapchain_.swapchain};
		const VkPresentInfoKHR presentInfo{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1U,
			.pWaitSemaphores = &renderFinished,
			.swapchainCount = 1U,
			.pSwapchains = &presentSwapchain,
			.pImageIndices = &imageIndex,
		};
		result = vkQueuePresentKHR(device_.presentQueue, &presentInfo);
		current_frame_ = (current_frame_ + 1U) % framesInFlight;
		// Both results ask for a new swapchain on the next frame; only the suboptimal image reached the screen.
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) { swapchain_.requestedExtent = {}; }
		if (result == VK_ERROR_OUT_OF_DATE_KHR) { return false; }
		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) { return fail("present", result); }
		return true;
	}

	/// @brief Records one frame: clear the output image, run the lighting stages, blit the result to the swapchain image.
	void RadianceCascadesRenderer::recordFrame(VkCommandBuffer command, std::uint32_t imageIndex) {
		constexpr VkImageAspectFlags colour{VK_IMAGE_ASPECT_COLOR_BIT};
		const VkImage target{swapchain_.images[imageIndex]};
		const auto background = background_color_.value;
		const VkClearColorValue clear{.float32 = {background.x, background.y, background.z, 1.0F}};
		const VkImageSubresourceRange range{.aspectMask = colour, .levelCount = 1U, .layerCount = 1U};
		// The previous contents of both images are not needed.
		transitionImage(command, output_.image, colour, 0U, 1U, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vkCmdClearColorImage(command, output_.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1U, &range);

		record(command);

		transitionImage(command, output_.image, colour, 0U, 1U, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		transitionImage(command, target, colour, 0U, 1U, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		const VkOffset3D corner{.x = static_cast<std::int32_t>(swapchain_.extent.width), .y = static_cast<std::int32_t>(swapchain_.extent.height), .z = 1};
		const VkImageBlit blit{
			.srcSubresource = {.aspectMask = colour, .layerCount = 1U},
			.srcOffsets = {VkOffset3D{}, corner},
			.dstSubresource = {.aspectMask = colour, .layerCount = 1U},
			.dstOffsets = {VkOffset3D{}, corner},
		};
		vkCmdBlitImage(command, output_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U, &blit, VK_FILTER_NEAREST);
		transitionImage(command, target, colour, 0U, 1U, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}

	/// @brief Renders one more frame and writes its swapchain image as a PNG; the frame is not counted.
	/// @return Error::not_initialized, Error::invalid_argument for an empty path, Error::io_error when the file cannot be
	/// written, or Error::platform_error when the frame could not be presented.
	auto RadianceCascadesRenderer::captureFrameToPng(const std::filesystem::path &output_path) -> std::expected<void, Error> {
		if (!initialized_) { return std::unexpected(Error::not_initialized); }
		if (output_path.empty()) { return std::unexpected(Error::invalid_argument); }
		if (const auto parent = output_path.parent_path(); !parent.empty()) {
			auto error = std::error_code{};
			std::filesystem::create_directories(parent, error);
			if (error) { return std::unexpected(Error::io_error); }
		}
		// The readback buffer must match the swapchain the frame will use.
		VkResult result = syncSwapchain(windowPixelExtent());
		if (result != VK_SUCCESS) { return fail("swapchain recreate", result); }
		auto readback = VulkanReadback{};
		result = readback.create(allocator_, device_.device, device_.graphicsQueue, command_pool_.commandPool, swapchain_.extent, swapchain_.imageFormat);
		if (result != VK_SUCCESS) { return fail("readback buffer", result); }
		const auto presented = drawFrame(&readback);
		if (!presented) { return std::unexpected(presented.error()); }
		if (!*presented || !readback.valid) { return std::unexpected(Error::platform_error); }
		if (!writeReadbackPng(readback.pixelBytes(), swapchain_.extent, swapchain_.imageFormat, output_path.string())) {
			return std::unexpected(Error::io_error);
		}
		return {};
	}

} // namespace vve::simple
