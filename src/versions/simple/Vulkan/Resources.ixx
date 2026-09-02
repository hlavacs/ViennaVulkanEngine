module;
#include <compare>
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#define VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL_vulkan.h>
#include <stb_image.h>
#ifdef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif

export module VEEngine.Simple.Vulkan:Resources;
import :Device;
import :Commands;
import :Readback;
import :OwnedHandle;
import std;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Math;
import VEEngine.Simple.Scene;
import VEEngine.Vector;

/**
	* @file
	* @brief Vulkan GPU resource ownership for textures, meshes, frame uniforms, descriptor pools, and descriptor sets.
	*
	* Functional objects:
	* - TextureImage owns one sampled RGBA8 texture image, view, sampler, and backing memory.
	* - VulkanDescriptorPool owns only VkDescriptorPool creation and teardown for uniform-buffer, shadow-map, and object-texture descriptor sets.
	* - VulkanDescriptorSets allocates per-frame uniform-buffer, shadow-map, and object-texture descriptor sets from a borrowed pool and layout.
	* - VulkanMesh owns the vertex and index buffers and index count for one uploaded CPU mesh.
	* - FrameUniforms stores frame matrices plus GPU-packed point, directional, and spot light data for set 0 binding 0.
	* - VulkanUniformBuffers owns one host-visible FrameUniforms buffer per frame.
	*/
export namespace vve::simple {

	/// @brief Internal owner for one sampled 8-bit RGBA texture image uploaded with a staging buffer.
	struct TextureImage {
		VkDevice device{VK_NULL_HANDLE};                                      ///< Borrowed Vulkan logical device used for upload commands.
		VulkanOwnedHandle<vk::raii::Image, VkImage> image{};                  ///< Owned device-local sampled color image.
		VulkanOwnedHandle<vk::raii::DeviceMemory, VkDeviceMemory> memory{};   ///< Owned device-local memory backing the texture image.
		VulkanOwnedHandle<vk::raii::ImageView, VkImageView> imageView{};      ///< Owned RGBA image view for shader sampling.
		VulkanOwnedHandle<vk::raii::Sampler, VkSampler> textureSampler{};     ///< Owned linear repeat sampler for texture reads.
		VkExtent2D extent{};                                                  ///< Loaded texture width and height in pixels.

		TextureImage() = default;
		TextureImage(const TextureImage &) = delete;
		TextureImage &operator=(const TextureImage &) = delete;

		/**
			* @brief Loads an RGBA file from disk and uploads it into a sampled SRGB Vulkan image.
			*
			* @param physicalDevice Physical device used to select memory types.
			* @param owningDevice Logical device that owns the image, view, sampler, and upload commands.
			* @param graphicsQueue Queue used for the one-time staging copy submission.
			* @param commandPool Command pool used for the temporary upload command buffer.
			* @param imagePath Filesystem path passed to stb_image for 8-bit RGBA loading.
			* @return VK_SUCCESS when the texture is ready for descriptor binding, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(
			VkPhysicalDevice physicalDevice,
			const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice,
			VkQueue graphicsQueue,
			VkCommandPool commandPool,
			const std::filesystem::path &imagePath
		) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE || owningDevice == VK_NULL_HANDLE || graphicsQueue == VK_NULL_HANDLE || commandPool == VK_NULL_HANDLE || imagePath.empty()) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			int width{};
			int height{};
			int channels{};
			const auto pathString = imagePath.string(); // stb_image requires a null-terminated filesystem path.
			auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>{
				stbi_load(pathString.c_str(), &width, &height, &channels, STBI_rgb_alpha),
				stbi_image_free
			};
			if (!pixels || width <= 0 || height <= 0) { return VK_ERROR_INITIALIZATION_FAILED; }

			const auto textureExtent = VkExtent2D{.width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height)};
			const VkDeviceSize byteCount = static_cast<VkDeviceSize>(textureExtent.width) * textureExtent.height * 4U;
			return create(physicalDevice, owningDevice, graphicsQueue, commandPool, std::span{reinterpret_cast<const std::byte *>(pixels.get()), static_cast<std::size_t>(byteCount)}, textureExtent);
		}

		/**
			* @brief Uploads tight RGBA bytes into a sampled SRGB Vulkan image.
			*
			* @param physicalDevice Physical device used to select memory types.
			* @param owningDevice Logical device that owns the image, view, sampler, and upload commands.
			* @param graphicsQueue Queue used for the one-time staging copy submission.
			* @param commandPool Command pool used for the temporary upload command buffer.
			* @param rgbaPixels CPU-side 8-bit RGBA pixels matching extent width times height.
			* @param textureExtent Texture width and height in pixels.
			* @return VK_SUCCESS when the texture is ready for descriptor binding, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(
			VkPhysicalDevice physicalDevice,
			const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice,
			VkQueue graphicsQueue,
			VkCommandPool commandPool,
			std::span<const std::byte> rgbaPixels,
			VkExtent2D textureExtent
		) {
			cleanup();
			const VkDeviceSize byteCount = static_cast<VkDeviceSize>(textureExtent.width) * textureExtent.height * 4U;
			if (physicalDevice == VK_NULL_HANDLE || owningDevice == VK_NULL_HANDLE || graphicsQueue == VK_NULL_HANDLE || commandPool == VK_NULL_HANDLE || textureExtent.width == 0U || textureExtent.height == 0U || rgbaPixels.size() != static_cast<std::size_t>(byteCount)) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			VulkanBuffer stagingBuffer{};
			VkResult result = stagingBuffer.create(
				physicalDevice,
				owningDevice,
				byteCount,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			);
			if (result != VK_SUCCESS) { return result; }

			result = stagingBuffer.upload(rgbaPixels.data(), byteCount);
			if (result != VK_SUCCESS) { return result; }

			device = owningDevice;
			extent = textureExtent;
			result = createImage(physicalDevice, owningDevice);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = uploadFrom(stagingBuffer.buffer, graphicsQueue, commandPool);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = createViewAndSampler(owningDevice);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			return VK_SUCCESS;
		}

		/**
			* @brief Destroys the owned sampler, view, image, memory, and clears the borrowed device handle.
			*/
		void cleanup() {
			textureSampler.reset();
			imageView.reset();
			image.reset();
			memory.reset();
			extent = {};
			device = VK_NULL_HANDLE;
		}

