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
import :Pipeline;
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
			* @return VK_SUCCESS when the descriptor pool is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice, std::uint32_t maxSets) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			std::vector<VkDescriptorPoolSize> poolSizes{}; // One pool entry per descriptor type in kDescriptorSetBindings.
			for (const VkDescriptorSetLayoutBinding &binding : kDescriptorSetBindings) {
				auto poolSize = std::ranges::find(poolSizes, binding.descriptorType, &VkDescriptorPoolSize::type);
				if (poolSize == poolSizes.end()) { poolSize = poolSizes.insert(poolSize, VkDescriptorPoolSize{.type = binding.descriptorType}); }
				poolSize->descriptorCount += maxSets * binding.descriptorCount; // Array bindings need one descriptor per element.
			}

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
			* @param count Number of per-frame descriptor sets to allocate.
			* @return VK_SUCCESS when all descriptor sets are allocated, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkDevice owningDevice, VkDescriptorPool pool, VkDescriptorSetLayout setLayout, std::uint32_t count) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE || pool == VK_NULL_HANDLE || setLayout == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }
			device = owningDevice;
			descriptorPool = pool;
			auto layouts = std::vector<VkDescriptorSetLayout>(count, setLayout);
			const VkDescriptorSetAllocateInfo allocateInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = pool,
				.descriptorSetCount = count,
				.pSetLayouts = layouts.data(),
			};

			descriptorSets.resize(count);
			const VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, descriptorSets.data());
			if (result != VK_SUCCESS) { cleanup(); }
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

			const VkDescriptorBufferInfo bufferInfo{
				.buffer = uniformBuffer,
				.offset = 0U,
				.range = range,
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = shaderBinding::frameUniforms,
				.dstArrayElement = 0U,
				.descriptorCount = 1U,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &bufferInfo,
			};

			vkUpdateDescriptorSets(device, 1U, &write, 0U, nullptr);
			return VK_SUCCESS;
		}

		/**
			* @brief Writes one shadow-array sampler of one frame descriptor set.
			*
			* @param frameIndex Frame set index to update.
			* @param binding shaderBinding::spotShadowArray, dirShadowArray, or pointShadowArray.
			* @param imageView Whole-array depth view.
			* @param sampler Comparison sampler.
			* @return VK_SUCCESS after updating the descriptor set, otherwise VK_ERROR_INITIALIZATION_FAILED.
			*/
		[[nodiscard]] VkResult writeShadowArray(std::uint32_t frameIndex, std::uint32_t binding, VkImageView imageView, VkSampler sampler) {
			if (frameIndex >= descriptorSets.size() || imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }
			const VkDescriptorImageInfo imageInfo{.sampler = sampler, .imageView = imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = binding,
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
			if (frameIndex >= descriptorSets.size() || images.size() != kMaxSceneTextures) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (std::ranges::any_of(images, [](const VkDescriptorImageInfo &image) { return image.imageView == VK_NULL_HANDLE || image.sampler == VK_NULL_HANDLE; })) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = shaderBinding::baseColorTextures,
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

	/// @brief Frame constants for set 0 binding 0; the member order and types mirror FrameUniforms in simple_forward.slang (std140).
	struct FrameUniforms {
		Mat4 view{};                                                                    ///< Camera view matrix.
		Mat4 projection{};                                                              ///< Camera projection matrix.
		std::array<Mat4, kShadowMatrixCount> shadowViewProjs{};                         ///< Spot, point-face, and directional-cascade light matrices (see kShadowMatrix*Base).
		Vec4 cascadeSplits{};                                                           ///< View-space far distance of each directional cascade.
		std::array<Vec4, kMaxShadowedPointLights> pointLightPositionRanges{};           ///< Point xyz position with range in w.
		std::array<Vec4, kMaxShadowedPointLights> pointLightColorIntensities{};         ///< Point rgb color with intensity in w.
		std::array<Vec4, kMaxShadowedSpotLights> spotLightPositionRanges{};             ///< Spot xyz position with range in w.
		std::array<Vec4, kMaxShadowedSpotLights> spotLightColorIntensities{};           ///< Spot rgb color with intensity in w.
		std::array<Vec4, kMaxShadowedSpotLights> spotLightDirections{};                 ///< Spot xyz direction with unused w.
		std::array<Vec4, kMaxShadowedSpotLights> spotLightConeAmbients{};               ///< Spot inner cone cosine, outer cone cosine, unused, ambient.
		std::array<Vec4, kMaxDirectionalLights> directionalLightDirections{};           ///< Directional xyz direction with unused w.
		std::array<Vec4, kMaxDirectionalLights> directionalLightColorIntensities{};     ///< Directional rgb color with intensity in w.
		std::array<Vec4, kMaxDirectionalLights> directionalLightAmbients{};             ///< Directional ambient term in w.
		std::uint32_t activeDirectionalLightCount{};                                    ///< Packed directional-light count.
		std::uint32_t activeSpotLightCount{};                                           ///< Packed spot-light count.
		float ambient{};                                                                ///< Scene-wide ambient term.
		std::uint32_t padding{};                                                        ///< std140 padding to 16 bytes.
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
