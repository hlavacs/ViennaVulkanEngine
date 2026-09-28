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
import VEEngine.Simple.Types;
import VEEngine.Simple.Scene;

/**
	* @file
	* @brief Vulkan GPU resource ownership for textures, meshes, frame uniforms, descriptor pools, and descriptor sets.
	*
	* Functional objects:
	* - TextureImage records RGBA8 uploads and mip generation into a VulkanUploadBatch; the renderer owns its sampler.
	* - VulkanDescriptorPool owns only VkDescriptorPool creation and teardown for uniform-buffer, shadow-map, and object-texture descriptor sets.
	* - VulkanDescriptorSets allocates per-frame uniform-buffer, shadow-map, and object-texture descriptor sets from a borrowed pool and layout.
	* - VulkanMesh owns the vertex and index buffers and index count for one uploaded CPU mesh.
	* - FrameUniforms stores frame matrices plus GPU-packed point, directional, and spot light data for set 0 binding 0.
	* - VulkanUniformBuffers owns one mapped FrameUniforms buffer per frame.
	*/
export namespace vve::simple {

	/// @brief Chooses the complete mip chain when optimal images support linear blits, otherwise only the base level.
	[[nodiscard]] constexpr std::uint32_t textureMipLevels(VkExtent2D extent, VkFormatFeatureFlags features) {
		constexpr auto required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
		return (features & required) == required ? std::bit_width(std::max(extent.width, extent.height)) : 1U;
	}

	/// @brief Sampled RGBA8 image with mip views; staging memory belongs to the caller's upload batch.
	struct TextureImage : VulkanImage {
		[[nodiscard]] VkResult create(VmaAllocator allocator, VkDevice owningDevice, VkPhysicalDevice physicalDevice,
			VulkanUploadBatch &batch, std::span<const std::byte> rgbaPixels, VkExtent2D textureExtent, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
	};

	/// @brief Records the base upload and successive linear blits; the image must survive until the batch completes.
	inline VkResult TextureImage::create(VmaAllocator allocator, VkDevice owningDevice, VkPhysicalDevice physicalDevice,
		VulkanUploadBatch &batch, std::span<const std::byte> rgbaPixels, VkExtent2D textureExtent, VkFormat format) {
		cleanup();
		const VkDeviceSize byteCount = static_cast<VkDeviceSize>(textureExtent.width) * textureExtent.height * 4U;
		if (rgbaPixels.size() != byteCount || byteCount == 0U || physicalDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }
		const auto staging = batch.stage(allocator, rgbaPixels);
		if (!staging) { return staging.error(); }
		VkFormatProperties properties{};
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
		const auto levels = textureMipLevels(textureExtent, properties.optimalTilingFeatures);
		const VkResult result = VulkanImage::create(allocator, owningDevice, textureExtent, format,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT, 1U, false, false, levels);
		if (result != VK_SUCCESS) { return result; }

		const auto commandBuffer = batch.commandBuffer;
		transitionImage(commandBuffer, image, aspect, 0U, 1U, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0U, mipLevels);
		const VkBufferImageCopy region{
			.imageSubresource = {.aspectMask = aspect, .mipLevel = 0U, .baseArrayLayer = 0U, .layerCount = 1U},
			.imageExtent = {.width = extent.width, .height = extent.height, .depth = 1U},
		};
		vkCmdCopyBufferToImage(commandBuffer, *staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U, &region);
		auto width = static_cast<std::int32_t>(extent.width), height = static_cast<std::int32_t>(extent.height);
		// Each completed level becomes the next blit's source, then remains shader-readable for its lifetime.
		for (std::uint32_t level{1U}; level < mipLevels; ++level) {
			const auto nextWidth = std::max(1, width / 2), nextHeight = std::max(1, height / 2);
			transitionImage(commandBuffer, image, aspect, 0U, 1U, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, level - 1U);
			const VkImageBlit blit{
				.srcSubresource = {.aspectMask = aspect, .mipLevel = level - 1U, .baseArrayLayer = 0U, .layerCount = 1U},
				.srcOffsets = {{0, 0, 0}, {width, height, 1}},
				.dstSubresource = {.aspectMask = aspect, .mipLevel = level, .baseArrayLayer = 0U, .layerCount = 1U},
				.dstOffsets = {{0, 0, 0}, {nextWidth, nextHeight, 1}},
			};
			vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U, &blit, VK_FILTER_LINEAR);
			transitionImage(commandBuffer, image, aspect, 0U, 1U, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, level - 1U);
			width = nextWidth;
			height = nextHeight;
		}
		transitionImage(commandBuffer, image, aspect, 0U, 1U, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels - 1U);
		return VK_SUCCESS;
	}


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
				// Use an iterator search to avoid a Clang 20 crash with ranges projections across modules.
				auto poolSize = poolSizes.begin();
				while (poolSize != poolSizes.end() && poolSize->type != binding.descriptorType) { ++poolSize; }
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
			* @brief Writes one frame descriptor set with the shared material storage buffer.
			*
			* @param frameIndex Frame set index to update.
			* @param materialBuffer Storage buffer containing dense GpuMaterial entries.
			* @param range Byte range exposed through the storage-buffer descriptor.
			* @return VK_SUCCESS after updating the descriptor set, otherwise VK_ERROR_INITIALIZATION_FAILED.
			*/
		[[nodiscard]] VkResult writeMaterialBuffer(std::uint32_t frameIndex, VkBuffer materialBuffer, VkDeviceSize range) {
			if (frameIndex >= descriptorSets.size() || materialBuffer == VK_NULL_HANDLE || range == 0U) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			const VkDescriptorBufferInfo bufferInfo{.buffer = materialBuffer, .offset = 0U, .range = range};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = shaderBinding::materials,
				.dstArrayElement = 0U,
				.descriptorCount = 1U,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
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
			* @brief Writes changed object texture elements of one completed frame descriptor set.
			*
			* @param frameIndex Frame set index to update.
			* @param images Valid sampler/view pairs for the consecutive elements being changed.
			* @param firstElement First texture slot to replace.
			* @return VK_SUCCESS after updating the descriptor set, otherwise VK_ERROR_INITIALIZATION_FAILED.
		*/
		[[nodiscard]] VkResult writeObjectTextures(std::uint32_t frameIndex, std::span<const VkDescriptorImageInfo> images, std::uint32_t firstElement = 0U) {
			if (frameIndex >= descriptorSets.size() || images.empty() || firstElement + images.size() > kMaxSceneTextures) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (std::ranges::any_of(images, [](const VkDescriptorImageInfo &image) { return image.imageView == VK_NULL_HANDLE || image.sampler == VK_NULL_HANDLE; })) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = shaderBinding::baseColorTextures,
				.dstArrayElement = firstElement,
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
		}

		/**
			* @brief Clears borrowed descriptor-set state on scope exit.
			*/
		~VulkanDescriptorSets() { cleanup(); }
	};