	private:
		/**
			* @brief Creates and binds the device-local sampled image allocation.
			*
			* @param physicalDevice Physical device used to query image memory requirements.
			* @return VK_SUCCESS when the image and memory binding are ready, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult createImage(VkPhysicalDevice physicalDevice, const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice) {
			const VkImageCreateInfo imageInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_R8G8B8A8_SRGB,
				.extent = {.width = extent.width, .height = extent.height, .depth = 1U},
				.mipLevels = 1U,
				.arrayLayers = 1U,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			};

			VkImage rawImage{VK_NULL_HANDLE};
			VkResult result = vkCreateImage(device, &imageInfo, nullptr, &rawImage);
			if (result != VK_SUCCESS) { return result; }
			image.handle = vk::raii::Image{owningDevice.handle, rawImage};

			VkMemoryRequirements requirements{};
			vkGetImageMemoryRequirements(device, image, &requirements);
			const std::optional<std::uint32_t> memoryType = findMemoryType(
				physicalDevice,
				requirements.memoryTypeBits,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			);
			if (!memoryType.has_value()) { return VK_ERROR_FEATURE_NOT_PRESENT; }

			const VkMemoryAllocateInfo allocateInfo{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = requirements.size,
				.memoryTypeIndex = *memoryType,
			};

			VkDeviceMemory rawMemory{VK_NULL_HANDLE};
			result = vkAllocateMemory(device, &allocateInfo, nullptr, &rawMemory);
			if (result != VK_SUCCESS) { return result; }
			memory.handle = vk::raii::DeviceMemory{owningDevice.handle, rawMemory};
			return vkBindImageMemory(device, image, memory, 0U);
		}

		/**
			* @brief Uploads staging-buffer pixels with one command buffer and leaves the image shader-readable.
			*
			* @param stagingBuffer Borrowed transfer-source buffer containing RGBA pixel bytes.
			* @param graphicsQueue Queue receiving the one-time upload command buffer.
			* @param commandPool Command pool used to allocate the temporary primary command buffer.
			* @return VK_SUCCESS when the copy has completed on the queue, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult uploadFrom(VkBuffer stagingBuffer, VkQueue graphicsQueue, VkCommandPool commandPool) {
			VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
			const VkCommandBufferAllocateInfo allocateInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = commandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1U,
			};
			VkResult result = vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer);
			if (result != VK_SUCCESS) { return result; }

			VkFence fence{VK_NULL_HANDLE};
			const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
			result = vkCreateFence(device, &fenceInfo, nullptr, &fence);
			if (result != VK_SUCCESS) { vkFreeCommandBuffers(device, commandPool, 1U, &commandBuffer); return result; }

			result = recordUpload(commandBuffer, stagingBuffer);
			if (result == VK_SUCCESS) {
				const VkSubmitInfo submitInfo{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.commandBufferCount = 1U,
					.pCommandBuffers = &commandBuffer,
				};
				result = vkQueueSubmit(graphicsQueue, 1U, &submitInfo, fence);
			}
			if (result == VK_SUCCESS) { result = vkWaitForFences(device, 1U, &fence, VK_TRUE, UINT64_MAX); }

			vkDestroyFence(device, fence, nullptr);
			vkFreeCommandBuffers(device, commandPool, 1U, &commandBuffer);
			return result;
		}

		/**
			* @brief Records the texture layout transitions and buffer-to-image copy.
			*
			* @param commandBuffer Temporary primary command buffer receiving upload commands.
			* @param stagingBuffer Borrowed transfer-source buffer containing tightly packed RGBA pixels.
			* @return VK_SUCCESS when recording completed, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult recordUpload(VkCommandBuffer commandBuffer, VkBuffer stagingBuffer) {
			const VkCommandBufferBeginInfo beginInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			};
			VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
			if (result != VK_SUCCESS) { return result; }

			VulkanReadback::transitionImage(commandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			const VkBufferImageCopy copyRegion{
				.bufferOffset = 0U,
				.bufferRowLength = 0U,
				.bufferImageHeight = 0U,
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0U,
					.baseArrayLayer = 0U,
					.layerCount = 1U,
				},
				.imageOffset = {.x = 0, .y = 0, .z = 0},
				.imageExtent = {.width = extent.width, .height = extent.height, .depth = 1U},
			};
			vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U, &copyRegion);
			VulkanReadback::transitionImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			return vkEndCommandBuffer(commandBuffer);
		}

		/**
			* @brief Creates the sampled RGBA image view and linear repeat sampler.
			*
			* @return VK_SUCCESS when both sampling handles are ready, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult createViewAndSampler(const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice) {
			const VkImageViewCreateInfo viewInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = image,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = VK_FORMAT_R8G8B8A8_SRGB,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0U,
					.levelCount = 1U,
					.baseArrayLayer = 0U,
					.layerCount = 1U,
				},
			};

			VkImageView rawView{VK_NULL_HANDLE};
			VkResult result = vkCreateImageView(device, &viewInfo, nullptr, &rawView);
			if (result != VK_SUCCESS) { return result; }
			imageView.handle = vk::raii::ImageView{owningDevice.handle, rawView};

			const VkSamplerCreateInfo samplerInfo{
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter = VK_FILTER_LINEAR,
				.minFilter = VK_FILTER_LINEAR,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
				.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.minLod = 0.0F,
				.maxLod = 1.0F,
			};

			VkSampler rawSampler{VK_NULL_HANDLE};
			result = vkCreateSampler(device, &samplerInfo, nullptr, &rawSampler);
			if (result != VK_SUCCESS) { return result; }
			textureSampler.handle = vk::raii::Sampler{owningDevice.handle, rawSampler};
			return VK_SUCCESS;
		}
	};


	/// @brief Minimal Vulkan descriptor-pool owner for uniform-buffer, shadow-map, and object-texture descriptor sets.
	struct VulkanDescriptorPool {
		VulkanOwnedHandle<vk::raii::DescriptorPool, VkDescriptorPool> descriptorPool{}; ///< Owned descriptor pool.

		VulkanDescriptorPool() = default;
		VulkanDescriptorPool(const VulkanDescriptorPool &) = delete;
		VulkanDescriptorPool &operator=(const VulkanDescriptorPool &) = delete;

		/**
			* @brief Creates a descriptor pool for per-frame uniform-buffer, shadow-map, and object-texture descriptor sets.
			*
			* @param owningDevice Logical device that owns the descriptor pool.
			* @param maxSets Maximum descriptor-set count.
			* @param descriptorBindings Slang-reflected set bindings used to size the pool.
			* @return VK_SUCCESS when the descriptor pool is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice, std::uint32_t maxSets, const Vector<VkDescriptorSetLayoutBinding> &descriptorBindings) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			std::vector<VkDescriptorPoolSize> poolSizes{}; // One pool entry per reflected descriptor type.
			for (const VkDescriptorSetLayoutBinding &binding : descriptorBindings) {
				auto poolSize = std::ranges::find(poolSizes, binding.descriptorType, &VkDescriptorPoolSize::type);
				if (poolSize == poolSizes.end()) { poolSize = poolSizes.insert(poolSize, VkDescriptorPoolSize{.type = binding.descriptorType}); }
				poolSize->descriptorCount += maxSets;
			}
			if (poolSizes.size() != 2U) { return VK_ERROR_INITIALIZATION_FAILED; }
			const auto uniformPoolSize = std::ranges::find(poolSizes, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &VkDescriptorPoolSize::type);
			const auto samplerPoolSize = std::ranges::find(poolSizes, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &VkDescriptorPoolSize::type);
			if (uniformPoolSize == poolSizes.end() || samplerPoolSize == poolSizes.end() || uniformPoolSize->descriptorCount != maxSets || samplerPoolSize->descriptorCount != maxSets * 6U) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkDescriptorPoolCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.maxSets = maxSets,
				.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
				.pPoolSizes = poolSizes.data(),
			};

			VkDescriptorPool rawDescriptorPool{VK_NULL_HANDLE};
			const VkResult result = vkCreateDescriptorPool(owningDevice, &createInfo, nullptr, &rawDescriptorPool);
			return descriptorPool.assign(owningDevice.handle, result, rawDescriptorPool);
		}

		/**
			* @brief Releases the owned descriptor pool through its RAII wrapper.
			*/
		void cleanup() { descriptorPool.reset(); }
	};

