module;
#include <compare>
#include <vulkan/vulkan_raii.hpp>
#include "../../../../third_party/SMAA/AreaTex.h"
#include "../../../../third_party/SMAA/SearchTex.h"
#include "simple_tonemap_vert_spv.h"
#include "simple_smaa_edges_spv.h"
#include "simple_smaa_weights_spv.h"
#include "simple_smaa_blend_spv.h"

module VVEngine.Simple.Renderer;
import std;
import VVEngine.Simple.Types;
import VVEngine.Simple.Vulkan;

/** @file @brief Optional SMAA HIGH resources and three explicit spatial passes.
 * One graphics queue orders shared per-window reads before subsequent attachment overwrites.
 * Immutable descriptors and all referenced window images retire together at their last submission serial.
 */
namespace vve::simple {
	namespace {
		constexpr VkImageSubresourceRange colorRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}; ///< Whole single-level colour images.
		constexpr auto attachmentUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; ///< Every intermediate is written then sampled.
		constexpr std::array dataFormats{VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8A8_UNORM}; ///< Edges and weights are never sRGB.

		/// @brief Publish an intermediate write or order its next overwrite without discarding synchronization.
		void barrier(VkCommandBuffer command, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
			VkAccessFlags source, VkAccessFlags destination, VkPipelineStageFlags from, VkPipelineStageFlags to) {
			const VkImageMemoryBarrier info{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = source, .dstAccessMask = destination, .oldLayout = oldLayout, .newLayout = newLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = image, .subresourceRange = colorRange};
			vkCmdPipelineBarrier(command, from, to, 0, 0, nullptr, 0, nullptr, 1, &info);
		}
	}

	/// @brief Upload the reference tables once per device, using a fenced upload batch and linear/point clamp samplers.
	VkResult ForwardRenderer::createSmaaLookup() {
		if (smaaLookup[1].image != VK_NULL_HANDLE) { return VK_SUCCESS; }
		std::array<VulkanImage, 2> tables;
		std::array<VulkanOwnedHandle<vk::raii::Sampler, VkSampler>, 2> samplers;
		VulkanUploadBatch upload;
		auto result = upload.begin(device.device, device.graphicsQueue, commandPool.commandPool);
		if (result != VK_SUCCESS) { return result; }
		const std::array<std::span<const std::byte>, 2> bytes{std::as_bytes(std::span{areaTexBytes}), std::as_bytes(std::span{searchTexBytes})};
		constexpr std::array extents{VkExtent2D{AREATEX_WIDTH, AREATEX_HEIGHT}, VkExtent2D{SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT}};
		constexpr std::array formats{VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8_UNORM};
		// Each table keeps the upstream packed rows and channel count, with no mip generation or colour conversion.
		// AreaTex.h's DX10 RG8 rows already match top-left Vulkan UVs. SearchTex alone is pre-flipped by its generator;
		// SMAASearchLength supplies its negative Y scale. CPU analytic checks reject an extra upload flip or subsample row.
		for (auto i : std::views::iota(0U, 2U)) {
			const auto staging = upload.stage(allocator, bytes[i]);
			if (!staging) { return staging.error(); }
			result = tables[i].create(allocator, device.device, extents[i], formats[i],
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			if (result != VK_SUCCESS) { return result; }
			transitionImage(upload.commandBuffer, tables[i].image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			const VkBufferImageCopy copy{.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
				.imageExtent = {extents[i].width, extents[i].height, 1}};
			vkCmdCopyBufferToImage(upload.commandBuffer, *staging, tables[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
			transitionImage(upload.commandBuffer, tables[i].image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			const auto filter = i == 0 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
			const VkSamplerCreateInfo sampler{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter = filter, .minFilter = filter, .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
				.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
			VkSampler raw{};
			result = vkCreateSampler(device.device, &sampler, nullptr, &raw);
			result = samplers[i].assign(device.device.handle, result, raw);
			if (result != VK_SUCCESS) { return result; }
		}
		result = upload.submit();
		if (result == VK_SUCCESS) { smaaLookup = std::move(tables); smaaSamplers = std::move(samplers); }
		return result;
	}

	/// @brief Retire size-dependent SMAA owners together; lookup textures remain resident until renderer shutdown.
	void ForwardRenderer::retireSmaa(WindowTarget &target) {
		if (target.ldrImage.image == VK_NULL_HANDLE) { return; }
		auto retired = std::make_unique<WindowTarget>();
		retired->ldrImage = std::move(target.ldrImage); retired->ldrUnormView = std::move(target.ldrUnormView);
		retired->smaaImages = std::move(target.smaaImages); retired->smaaReadback = std::move(target.smaaReadback);
		for (auto i : std::views::iota(0U, 3U)) { retired->smaaPipelines[i].pipeline = std::move(target.smaaPipelines[i].pipeline); }
		retired->smaaLayout = std::move(target.smaaLayout); retired->smaaPipelineLayout = std::move(target.smaaPipelineLayout);
		retired->smaaPool = std::move(target.smaaPool); retired->smaaSet = std::exchange(target.smaaSet, VK_NULL_HANDLE);
		retire(std::move(retired));
	}

	/// @brief Check every format/usage before acquisition, then create optional per-window immutable resources.
	VkResult ForwardRenderer::prepareSmaa(WindowTarget &target) {
		if (!smaaEnabled()) { return VK_SUCCESS; }
		auto result = toneMapSupport(target);
		if (result != VK_SUCCESS) { return result; }
		if (target.smaaSet == VK_NULL_HANDLE) {
			// Mutable RGBA8 views share the Vulkan format compatibility class; query both views' actual feature needs.
			for (auto format : {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8_UNORM}) {
				VkFormatProperties features{};
				vkGetPhysicalDeviceFormatProperties(physicalDevice.physicalDevice, format, &features);
				const auto usage = format == VK_FORMAT_R8_UNORM ? VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
					: attachmentUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				const auto needed = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
					VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
					(format == VK_FORMAT_R8_UNORM ? 0U : VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
				VkImageFormatProperties image{};
				result = vkGetPhysicalDeviceImageFormatProperties(physicalDevice.physicalDevice, format, VK_IMAGE_TYPE_2D,
					VK_IMAGE_TILING_OPTIMAL, usage, format == VK_FORMAT_R8G8B8A8_SRGB ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0U, &image);
				if (result != VK_SUCCESS || (features.optimalTilingFeatures & needed) != needed) { return VK_ERROR_FORMAT_NOT_SUPPORTED; }
			}
			retireSmaa(target); // Discard only an unsubmitted partial setup before retrying.
			result = createSmaaLookup();
			if (result != VK_SUCCESS) { return result; }
			result = target.ldrImage.create(allocator, device.device, target.swapchain.extent, VK_FORMAT_R8G8B8A8_SRGB,
				attachmentUsage, VK_IMAGE_ASPECT_COLOR_BIT, 1, false, false, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT);
			if (result != VK_SUCCESS) { return result; }
			const VkImageViewCreateInfo view{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = target.ldrImage.image,
				.viewType = VK_IMAGE_VIEW_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM, .subresourceRange = colorRange};
			VkImageView rawView{};
			result = vkCreateImageView(device.device, &view, nullptr, &rawView);
			result = target.ldrUnormView.assign(device.device.handle, result, rawView);
			if (result != VK_SUCCESS) { return result; }
			for (auto i : std::views::iota(0U, 2U)) {
				result = target.smaaImages[i].create(allocator, device.device, target.swapchain.extent, dataFormats[i],
					attachmentUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
				if (result != VK_SUCCESS) { return result; }
			}
			std::array<VkDescriptorSetLayoutBinding, 8> bindings{};
			for (auto i : std::views::iota(0U, 8U)) {
				bindings[i] = {i, i < 6 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
			}
			const VkDescriptorSetLayoutCreateInfo setInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.bindingCount = unsigned(bindings.size()), .pBindings = bindings.data()};
			VkDescriptorSetLayout rawLayout{};
			result = vkCreateDescriptorSetLayout(device.device, &setInfo, nullptr, &rawLayout);
			result = target.smaaLayout.assign(device.device.handle, result, rawLayout);
			if (result != VK_SUCCESS) { return result; }
			const VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, 20};
			const VkPipelineLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				.setLayoutCount = 1, .pSetLayouts = &rawLayout, .pushConstantRangeCount = 1, .pPushConstantRanges = &push};
			VkPipelineLayout rawPipelineLayout{};
			result = vkCreatePipelineLayout(device.device, &layoutInfo, nullptr, &rawPipelineLayout);
			result = target.smaaPipelineLayout.assign(device.device.handle, result, rawPipelineLayout);
			if (result != VK_SUCCESS) { return result; }
			VulkanShaderModule vertex;
			result = vertex.create(device.device, simple_tonemap_vert_spv);
			if (result != VK_SUCCESS) { return result; }
			const std::array code{std::span<const std::uint32_t>{simple_smaa_edges_spv},
				std::span<const std::uint32_t>{simple_smaa_weights_spv}, std::span<const std::uint32_t>{simple_smaa_blend_spv}};
			constexpr std::array entries{"smaaEdges", "smaaWeights", "smaaBlend"};
			for (auto i : std::views::iota(0U, 3U)) {
				VulkanShaderModule fragment;
				result = fragment.create(device.device, code[i]);
				if (result != VK_SUCCESS) { return result; }
				result = target.smaaPipelines[i].create(device.device, rawPipelineLayout, vertex.shaderModule, "toneMapVertex",
					fragment.shaderModule, {}, i < 2 ? dataFormats[i] : target.swapchain.imageFormat,
					depthFormat, 1, false, VK_SAMPLE_COUNT_1_BIT, {}, entries[i], true);
				if (result != VK_SUCCESS) { return result; }
			}
			result = target.smaaPool.create(device.device, 1, bindings);
			if (result != VK_SUCCESS) { return result; }
			VkDescriptorSet set{};
			const VkDescriptorSetAllocateInfo allocate{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = target.smaaPool.descriptorPool, .descriptorSetCount = 1, .pSetLayouts = &rawLayout};
			result = vkAllocateDescriptorSets(device.device, &allocate, &set);
			if (result != VK_SUCCESS) { return result; }
			const std::array<VkImageView, 6> views{rawView, target.ldrImage.imageView, target.smaaImages[0].imageView,
				smaaLookup[0].imageView, smaaLookup[1].imageView, target.smaaImages[1].imageView};
			std::array<VkDescriptorImageInfo, 8> images{};
			std::array<VkWriteDescriptorSet, 8> writes{};
			for (auto i : std::views::iota(0U, 8U)) {
				images[i] = {.sampler = i < 6 ? VK_NULL_HANDLE : static_cast<VkSampler>(smaaSamplers[i - 6]),
					.imageView = i < 6 ? views[i] : VK_NULL_HANDLE, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
				writes[i] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set, .dstBinding = i,
					.descriptorCount = 1, .descriptorType = bindings[i].descriptorType, .pImageInfo = &images[i]};
			}
			vkUpdateDescriptorSets(device.device, unsigned(writes.size()), writes.data(), 0, nullptr);
			target.smaaSet = set; // Publish completion only after all setup succeeds.
		}
		// Intermediate diagnostics have no cost in normal rendering and survive until the test observes completion.
		if (debugSmaaReadback) {
			for (auto i : std::views::iota(0U, 2U)) {
				if (target.smaaReadback[i].buffer != VK_NULL_HANDLE) { continue; }
				result = target.smaaReadback[i].create(allocator, VkDeviceSize(target.swapchain.extent.width) * target.swapchain.extent.height * (i == 0 ? 2 : 4),
					VK_BUFFER_USAGE_TRANSFER_DST_BIT, BufferMemory::readback);
				if (result != VK_SUCCESS) { return result; }
			}
		}
		return VK_SUCCESS;
	}

	/// @brief Clear edges/weights, run the pinned reference passes, and finish with one display encoding before GUI.
	void ForwardRenderer::recordSmaa(WindowTarget &target, VkCommandBuffer command, std::uint32_t imageIndex) {
		writeGpuTimestamp(target, command, 14); // Include all three passes and their attachment dependencies.
		barrier(command, target.ldrImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		const auto extent = target.swapchain.extent;
		const VkViewport viewport{0, 0, float(extent.width), float(extent.height), 0, 1};
		const VkRect2D scissor{.extent = extent};
		const std::array<float, 5> push{1.0F / extent.width, 1.0F / extent.height, float(extent.width), float(extent.height),
			std::bit_cast<float>(unsigned(target.swapchain.imageFormat == VK_FORMAT_R8G8B8A8_UNORM || target.swapchain.imageFormat == VK_FORMAT_B8G8R8A8_UNORM))};
		constexpr std::array passes{RecordedPass::smaa_edges, RecordedPass::smaa_weights, RecordedPass::smaa_blend};
		constexpr std::array labels{"SMAA edges", "SMAA weights", "SMAA neighbourhood"};
		for (auto i : std::views::iota(0U, 3U)) {
			const auto image = i < 2 ? target.smaaImages[i].image : target.swapchain.images[imageIndex];
			// Include diagnostic transfer reads in the inter-frame ordering; UNDEFINED discards contents, never dependencies.
			barrier(command, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				i < 2 ? VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT : 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				i < 2 ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
			const VkRenderingAttachmentInfo attachment{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = i < 2 ? target.smaaImages[i].imageView : static_cast<VkImageView>(target.imageViews.ownedViews[imageIndex]),
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .loadOp = i < 2 ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE, .clearValue = {.color = {{0, 0, 0, 0}}}};
			const VkRenderingInfo rendering{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.renderArea = {.extent = extent}, .layerCount = 1, .colorAttachmentCount = 1, .pColorAttachments = &attachment};
			recordedPassOrder.push_back(passes[i]);
			debugLabels.begin(command, labels[i], DebugLabelKind::post);
			vkCmdBeginRendering(command, &rendering);
			vkCmdSetViewport(command, 0, 1, &viewport); vkCmdSetScissor(command, 0, 1, &scissor);
			vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, target.smaaPipelines[i].pipeline);
			vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, target.smaaPipelineLayout, 0, 1, &target.smaaSet, 0, nullptr);
			vkCmdPushConstants(command, target.smaaPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), push.data());
			vkCmdDraw(command, 3, 1, 0, 0);
			vkCmdEndRendering(command);
			debugLabels.end(command);
			if (i < 2) {
				barrier(command, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			}
		}
		writeGpuTimestamp(target, command, 15); // Exclude test-only intermediate copies from the SMAA span.
		if (debugSmaaReadback) {
			// Serialize only test readback writes; ordinary submissions and SMAA attachments retain their normal queue overlap.
			const VkMemoryBarrier before{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT};
			vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &before, 0, nullptr, 0, nullptr);
			for (auto i : std::views::iota(0U, 2U)) {
				const auto image = target.smaaImages[i].image;
				barrier(command, image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
				const VkBufferImageCopy copy{.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
					.imageExtent = {extent.width, extent.height, 1}};
				vkCmdCopyImageToBuffer(command, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target.smaaReadback[i].buffer, 1, &copy);
				barrier(command, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			}
			const VkMemoryBarrier host{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
			vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, nullptr, 0, nullptr);
		}
	}
} // namespace vve::simple