	/// @brief Device-local static vertex and index buffers for one uploaded CPU mesh.
	struct VulkanMesh {
		VulkanBuffer vertexBuffer{};        ///< Owned device-local static vertices; retired on the first edit.
		VulkanBuffer indexBuffer{};         ///< Owned device-local indices, shared by static and dynamic draws.
		std::uint32_t indexCount{0U};       ///< Number of indices recorded for indexed draws.
		Bounds localBox{};                   ///< Cached object-space bounds; invalid bounds are never culled.

		VulkanMesh() = default;
		VulkanMesh(const VulkanMesh &) = delete;
		VulkanMesh &operator=(const VulkanMesh &) = delete;
		VulkanMesh(VulkanMesh &&) noexcept = default;
		VulkanMesh &operator=(VulkanMesh &&) noexcept = default;

		/// @brief Stages both static buffers in one submission, retaining staging memory until its fence completes.
		template <typename TMesh>
		[[nodiscard]] VkResult create(VmaAllocator allocator, VkDevice device, VkQueue queue, VkCommandPool pool, const TMesh &mesh) {
			cleanup();
			if (mesh.vertices->empty() || mesh.indices.empty()) { return VK_ERROR_INITIALIZATION_FAILED; }
			const VkDeviceSize vertexSize = sizeof(mesh.vertices->front()) * mesh.vertices->size();
			const VkDeviceSize indexSize = sizeof(std::uint32_t) * mesh.indices.size();
			VulkanBuffer vertexStaging{}, indexStaging{};
			VkResult result = vertexStaging.create(allocator, vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BufferMemory::upload);
			if (result == VK_SUCCESS) { result = uploadValues(vertexStaging, *mesh.vertices); }
			if (result == VK_SUCCESS) { result = indexStaging.create(allocator, indexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BufferMemory::upload); }
			if (result == VK_SUCCESS) { result = uploadValues(indexStaging, mesh.indices); }
			if (result == VK_SUCCESS) { result = vertexBuffer.create(allocator, vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, BufferMemory::device_local); }
			if (result == VK_SUCCESS) { result = indexBuffer.create(allocator, indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, BufferMemory::device_local); }
			if (result == VK_SUCCESS) {
				result = submitOnce(device, queue, pool, [&](VkCommandBuffer commandBuffer) {
					const VkBufferCopy vertexCopy{.size = vertexSize}, indexCopy{.size = indexSize};
					vkCmdCopyBuffer(commandBuffer, vertexStaging.buffer, vertexBuffer.buffer, 1U, &vertexCopy);
					vkCmdCopyBuffer(commandBuffer, indexStaging.buffer, indexBuffer.buffer, 1U, &indexCopy);
					// Make both transfer writes visible to vertex and index reads in subsequent draws.
					const VkMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
						.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT};
					vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
						0U, 1U, &barrier, 0U, nullptr, 0U, nullptr);
				});
			}
			if (result != VK_SUCCESS) { cleanup(); return result; }
			indexCount = static_cast<std::uint32_t>(mesh.indices.size());
			localBox = vertexBounds(mesh);
			return VK_SUCCESS;
		}

		/// @brief Uploads replacement vertices and refreshes culling bounds without changing allocation size or topology.
		template <typename TMesh>
		[[nodiscard]] VkResult updateVertices(VulkanBuffer &buffer, const TMesh &mesh) {
			if (mesh.vertices->empty()) { return VK_ERROR_INITIALIZATION_FAILED; }
			const VkDeviceSize vertexSize = sizeof(mesh.vertices->front()) * mesh.vertices->size();
			if (vertexSize != buffer.size) { return VK_ERROR_INITIALIZATION_FAILED; }
			const VkResult result = uploadValues(buffer, *mesh.vertices);
			if (result == VK_SUCCESS) { localBox = vertexBounds(mesh); }
			return result;
		}

		void cleanup() {
			vertexBuffer.cleanup();
			indexBuffer.cleanup();
			indexCount = 0U;
			localBox = {};
		}

	private:
		/// @brief Computes the GPU mesh's culling box from uploaded vertices; unknown positions disable culling.
		template <typename TMesh>
		[[nodiscard]] static Bounds vertexBounds(const TMesh &mesh) {
			Bounds result{};
			// One calculation serves primitive, imported and edited meshes without a duplicate CPU box.
			for (const auto &vertex : *mesh.vertices) {
				const auto &position = vertex.position;
				if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) { return {}; }
				result.minimum.value = result.valid ? min(result.minimum.value, position) : position;
				result.maximum.value = result.valid ? max(result.maximum.value, position) : position;
				result.valid = true;
			}
			return result;
		}

		/// @brief Copies contiguous values into a mapped GPU buffer in one transfer.
		template <typename TValues>
		[[nodiscard]] static VkResult uploadValues(VulkanBuffer &buffer, const TValues &values) {
			using Value = std::remove_cvref_t<decltype(values.front())>;
			const auto byteCount = sizeof(Value) * values.size();
			if (buffer.mapped == nullptr || byteCount > buffer.size) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (byteCount != 0U) { std::memcpy(buffer.mapped, values.data(), byteCount); }
			return VK_SUCCESS;
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
		std::array<Vec4, kMaxShadowedSpotLights> spotLightDirections{};                 ///< Spot xyz direction; w = 1 skips shadows for ambient-only lights.
		std::array<Vec4, kMaxShadowedSpotLights> spotLightConeAmbients{};               ///< Spot inner cone cosine, outer cone cosine, unused, ambient.
		std::array<Vec4, kMaxDirectionalLights> directionalLightDirections{};           ///< Directional xyz direction; w = 1 skips shadows for ambient-only lights.
		std::array<Vec4, kMaxDirectionalLights> directionalLightColorIntensities{};     ///< Directional rgb color with intensity in w.
		std::array<Vec4, kMaxDirectionalLights> directionalLightAmbients{};             ///< Directional ambient term in w.
		std::uint32_t activeDirectionalLightCount{};                                    ///< Packed directional-light count.
		std::uint32_t activeSpotLightCount{};                                           ///< Packed spot-light count.
		float ambient{};                                                                ///< Scene-wide ambient term.
		std::uint32_t activePointLightCount{};                                          ///< Packed point-light count.
	};
	/// Matrices are 64 bytes and vectors 16 bytes in the shader; a double Scalar (VVE_MATH_USE_DOUBLE) would break the std140 mirror.
	static_assert(sizeof(FrameUniforms) == (2U + kShadowMatrixCount) * 64U +
		(1U + 2U * kMaxShadowedPointLights + 4U * kMaxShadowedSpotLights + 3U * kMaxDirectionalLights + 1U) * 16U);

	/// @brief One mapped FrameUniforms buffer per frame in flight.
	struct VulkanUniformBuffers {
		std::vector<VulkanBuffer> buffers{}; ///< Owned per-frame host-visible uniform buffers.

		/// @brief Creates one FrameUniforms-sized buffer for each frame slot.
		[[nodiscard]] VkResult create(VmaAllocator allocator, std::uint32_t framesInFlight) {
			cleanup();
			if (framesInFlight == 0U) { return VK_ERROR_INITIALIZATION_FAILED; }
			buffers.resize(framesInFlight);
			for (VulkanBuffer &buffer : buffers) {
				const VkResult result = buffer.create(allocator, sizeof(FrameUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, BufferMemory::upload);
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