	/// @brief Minimal Vulkan descriptor-set owner for per-frame uniform-buffer, shadow-map, and object-texture bindings.
	struct VulkanDescriptorSets {
		VkDevice device{VK_NULL_HANDLE};                         ///< Borrowed device used for allocation and updates.
		VkDescriptorPool descriptorPool{VK_NULL_HANDLE};          ///< Borrowed pool that owns the allocations; sets are freed implicitly with the pool.
		Vector<VkDescriptorSetLayoutBinding> descriptorBindings{}; ///< Slang-reflected set-0 bindings used when writing descriptors.
		std::optional<std::uint32_t> objectTextureBinding{};      ///< Reflected set-0 binding for the object base-color texture.
		std::array<std::optional<std::uint32_t>, 5U> shadowSamplerBindings{}; ///< Reflected set-0 bindings for all shadow samplers.
		std::vector<VkDescriptorSet> descriptorSets{};            ///< Owned descriptor sets allocated one per frame.

		VulkanDescriptorSets() = default;
		VulkanDescriptorSets(const VulkanDescriptorSets &) = delete;
		VulkanDescriptorSets &operator=(const VulkanDescriptorSets &) = delete;

		/**
			* @brief Allocates one descriptor set per frame from a borrowed pool and layout.
			*
			* @param owningDevice Logical device that owns the descriptor pool.
			* @param pool Descriptor pool used for descriptor-set allocation.
			* @param setLayout Descriptor-set layout repeated for every frame set.
			* @param reflectedBindings Slang-reflected set-0 binding contract used by descriptor writes.
			* @param reflectedObjectTextureBinding Slang-reflected binding for the named object base-color texture.
			* @param reflectedShadowSamplerBindings Slang-reflected bindings for the named shadow-map sampler parameters.
			* @param count Number of per-frame descriptor sets to allocate.
			* @return VK_SUCCESS when all descriptor sets are allocated, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkDevice owningDevice, VkDescriptorPool pool, VkDescriptorSetLayout setLayout, const Vector<VkDescriptorSetLayoutBinding> &reflectedBindings, std::uint32_t reflectedObjectTextureBinding, std::array<std::uint32_t, 5U> reflectedShadowSamplerBindings, std::uint32_t count) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE || pool == VK_NULL_HANDLE || setLayout == VK_NULL_HANDLE) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			const auto uniformBinding = std::ranges::find(reflectedBindings, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &VkDescriptorSetLayoutBinding::descriptorType);
			if (uniformBinding == reflectedBindings.end()) { return VK_ERROR_INITIALIZATION_FAILED; }
			const auto hasSamplerBinding = [&reflectedBindings](std::uint32_t binding) {
				const auto layoutBinding = std::ranges::find(reflectedBindings, binding, &VkDescriptorSetLayoutBinding::binding);
				return layoutBinding != reflectedBindings.end() && layoutBinding->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			};
			if (!hasSamplerBinding(reflectedObjectTextureBinding) || !std::ranges::all_of(reflectedShadowSamplerBindings, hasSamplerBinding)) { return VK_ERROR_INITIALIZATION_FAILED; }

			device = owningDevice;
			descriptorPool = pool;
			descriptorBindings = reflectedBindings;
			objectTextureBinding = reflectedObjectTextureBinding;
			std::ranges::transform(reflectedShadowSamplerBindings, shadowSamplerBindings.begin(), [](std::uint32_t binding) { return std::optional{binding}; });
			auto layouts = std::vector<VkDescriptorSetLayout>(count, setLayout);
			const VkDescriptorSetAllocateInfo allocateInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = pool,
				.descriptorSetCount = count,
				.pSetLayouts = layouts.data(),
			};

			descriptorSets.resize(count);
			const VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, descriptorSets.data());
			if (result != VK_SUCCESS) {
				descriptorSets.clear();
				descriptorBindings.clear();
				objectTextureBinding.reset();
				std::ranges::fill(shadowSamplerBindings, std::nullopt);
				device = VK_NULL_HANDLE;
				descriptorPool = VK_NULL_HANDLE;
			}
			return result;
		}

		/**
			* @brief Writes one frame descriptor set with its binding-0 uniform buffer.
			*
			* @param frameIndex Frame set index to update.
			* @param uniformBuffer Uniform buffer bound to descriptor binding 0.
			* @param range Byte range exposed through the uniform-buffer descriptor.
			* @return VK_SUCCESS after updating the descriptor set, otherwise VK_ERROR_INITIALIZATION_FAILED.
			*/
		[[nodiscard]] VkResult writeUniformBuffer(std::uint32_t frameIndex, VkBuffer uniformBuffer, VkDeviceSize range) {
			if (frameIndex >= descriptorSets.size() || uniformBuffer == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }
			const auto uniformBinding = std::ranges::find(descriptorBindings, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &VkDescriptorSetLayoutBinding::descriptorType);
			if (uniformBinding == descriptorBindings.end()) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkDescriptorBufferInfo bufferInfo{
				.buffer = uniformBuffer,
				.offset = 0U,
				.range = range,
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = uniformBinding->binding,
				.dstArrayElement = 0U,
				.descriptorCount = 1U,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &bufferInfo,
			};

			vkUpdateDescriptorSets(device, 1U, &write, 0U, nullptr);
			return VK_SUCCESS;
		}

