#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan.h>

import std;
import VEEngine.Simple.Renderer;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

/**
 * @file
 * @brief End-to-end executable for the simple forward renderer.
 *
 * Functional objects:
 * - main creates one SDL3 Vulkan window, loads the sample scene before renderer init, draws a few frames, captures one deterministic swapchain image, waits for device idle, and tears down Vulkan and SDL resources.
 */

/**
 * @brief Exercises the simple renderer Vulkan RAII chain and fixed forward draw path.
 *
 * @return Zero after the renderer draws the fixed frame count, non-zero with the failing stage and backend error text.
 */
int main() {
	SDL_SetMainReady();
#ifdef VVE_SDL_VULKAN_LIBRARY
	SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, VVE_SDL_VULKAN_LIBRARY);
#endif
	if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
		std::println(stderr, "simple_forward_demo failed: stage=SDL video init, sdl_error={}", SDL_GetError());
		return 1;
	}

	const char *const videoDriver = SDL_GetCurrentVideoDriver(); // Driver name proves the active display backend.
	std::println("simple_forward_demo SDL video driver: {}", videoDriver == nullptr ? "<none>" : videoDriver);

	SDL_Window *const window = SDL_CreateWindow("VVE Simple Forward Demo", 800, 600, SDL_WINDOW_VULKAN);
	if (window == nullptr) {
		std::println(stderr, "simple_forward_demo failed: stage=SDL Vulkan window creation, sdl_error={}", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return 2;
	}
	std::println("simple_forward_demo window created");

	vve::simple::Renderer renderer{}; // Renderer owns all Vulkan objects created after scene upload data is loaded.
	renderer.loadScene(vve::simple::makeSampleScene());
	std::println("simple_forward_demo scene loaded before renderer init");

	if (const VkResult result = renderer.init(window); result != VK_SUCCESS) {
		std::println(stderr,
						 "simple_forward_demo failed: stage=renderer init, vk_result={}, sdl_error={}",
						 static_cast<int>(result),
						 SDL_GetError());
		renderer.cleanup();
		SDL_DestroyWindow(window);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return 3;
	}
	std::println("simple_forward_demo renderer init");

	constexpr std::string_view capturePath{"simple_forward_demo_capture.png"}; // Stable path used by automatic image analysis.
	vve::simple::VulkanReadback readback{};
	const VkResult readbackCreateResult = readback.create(
		renderer.physicalDevice.physicalDevice,
		renderer.device.device,
		renderer.device.graphicsQueue,
		renderer.commandPool.commandPool,
		renderer.swapchain.extent,
		renderer.swapchain.imageFormat);
	std::println("simple_forward_demo readback create vk_result={}", static_cast<int>(readbackCreateResult));

	// Draw a short fixed sequence so command recording, queue submit, and presentation run in this process.
	for (int frame = 0; frame < 3; ++frame) {
		SDL_Event event{};
		while (SDL_PollEvent(&event)) {}
		renderer.drawFrame(frame == 0 ? &readback : nullptr);
		std::println("simple_forward_demo frame {} drawn", frame + 1);

		// Capture only the first fixed frame after presentation work has reached idle.
		if (frame == 0) {
			VkResult captureWaitResult = vkDeviceWaitIdle(renderer.device.device);
			std::println("simple_forward_demo capture wait vk_result={}", static_cast<int>(captureWaitResult));

			const VkResult captureResult = captureWaitResult == VK_SUCCESS
				? renderer.lastReadbackCaptureResult.value_or(VK_ERROR_INITIALIZATION_FAILED)
				: captureWaitResult;
			const bool pngWritten = captureResult == VK_SUCCESS && vve::simple::writeReadbackPng(
				readback.pixelBytes(),
				renderer.swapchain.extent,
				renderer.swapchain.imageFormat,
				capturePath);
			std::println("simple_forward_demo readback capture vk_result={}", static_cast<int>(captureResult));
			std::println("simple_forward_demo readback png_written={}, path={}", pngWritten, capturePath);

			// Summarize the captured bytes so automated checks can reject clear-only frames.
			std::filesystem::create_directories("docs");
			std::ofstream analysis{"docs/simple_forward_demo_render_analysis.txt", std::ios::trunc};
			const auto pixels = readback.pixelBytes();
			const bool rgbaFormat = renderer.swapchain.imageFormat == VK_FORMAT_R8G8B8A8_UNORM
				|| renderer.swapchain.imageFormat == VK_FORMAT_R8G8B8A8_SRGB;
			const bool bgraFormat = renderer.swapchain.imageFormat == VK_FORMAT_B8G8R8A8_UNORM
				|| renderer.swapchain.imageFormat == VK_FORMAT_B8G8R8A8_SRGB;
			constexpr std::size_t channels{4U};           // The simple readback path stores 8-bit color formats.
			constexpr unsigned int clearR{};              // Renderer clears color to opaque black.
			constexpr unsigned int clearG{};
			constexpr unsigned int clearB{};
			constexpr unsigned int threshold{8U};         // Fixed tolerance hides tiny conversion noise.
			const std::size_t width = static_cast<std::size_t>(renderer.swapchain.extent.width);
			const std::size_t height = static_cast<std::size_t>(renderer.swapchain.extent.height);
			const bool validSize = width != 0U && height != 0U
				&& width <= std::numeric_limits<std::size_t>::max() / height / channels
				&& pixels.size() == width * height * channels;
			std::array<std::size_t, 64U> histogram{};      // Four coarse bins per RGB channel.
			std::size_t nonBackgroundPixels{};
			std::size_t minX{width};
			std::size_t minY{height};
			std::size_t maxX{};
			std::size_t maxY{};

			if ((rgbaFormat || bgraFormat) && validSize && captureResult == VK_SUCCESS) {
				// Walk pixels once and classify only color channels against the known clear color.
				for (std::size_t y{}; y < height; ++y) {
					for (std::size_t x{}; x < width; ++x) {
						const std::size_t offset = (y * width + x) * channels;
						const unsigned int first = std::to_integer<unsigned int>(pixels[offset + 0U]);
						const unsigned int second = std::to_integer<unsigned int>(pixels[offset + 1U]);
						const unsigned int third = std::to_integer<unsigned int>(pixels[offset + 2U]);
						const unsigned int r = bgraFormat ? third : first;
						const unsigned int g = second;
						const unsigned int b = bgraFormat ? first : third;
						const bool differsFromClear = std::abs(static_cast<int>(r) - static_cast<int>(clearR)) > static_cast<int>(threshold)
							|| std::abs(static_cast<int>(g) - static_cast<int>(clearG)) > static_cast<int>(threshold)
							|| std::abs(static_cast<int>(b) - static_cast<int>(clearB)) > static_cast<int>(threshold);
						if (differsFromClear) {
							++nonBackgroundPixels;
							minX = std::min(minX, x);
							minY = std::min(minY, y);
							maxX = std::max(maxX, x);
							maxY = std::max(maxY, y);
							++histogram[(r / 64U) * 16U + (g / 64U) * 4U + (b / 64U)];
						}
					}
				}
			}

			std::vector<std::pair<std::size_t, std::size_t>> bucketCounts; // Pair is bucket index and count.
			for (std::size_t bucket{}; bucket < histogram.size(); ++bucket) {
				if (histogram[bucket] != 0U) { bucketCounts.emplace_back(bucket, histogram[bucket]); }
			}
			std::sort(bucketCounts.begin(), bucketCounts.end(), [](const auto &left, const auto &right) {
				return left.second == right.second ? left.first < right.first : left.second > right.second;
			});
			const std::size_t dominantThreshold = nonBackgroundPixels == 0U ? 1U : std::max<std::size_t>(1U, nonBackgroundPixels / 100U);
			const std::size_t dominantBucketCount = static_cast<std::size_t>(std::ranges::count_if(bucketCounts, [dominantThreshold](const auto &bucket) {
				return bucket.second >= dominantThreshold;
			}));
			const double fraction = width == 0U || height == 0U ? 0.0 : static_cast<double>(nonBackgroundPixels) / static_cast<double>(width * height);
			analysis << "analysis_status=" << (((rgbaFormat || bgraFormat) && validSize && captureResult == VK_SUCCESS) ? "ok" : "invalid_or_unsupported") << '\n';
			analysis << "image_format=" << static_cast<int>(renderer.swapchain.imageFormat) << '\n';
			analysis << "format_order=" << (rgbaFormat ? "rgba" : (bgraFormat ? "bgra" : "unsupported")) << '\n';
			analysis << "extent_width=" << width << '\n';
			analysis << "extent_height=" << height << '\n';
			analysis << "total_pixels=" << (width * height) << '\n';
			analysis << "non_background_pixels=" << nonBackgroundPixels << '\n';
			analysis << "non_background_fraction=" << std::fixed << std::setprecision(8) << fraction << '\n';
			analysis << "non_background_bbox_min_x=" << (nonBackgroundPixels == 0U ? -1 : static_cast<int>(minX)) << '\n';
			analysis << "non_background_bbox_min_y=" << (nonBackgroundPixels == 0U ? -1 : static_cast<int>(minY)) << '\n';
			analysis << "non_background_bbox_max_x=" << (nonBackgroundPixels == 0U ? -1 : static_cast<int>(maxX)) << '\n';
			analysis << "non_background_bbox_max_y=" << (nonBackgroundPixels == 0U ? -1 : static_cast<int>(maxY)) << '\n';
			analysis << "histogram_bucket_count=" << histogram.size() << '\n';
			analysis << "non_background_histogram_nonzero_buckets=" << bucketCounts.size() << '\n';
			analysis << "dominant_non_background_bucket_count=" << dominantBucketCount << '\n';
			for (std::size_t index{}; index < std::min<std::size_t>(bucketCounts.size(), 8U); ++index) {
				const std::size_t bucket = bucketCounts[index].first;
				analysis << "top_bucket_" << index << "_rgb_bin=" << (bucket / 16U) << ',' << ((bucket / 4U) % 4U) << ',' << (bucket % 4U) << '\n';
				analysis << "top_bucket_" << index << "_count=" << bucketCounts[index].second << '\n';
			}
		}
	}

	if (const VkResult result = vkDeviceWaitIdle(renderer.device.device); result != VK_SUCCESS) {
		std::println(stderr, "simple_forward_demo failed: stage=device idle wait, vk_result={}", static_cast<int>(result));
		readback.cleanup();
		renderer.cleanup();
		SDL_DestroyWindow(window);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return 4;
	}
	std::println("simple_forward_demo device idle");

	// Keep teardown owner state machine-readable before the logical device is destroyed.
	std::ofstream teardownDiagnostics{"docs/simple_forward_demo_runtime_diagnostics.txt", std::ios::trunc};
	teardownDiagnostics << "teardown: readback buffer live before cleanup=" << (readback.buffer.buffer != VK_NULL_HANDLE) << '\n';
	teardownDiagnostics << "teardown: readback memory live before cleanup=" << (readback.buffer.memory != VK_NULL_HANDLE) << '\n';
	readback.cleanup();
	teardownDiagnostics << "teardown: readback buffer live after cleanup=" << (readback.buffer.buffer != VK_NULL_HANDLE) << '\n';
	teardownDiagnostics << "teardown: readback memory live after cleanup=" << (readback.buffer.memory != VK_NULL_HANDLE) << '\n';
	teardownDiagnostics << "teardown: renderer device live before cleanup=" << (renderer.device.device != VK_NULL_HANDLE) << '\n';
	renderer.cleanup();
	teardownDiagnostics << "teardown: renderer device live after cleanup=" << (renderer.device.device != VK_NULL_HANDLE) << '\n';
	SDL_DestroyWindow(window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	std::println("simple_forward_demo cleanup done");
	return 0;
}
