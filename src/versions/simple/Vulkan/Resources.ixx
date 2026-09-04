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
#include <vk_mem_alloc.h>
#ifdef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif

export module VEEngine.Simple.Vulkan:Resources;
import :Device;
import :Commands;
import :Memory;
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
	* - TextureImage is a sampled RGBA8 VulkanImage with a sampler and a staging upload.
	* - VulkanDescriptorPool owns only VkDescriptorPool creation and teardown for uniform-buffer, shadow-map, and object-texture descriptor sets.
	* - VulkanDescriptorSets allocates per-frame uniform-buffer, shadow-map, and object-texture descriptor sets from a borrowed pool and layout.
	* - VulkanMesh owns the vertex and index buffers and index count for one uploaded CPU mesh.
	* - FrameUniforms stores frame matrices plus GPU-packed point, directional, and spot light data for set 0 binding 0.
	* - VulkanUniformBuffers owns one mapped FrameUniforms buffer per frame.
	*/
export namespace vve::simple {

	/// @brief Sampled RGBA8 texture: the VulkanImage base owns image and view, this adds the sampler and the staging upload.
	struct TextureImage : VulkanImage {
		VkSampler textureSampler{VK_NULL_HANDLE}; ///< Owned linear repeat sampler for texture reads.

		~TextureImage() { cleanup(); }

		/// @brief Loads an 8-bit RGBA file with stb_image and uploads it.
		[[nodiscard]] VkResult create(VmaAllocator allocator, VkDevice owningDevice, VkQueue graphicsQueue, VkCommandPool commandPool, const std::filesystem::path &imagePath) {
			int width{};
			int height{};
			int channels{};
			const auto pathString = imagePath.string(); // stb_image requires a null-terminated filesystem path.
			auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>{stbi_load(pathString.c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free};
			if (!pixels || width <= 0 || height <= 0) { return VK_ERROR_INITIALIZATION_FAILED; }
			const auto textureExtent = VkExtent2D{.width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height)};
			const auto byteCount = static_cast<std::size_t>(textureExtent.width) * textureExtent.height * 4U;
			return create(allocator, owningDevice, graphicsQueue, commandPool, std::span{reinterpret_cast<const std::byte *>(pixels.get()), byteCount}, textureExtent);
		}