		/**
			* @brief Writes one frame descriptor set with its binding-1 sampled shadow map.
			*
			* @param frameIndex Frame set index to update.
			* @param imageView Shadow-map image view bound to descriptor binding 1.
			* @param sampler Shadow-map sampler bound to descriptor binding 1.
			* @return VK_SUCCESS after updating the descriptor set, otherwise VK_ERROR_INITIALIZATION_FAILED.
			*/
		[[nodiscard]] VkResult writeShadowMap(std::uint32_t frameIndex, VkImageView imageView, VkSampler sampler) {
			if (frameIndex >= descriptorSets.size() || !shadowSamplerBindings[0U] || imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkDescriptorImageInfo imageInfo{
				.sampler = sampler,
				.imageView = imageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = *shadowSamplerBindings[0U],
				.dstArrayElement = 0U,
				.descriptorCount = 1U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &imageInfo,
			};

			vkUpdateDescriptorSets(device, 1U, &write, 0U, nullptr);
			return VK_SUCCESS;
		}

		/**
			* @brief Writes one frame descriptor set with its binding-4 sampled spot shadow map.
			*
			* @param frameIndex Frame set index to update.
			* @param imageView Spot shadow-map image view bound to descriptor binding 4.
			* @param sampler Spot shadow-map sampler bound to descriptor binding 4.
			* @return VK_SUCCESS after updating the descriptor set, otherwise VK_ERROR_INITIALIZATION_FAILED.
			*/
		[[nodiscard]] VkResult writeSpotShadowMap(std::uint32_t frameIndex, VkImageView imageView, VkSampler sampler) {
			if (frameIndex >= descriptorSets.size() || !shadowSamplerBindings[1U] || imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkDescriptorImageInfo imageInfo{
				.sampler = sampler,
				.imageView = imageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = *shadowSamplerBindings[1U],
				.dstArrayElement = 0U,
				.descriptorCount = 1U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &imageInfo,
			};

			vkUpdateDescriptorSets(device, 1U, &write, 0U, nullptr);
			return VK_SUCCESS;
		}

		/**
			* @brief Writes one frame descriptor set with its binding-5 sampled spot shadow-map array.
			*
			* @param frameIndex Frame set index to update.
			* @param imageView Spot shadow-map array image view bound to descriptor binding 5.
			* @param sampler Spot shadow-map array sampler bound to descriptor binding 5.
			* @return VK_SUCCESS after updating the descriptor set, otherwise VK_ERROR_INITIALIZATION_FAILED.
			*/
		[[nodiscard]] VkResult writeSpotShadowArray(std::uint32_t frameIndex, VkImageView imageView, VkSampler sampler) {
			if (frameIndex >= descriptorSets.size() || !shadowSamplerBindings[2U] || imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkDescriptorImageInfo imageInfo{
				.sampler = sampler,
				.imageView = imageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = *shadowSamplerBindings[2U],
				.dstArrayElement = 0U,
				.descriptorCount = 1U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &imageInfo,
			};

			vkUpdateDescriptorSets(device, 1U, &write, 0U, nullptr);
			return VK_SUCCESS;
		}

		/**
			* @brief Writes one frame descriptor set with its binding-6 sampled directional shadow-map array.
			*
			* @param frameIndex Frame set index to update.
			* @param imageView Directional shadow-map array image view bound to descriptor binding 6.
			* @param sampler Directional shadow-map array sampler bound to descriptor binding 6.
			* @return VK_SUCCESS after updating the descriptor set, otherwise VK_ERROR_INITIALIZATION_FAILED.
			*/
		[[nodiscard]] VkResult writeDirShadowArray(std::uint32_t frameIndex, VkImageView imageView, VkSampler sampler) {
			if (frameIndex >= descriptorSets.size() || !shadowSamplerBindings[3U] || imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkDescriptorImageInfo imageInfo{
				.sampler = sampler,
				.imageView = imageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = *shadowSamplerBindings[3U],
				.dstArrayElement = 0U,
				.descriptorCount = 1U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &imageInfo,
			};

			vkUpdateDescriptorSets(device, 1U, &write, 0U, nullptr);
			return VK_SUCCESS;
		}

		/**
			* @brief Writes one frame descriptor set with its binding-7 sampled point shadow-map array.
			*
			* @param frameIndex Frame set index to update.
			* @param imageView Point shadow-map array image view bound to descriptor binding 7.
			* @param sampler Point shadow-map array sampler bound to descriptor binding 7.
			* @return VK_SUCCESS after updating the descriptor set, otherwise VK_ERROR_INITIALIZATION_FAILED.
			*/
		[[nodiscard]] VkResult writePointShadowArray(std::uint32_t frameIndex, VkImageView imageView, VkSampler sampler) {
			if (frameIndex >= descriptorSets.size() || !shadowSamplerBindings[4U] || imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkDescriptorImageInfo imageInfo{
				.sampler = sampler,
				.imageView = imageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = *shadowSamplerBindings[4U],
				.dstArrayElement = 0U,
				.descriptorCount = 1U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &imageInfo,
			};

			vkUpdateDescriptorSets(device, 1U, &write, 0U, nullptr);
			return VK_SUCCESS;
		}

		/**
			* @brief Writes one frame descriptor set with its reserved binding-2 sampled object texture.
			*
			* @param frameIndex Frame set index to update.
			* @param texture Object base-color texture bound to the reflected Slang parameter binding.
			* @return VK_SUCCESS after updating the descriptor set, otherwise VK_ERROR_INITIALIZATION_FAILED.
		*/
		[[nodiscard]] VkResult writeObjectTexture(std::uint32_t frameIndex, const TextureImage &texture) {
			if (frameIndex >= descriptorSets.size() || !objectTextureBinding || texture.imageView == VK_NULL_HANDLE || texture.textureSampler == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkDescriptorImageInfo imageInfo{
				.sampler = texture.textureSampler,
				.imageView = texture.imageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = *objectTextureBinding,
				.dstArrayElement = 0U,
				.descriptorCount = 1U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &imageInfo,
			};

			vkUpdateDescriptorSets(device, 1U, &write, 0U, nullptr);
			return VK_SUCCESS;
		}

		/**
			* @brief Clears descriptor-set handles while leaving implicit pool-owned allocation lifetime intact.
			*/
		void cleanup() {
			descriptorSets.clear();
			descriptorBindings.clear();
			objectTextureBinding.reset();
			std::ranges::fill(shadowSamplerBindings, std::nullopt);
			device = VK_NULL_HANDLE;
			descriptorPool = VK_NULL_HANDLE;
		}

		/**
			* @brief Clears borrowed descriptor-set state on scope exit.
			*/
		~VulkanDescriptorSets() { cleanup(); }
	};

	/// @brief Minimal Vulkan mesh owner for one uploaded CPU mesh.
	struct VulkanMesh {
		VulkanBuffer vertexBuffer{};       ///< owned device-local-style host-visible vertex buffer.
		VulkanBuffer indexBuffer{};        ///< owned host-visible index buffer.
		std::uint32_t indexCount{0U};       ///< number of indices recorded for indexed draws.

		VulkanMesh() = default;
		VulkanMesh(const VulkanMesh &) = delete;
		VulkanMesh &operator=(const VulkanMesh &) = delete;

		/**
			* @brief Transfers uploaded mesh buffers and leaves the source empty.
			*/
		VulkanMesh(VulkanMesh &&other) noexcept
			: vertexBuffer{std::move(other.vertexBuffer)},
				indexBuffer{std::move(other.indexBuffer)},
				indexCount{std::exchange(other.indexCount, 0U)} {}

		/**
			* @brief Replaces this mesh with another uploaded mesh.
			*
			* @return This mesh after taking ownership from the source.
			*/
		VulkanMesh &operator=(VulkanMesh &&other) noexcept {
			if (this != &other) {
				cleanup();
				vertexBuffer = std::move(other.vertexBuffer);
				indexBuffer = std::move(other.indexBuffer);
				indexCount = std::exchange(other.indexCount, 0U);
			}
			return *this;
		}

		/**
			* @brief Creates vertex and index buffers and uploads one CPU mesh into host-visible memory.
			*
			* @param physicalDevice Physical device used to select host-visible memory types.
			* @param device Logical device that owns the created buffers.
			* @param mesh CPU mesh whose vertices and indices are uploaded.
			* @return VK_SUCCESS when both buffers are uploaded, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkPhysicalDevice physicalDevice, const VulkanOwnedHandle<vk::raii::Device, VkDevice> &device, const vve::simple::Mesh &mesh) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE || mesh.vertices.empty() || mesh.indices.empty()) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			const VkDeviceSize vertexSize = sizeof(vve::simple::Vertex) * mesh.vertices.size();
			const VkMemoryPropertyFlags hostVisibleMemory = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			VkResult result = vertexBuffer.create(
				physicalDevice,
				device,
				vertexSize,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				hostVisibleMemory
			);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = vertexBuffer.upload(mesh.vertices.data(), vertexSize);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			const VkDeviceSize indexSize = sizeof(std::uint32_t) * mesh.indices.size();
			result = indexBuffer.create(
				physicalDevice,
				device,
				indexSize,
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				hostVisibleMemory
			);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = indexBuffer.upload(mesh.indices.data(), indexSize);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			indexCount = static_cast<std::uint32_t>(mesh.indices.size());
			return VK_SUCCESS;
		}

		/**
		 * @brief Uploads replacement vertices when a mesh keeps the same allocation size and topology.
		 */
		[[nodiscard]] VkResult updateVertices(const vve::simple::Mesh &mesh) {
			const VkDeviceSize vertexSize = sizeof(vve::simple::Vertex) * mesh.vertices.size();
			if (mesh.vertices.empty() || vertexSize != vertexBuffer.size) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			return vertexBuffer.upload(mesh.vertices.data(), vertexSize);
		}

		/**
			* @brief Destroys the owned buffers and clears the draw count.
			*/
		void cleanup() {
			vertexBuffer.cleanup();
			indexBuffer.cleanup();
			indexCount = 0U;
		}

		/**
			* @brief Destroys the owned mesh buffers on scope exit.
			*/
		~VulkanMesh() { cleanup(); }
	};

	/// @brief Plain per-frame uniform data matching the Slang set 0 binding 0 block layout.
	struct FrameUniforms {
		Mat4 view{};        ///< shared camera view matrix
		Mat4 projection{};  ///< shared camera projection matrix
		Mat4 lightViewProj{}; ///< light view-projection for the upcoming shadow pass
		std::array<Mat4, kMaxDirectionalLights * kNumShadowCascades> dirLightViewProjArray{}; ///< flattened per-directional cascade matrices for depth passes and sampling
		Vec4 cascadeSplits{}; ///< view-space far distance of each directional shadow cascade
		std::array<Mat4, kMaxShadowedSpotLights> spotLightViewProjs{}; ///< spot-light view-projections for future shadow data
		std::array<Mat4, kMaxShadowedPointLights * 6U> pointLightFaceViewProjs{}; ///< per-point-face view-projections for future point shadows
		std::array<Vec4, kMaxShadowedPointLights> pointLightPositionRanges{}; ///< per-point xyz position with range in w
		std::array<Vec4, kMaxShadowedPointLights> pointLightColorIntensities{}; ///< per-point rgb color with intensity in w
		Vec4 lightPositionRange{};    ///< xyz world-space point-light position, w range
		Vec4 lightColorIntensity{};   ///< rgb direct-light color, w direct-light intensity
		Vec4 lightShadowAmbient{};    ///< xyz shadow-map direction approximation, w ambient term
		Vec4 dirLightDirection{};     ///< xyz world-space directional-light direction, w unused
		Vec4 dirLightColorIntensity{}; ///< rgb directional-light color, w directional-light intensity
		Vec4 dirLightShadowAmbient{}; ///< xyz directional shadow-map direction, w directional ambient term
		Vec4 spotLightPositionRange{}; ///< xyz world-space spot-light position, w range
		Vec4 spotLightColorIntensity{}; ///< rgb spot-light color, w spot-light intensity
		Vec4 spotLightDirection{};    ///< xyz spot-light light-to-scene direction, w unused
		Vec4 spotLightConeAmbient{};  ///< x inner cone cosine, y outer cone cosine, z active spot count, w ambient term
		std::array<Vec4, kMaxShadowedSpotLights> spotLightPositionRanges{}; ///< per-spot xyz position with range in w
		std::array<Vec4, kMaxShadowedSpotLights> spotLightColorIntensities{}; ///< per-spot rgb color with intensity in w
		std::array<Vec4, kMaxShadowedSpotLights> spotLightDirections{}; ///< per-spot xyz light-to-scene direction with unused w
		std::array<Vec4, kMaxShadowedSpotLights> spotLightConeAmbients{}; ///< per-spot inner cone, outer cone, active count, and ambient
		std::array<Vec4, kMaxDirectionalLights> directionalLightDirections{}; ///< per-directional xyz light-to-scene direction with unused w
		std::array<Vec4, kMaxDirectionalLights> directionalLightColorIntensities{}; ///< per-directional rgb color with intensity in w
		std::array<Vec4, kMaxDirectionalLights> directionalLightAmbients{}; ///< per-directional ambient term packed in w
		std::uint32_t activeDirectionalLightCount{}; ///< active directional-light count clamped to the fixed cap
		std::array<std::uint32_t, 3U> directionalLightPadding{}; ///< std140 padding after the scalar count
	};

	/// @brief Minimal per-frame uniform-buffer owner for shared view and projection data.
	struct VulkanUniformBuffers {
		std::vector<VulkanBuffer> buffers{}; ///< owned per-frame host-visible uniform buffers.

		VulkanUniformBuffers() = default;
		VulkanUniformBuffers(const VulkanUniformBuffers &) = delete;
		VulkanUniformBuffers &operator=(const VulkanUniformBuffers &) = delete;

		/**
			* @brief Creates one host-visible FrameUniforms buffer for each frame slot.
			*
			* @param physicalDevice Physical device used to select host-visible memory.
			* @param device Logical device that owns the uniform buffers.
			* @param framesInFlight Number of per-frame uniform buffers to create.
			* @return VK_SUCCESS when all buffers are created, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkPhysicalDevice physicalDevice, const VulkanOwnedHandle<vk::raii::Device, VkDevice> &device, std::uint32_t framesInFlight) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE || framesInFlight == 0U) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			auto createdBuffers = std::vector<VulkanBuffer>(framesInFlight);
			buffers.swap(createdBuffers);
			for (std::uint32_t frame{}; frame < framesInFlight; ++frame) {
				const VkResult result = buffers[frame].create(
					physicalDevice,
					device,
					sizeof(FrameUniforms),
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
				);
				if (result != VK_SUCCESS) { cleanup(); return result; }
			}
			return VK_SUCCESS;
		}

		/**
			* @brief Uploads shared frame matrices into one frame slot.
			*
			* @param frameIndex Frame slot whose uniform buffer receives the data.
			* @param uniforms Source view and projection matrices copied into the buffer.
			* @return VK_SUCCESS after upload, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult update(std::uint32_t frameIndex, const FrameUniforms &uniforms) {
			if (frameIndex >= buffers.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
			return buffers[frameIndex].upload(&uniforms, sizeof(FrameUniforms));
		}

		/**
			* @brief Clears the owned buffer list and lets each VulkanBuffer release its handles.
			*/
		void cleanup() {
			buffers.clear();
		}

		/**
			* @brief Clears the owned uniform buffers on scope exit.
			*/
		~VulkanUniformBuffers() { cleanup(); }
	};

} // namespace vve::simple