		/// @brief Uploads tight RGBA8 bytes through a staging buffer into a sampled SRGB image.
		[[nodiscard]] VkResult create(VmaAllocator allocator, VkDevice owningDevice, VkQueue graphicsQueue, VkCommandPool commandPool, std::span<const std::byte> rgbaPixels, VkExtent2D textureExtent) {
			cleanup();
			const VkDeviceSize byteCount = static_cast<VkDeviceSize>(textureExtent.width) * textureExtent.height * 4U;
			if (rgbaPixels.size() != byteCount || byteCount == 0U) { return VK_ERROR_INITIALIZATION_FAILED; }

			VulkanBuffer staging{};
			VkResult result = staging.create(allocator, byteCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);
			if (result != VK_SUCCESS) { return result; }
			result = staging.upload(rgbaPixels.data(), byteCount);
			if (result != VK_SUCCESS) { return result; }

			result = VulkanImage::create(allocator, owningDevice, textureExtent, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			if (result != VK_SUCCESS) { return result; }

			result = submitOnce(owningDevice, graphicsQueue, commandPool, [&](VkCommandBuffer commandBuffer) {
				transitionImage(commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT, 0U, 1U, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
				const VkBufferImageCopy region{
					.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0U, .baseArrayLayer = 0U, .layerCount = 1U},
					.imageExtent = {.width = extent.width, .height = extent.height, .depth = 1U},
				};
				vkCmdCopyBufferToImage(commandBuffer, staging.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U, &region);
				transitionImage(commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT, 0U, 1U, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			});
			if (result != VK_SUCCESS) { cleanup(); return result; }

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
			result = vkCreateSampler(owningDevice, &samplerInfo, nullptr, &textureSampler);
			if (result != VK_SUCCESS) { cleanup(); }
			return result;
		}

		/// @brief Destroys the sampler, then the view and image.
		void cleanup() {
			if (textureSampler != VK_NULL_HANDLE) { vkDestroySampler(device, textureSampler, nullptr); }
			textureSampler = VK_NULL_HANDLE;
			VulkanImage::cleanup();
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
				poolSize->descriptorCount += maxSets * binding.descriptorCount; // Array bindings need one descriptor per element.
			}
			if (poolSizes.size() != 2U) { return VK_ERROR_INITIALIZATION_FAILED; }
			const auto uniformPoolSize = std::ranges::find(poolSizes, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &VkDescriptorPoolSize::type);
			const auto samplerPoolSize = std::ranges::find(poolSizes, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &VkDescriptorPoolSize::type);
			if (uniformPoolSize == poolSizes.end() || samplerPoolSize == poolSizes.end() || uniformPoolSize->descriptorCount != maxSets) { return VK_ERROR_INITIALIZATION_FAILED; }

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
		std::optional<std::uint32_t> objectTextureBinding{};      ///< Reflected set-0 binding for the object base-color texture array.
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
			* @brief Writes the whole object base-color texture array of one frame descriptor set.
			*
			* @param frameIndex Frame set index to update.
			* @param images One valid sampler/view pair per array element, in slot order.
			* @return VK_SUCCESS after updating the descriptor set, otherwise VK_ERROR_INITIALIZATION_FAILED.
		*/
		[[nodiscard]] VkResult writeObjectTextures(std::uint32_t frameIndex, std::span<const VkDescriptorImageInfo> images) {
			if (frameIndex >= descriptorSets.size() || !objectTextureBinding || images.empty()) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (std::ranges::any_of(images, [](const VkDescriptorImageInfo &image) { return image.imageView == VK_NULL_HANDLE || image.sampler == VK_NULL_HANDLE; })) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = *objectTextureBinding,
				.dstArrayElement = 0U,
				.descriptorCount = static_cast<std::uint32_t>(images.size()),
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = images.data(),
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

	/// @brief Host-visible vertex and index buffers for one uploaded CPU mesh.
	struct VulkanMesh {
		VulkanBuffer vertexBuffer{};        ///< Owned mapped vertex buffer.
		VulkanBuffer indexBuffer{};         ///< Owned mapped index buffer.
		std::uint32_t indexCount{0U};       ///< Number of indices recorded for indexed draws.

		VulkanMesh() = default;
		VulkanMesh(const VulkanMesh &) = delete;
		VulkanMesh &operator=(const VulkanMesh &) = delete;
		VulkanMesh(VulkanMesh &&) noexcept = default;
		VulkanMesh &operator=(VulkanMesh &&) noexcept = default;

		/// @brief Creates both buffers and copies the mesh into them.
		[[nodiscard]] VkResult create(VmaAllocator allocator, const vve::simple::Mesh &mesh) {
			cleanup();
			if (mesh.vertices.empty() || mesh.indices.empty()) { return VK_ERROR_INITIALIZATION_FAILED; }
			const VkDeviceSize vertexSize = sizeof(vve::simple::Vertex) * mesh.vertices.size();
			const VkDeviceSize indexSize = sizeof(std::uint32_t) * mesh.indices.size();
			VkResult result = vertexBuffer.create(allocator, vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true);
			if (result == VK_SUCCESS) { result = vertexBuffer.upload(mesh.vertices.data(), vertexSize); }
			if (result == VK_SUCCESS) { result = indexBuffer.create(allocator, indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, true); }
			if (result == VK_SUCCESS) { result = indexBuffer.upload(mesh.indices.data(), indexSize); }
			if (result != VK_SUCCESS) { cleanup(); return result; }
			indexCount = static_cast<std::uint32_t>(mesh.indices.size());
			return VK_SUCCESS;
		}

		/// @brief Uploads replacement vertices when a mesh keeps the same allocation size and topology.
		[[nodiscard]] VkResult updateVertices(const vve::simple::Mesh &mesh) {
			const VkDeviceSize vertexSize = sizeof(vve::simple::Vertex) * mesh.vertices.size();
			if (mesh.vertices.empty() || vertexSize != vertexBuffer.size) { return VK_ERROR_INITIALIZATION_FAILED; }
			return vertexBuffer.upload(mesh.vertices.data(), vertexSize);
		}

		void cleanup() {
			vertexBuffer.cleanup();
			indexBuffer.cleanup();
			indexCount = 0U;
		}
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

	/// @brief One mapped FrameUniforms buffer per frame in flight.
	struct VulkanUniformBuffers {
		std::vector<VulkanBuffer> buffers{}; ///< Owned per-frame host-visible uniform buffers.

		/// @brief Creates one FrameUniforms-sized buffer for each frame slot.
		[[nodiscard]] VkResult create(VmaAllocator allocator, std::uint32_t framesInFlight) {
			cleanup();
			if (framesInFlight == 0U) { return VK_ERROR_INITIALIZATION_FAILED; }
			buffers.resize(framesInFlight);
			for (VulkanBuffer &buffer : buffers) {
				const VkResult result = buffer.create(allocator, sizeof(FrameUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true);
				if (result != VK_SUCCESS) { cleanup(); return result; }
			}
			return VK_SUCCESS;
		}

		/// @brief Copies the frame data into one frame slot.
		[[nodiscard]] VkResult update(std::uint32_t frameIndex, const FrameUniforms &uniforms) {
			if (frameIndex >= buffers.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
			return buffers[frameIndex].upload(&uniforms, sizeof(FrameUniforms));
		}

		void cleanup() { buffers.clear(); }
	};

} // namespace vve::simple
