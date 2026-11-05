module;
#include <compare>
#include <vulkan/vulkan_raii.hpp>
#include <VVPPL.h>
#include <imgui.h>
#include "simple_tonemap_vert_spv.h"
#include "simple_tonemap_frag_spv.h"
#include "simple_gui_output_vert_spv.h"
#include "simple_gui_output_frag_spv.h"

module VVEngine.Simple.Renderer;
import std;
import VVEngine.Simple.Types;
import VVEngine.Simple.Vulkan;

/** @file @brief On-demand Neutral output resources, fenced retirement and explicit HDR-to-display recording.
 * One graphics queue orders shared intermediate reads before subsequent writes, including frames in flight.
 * Descriptors are immutable; disabling moves their complete owner bundle into submission-serial retirement.
 */
namespace vve::simple {
	/// @brief Test-only retired output snapshot, enabled by debugOutputReadback; no normal-frame allocation or lifetime change.
	auto ForwardRenderer::retiredOutputImages() const -> std::vector<std::pair<std::uint64_t, std::array<VkImage, 4>>> {
		if (!debugOutputReadback) { return {}; }
		std::vector<std::pair<std::uint64_t, std::array<VkImage, 4>>> result;
		// Inspect owners without retaining them or affecting the fence-driven retirement queue.
		for (const auto &[serial, resource] : retiredResources_) {
			if (const auto *target = std::get_if<std::unique_ptr<WindowTarget>>(&resource); target && *target) {
				const auto &t = **target;
				const std::array<VkImage, 4> images{t.postImage.image, t.ldrImage.image, t.smaaImages[0].image, t.smaaImages[1].image};
				if (std::ranges::any_of(images, [](VkImage image) { return image != VK_NULL_HANDLE; })) {
					result.emplace_back(serial, images);
				}
			}
		}
		return result;
	}

	/// @brief Count actual optional Vulkan objects in live targets and fence-retired bundles, excluding scene HDR storage.
	auto ForwardRenderer::toneMapResourceCounts() const -> std::array<std::size_t, 2> {
		const auto count = [](const WindowTarget &t) -> std::size_t {
			std::size_t smaa = (t.ldrImage.image != VK_NULL_HANDLE) + t.ldrUnormView.valid() + t.smaaLayout.valid() +
				t.smaaPipelineLayout.valid() + t.smaaPool.descriptorPool.valid() + (t.smaaSet != VK_NULL_HANDLE);
			for (const auto &image : t.smaaImages) { smaa += image.image != VK_NULL_HANDLE; }
			for (const auto &pipeline : t.smaaPipelines) { smaa += pipeline.pipeline.valid(); }
			return smaa + (t.postImage.image != VK_NULL_HANDLE) + t.toneMapLayout.valid() + t.toneMapPipelineLayout.valid() +
				t.toneMapPipeline.pipeline.valid() + t.toneMapPool.descriptorPool.valid() + (t.toneMapSet != VK_NULL_HANDLE) +
				t.toneMapGuiPipeline.pipeline.valid() + t.toneMapGuiLayout.valid();
		};
		std::array<std::size_t, 2> result{};
		for (const auto &image : smaaLookup) { result[0] += image.image != VK_NULL_HANDLE; }
		for (const auto &sampler : smaaSamplers) { result[0] += sampler.valid(); }
		for (const auto &target : targets) { result[0] += count(target); }
		for (const auto &[serial, resource] : retiredResources_) {
			if (const auto *target = std::get_if<std::unique_ptr<WindowTarget>>(&resource); target && *target) { result[1] += count(**target); }
		}
		return result;
	}

	/// @brief Copy output inside its ordinary submission, before presentation; no host wait serializes setting changes.
	VkResult ForwardRenderer::recordOutputDiagnostic(WindowTarget &target, VkCommandBuffer command, std::uint32_t imageIndex, bool attachment) {
		auto &frame = outputDiagnostics.emplace_back();
		frame.window = target.handle; frame.extent = target.swapchain.extent; frame.format = target.swapchain.imageFormat;
		frame.settings = toneMapping_; frame.passes = recordedPassOrder;
		frame.encoded = target.lastOutputEncoded; frame.preserveAlpha = target.lastOutputPreservesAlpha;
		const auto result = frame.pixels.create(allocator, VkDeviceSize(frame.extent.width) * frame.extent.height * 4,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT, BufferMemory::readback);
		if (result != VK_SUCCESS) { outputDiagnostics.pop_back(); return result; }
		const VkImageMemoryBarrier begin{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = static_cast<VkAccessFlags>(attachment ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_TRANSFER_WRITE_BIT),
			.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.oldLayout = attachment ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = target.swapchain.images[imageIndex],
			.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
		vkCmdPipelineBarrier(command, attachment ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &begin);
		const VkBufferImageCopy copy{.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
			.imageExtent = {frame.extent.width, frame.extent.height, 1}};
		vkCmdCopyImageToBuffer(command, begin.image, begin.newLayout, frame.pixels.buffer, 1, &copy);
		// Coherent mapped storage becomes readable only after this submission's fence is observed.
		const VkMemoryBarrier host{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
		auto restore = begin;
		restore.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT; restore.dstAccessMask = begin.srcAccessMask;
		restore.oldLayout = begin.newLayout; restore.newLayout = begin.oldLayout;
		vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_HOST_BIT | (attachment ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT),
			0, 1, &host, 0, nullptr, 1, &restore);
		return VK_SUCCESS;
	}
	/// @brief Validate settings and every live output before committing; capability failure preserves the selection.
	auto ForwardRenderer::setToneMapping(ToneMapping settings) -> std::expected<void, Error> {
		if ((settings.op != ToneMappingOperator::none && settings.op != ToneMappingOperator::khronos_neutral) ||
			!std::isfinite(settings.exposure) || settings.exposure <= 0) { return std::unexpected(Error::invalid_argument); }
		if (settings.op != ToneMappingOperator::none) {
			for (const auto &target : targets) {
				if (target.swapchain.swapchain != VK_NULL_HANDLE && toneMapSupport(target) != VK_SUCCESS) {
					return std::unexpected(Error::platform_error);
				}
			}
		}
		toneMapping_ = settings;
		if (settings.op == ToneMappingOperator::none && !smaaEnabled()) {
			for (auto &target : targets) { retireToneMap(target); }
		}
		return {};
	}

	/// @brief Opt-in supports only RGBA/BGRA sRGB or UNORM with SRGB_NONLINEAR and usable attachment/sample formats.
	VkResult ForwardRenderer::toneMapSupport(const WindowTarget &target) const {
		const auto format = target.swapchain.imageFormat;
		const bool supported = format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB ||
			format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_R8G8B8A8_UNORM;
		if (!supported || target.swapchain.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { return VK_ERROR_FORMAT_NOT_SUPPORTED; }
		VkFormatProperties features{};
		vkGetPhysicalDeviceFormatProperties(physicalDevice.physicalDevice, format, &features);
		if (!(features.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) { return VK_ERROR_FORMAT_NOT_SUPPORTED; }
		VkImageFormatProperties image{};
		const auto usage = target.postProcess ? VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
			: VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		const auto result = vkGetPhysicalDeviceImageFormatProperties(physicalDevice.physicalDevice, VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &image);
		if (result != VK_SUCCESS) { return result; }
		vkGetPhysicalDeviceFormatProperties(physicalDevice.physicalDevice, VK_FORMAT_R16G16B16A16_SFLOAT, &features);
		return (features.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) &&
			(!target.postProcess || (features.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
			? VK_SUCCESS : VK_ERROR_FORMAT_NOT_SUPPORTED;
	}

	/// @brief Move complete optional owners together; no resource can die before its last submitted use completes.
	void ForwardRenderer::retireToneMap(WindowTarget &target) {
		if (!target.toneMapLayout.valid() && target.postImage.image == VK_NULL_HANDLE) { return; }
		auto retired = std::make_unique<WindowTarget>();
		retired->postImage = std::move(target.postImage);
		retired->toneMapPipeline.pipeline = std::move(target.toneMapPipeline.pipeline);
		retired->toneMapGuiPipeline.pipeline = std::move(target.toneMapGuiPipeline.pipeline);
		retired->toneMapGuiLayout = std::move(target.toneMapGuiLayout);
		retired->toneMapPool = std::move(target.toneMapPool);
		retired->toneMapPipelineLayout = std::move(target.toneMapPipelineLayout);
		retired->toneMapLayout = std::move(target.toneMapLayout);
		retired->toneMapSet = target.toneMapSet; // Borrowed handle remains counted with the pool that owns it.
		target.toneMapSet = VK_NULL_HANDLE;
		retire(std::move(retired));
	}

	/// @brief Prepare before acquisition/submission; none returns without creating any optional Vulkan object.
	VkResult ForwardRenderer::prepareToneMap(WindowTarget &target) {
		if (toneMapping_.op == ToneMappingOperator::none && !smaaEnabled()) { return VK_SUCCESS; }
		auto result = toneMapSupport(target);
		if (result != VK_SUCCESS || target.toneMapSet != VK_NULL_HANDLE) { return result; }
		// A failed partial creation is never submitted and is discarded before another attempt.
		retireToneMap(target);
		if (!target.hdrSampled && !target.postProcess) {
			// Runtime enable replaces HDR without waiting for its previous frames; only the next recorded frame uses the new image.
			VulkanImage sampled;
			result = sampled.create(allocator, device.device, target.swapchain.extent, VK_FORMAT_R16G16B16A16_SFLOAT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			if (result != VK_SUCCESS) { return result; }
			auto retired = std::make_unique<WindowTarget>();
			retired->hdrImage = std::move(target.hdrImage);
			retire(std::move(retired));
			target.hdrImage = std::move(sampled); target.hdrSampled = true;
		}
		if (target.postProcess) {
			result = target.postImage.create(allocator, device.device, target.swapchain.extent, VK_FORMAT_R16G16B16A16_SFLOAT,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			if (result != VK_SUCCESS) { return result; }
		}
		const std::array bindings{VkDescriptorSetLayoutBinding{.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT}};
		const VkDescriptorSetLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = 1, .pBindings = bindings.data()};
		VkDescriptorSetLayout rawLayout{};
		result = vkCreateDescriptorSetLayout(device.device, &layoutInfo, nullptr, &rawLayout);
		result = target.toneMapLayout.assign(device.device.handle, result, rawLayout);
		if (result != VK_SUCCESS) { return result; }
		const VkPushConstantRange push{.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .size = 12};
		const VkPipelineLayoutCreateInfo pipelineInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 1, .pSetLayouts = &rawLayout, .pushConstantRangeCount = 1, .pPushConstantRanges = &push};
		VkPipelineLayout rawPipelineLayout{};
		result = vkCreatePipelineLayout(device.device, &pipelineInfo, nullptr, &rawPipelineLayout);
		result = target.toneMapPipelineLayout.assign(device.device.handle, result, rawPipelineLayout);
		if (result != VK_SUCCESS) { return result; }
		VulkanShaderModule vertex{}, fragment{};
		result = vertex.create(device.device, simple_tonemap_vert_spv);
		if (result == VK_SUCCESS) { result = fragment.create(device.device, simple_tonemap_frag_spv); }
		if (result == VK_SUCCESS) {
			result = target.toneMapPipeline.create(device.device, rawPipelineLayout, vertex.shaderModule, "toneMapVertex",
				fragment.shaderModule, {}, smaaEnabled() ? VK_FORMAT_R8G8B8A8_SRGB : target.swapchain.imageFormat,
				depthFormat, 1, false, VK_SAMPLE_COUNT_1_BIT, {}, "toneMapFragment", true);
		}
		if (result != VK_SUCCESS) { return result; }
		result = target.toneMapPool.create(device.device, 1, bindings);
		if (result != VK_SUCCESS) { return result; }
		const VkDescriptorSetAllocateInfo allocate{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = target.toneMapPool.descriptorPool, .descriptorSetCount = 1, .pSetLayouts = &rawLayout};
		result = vkAllocateDescriptorSets(device.device, &allocate, &target.toneMapSet);
		if (result != VK_SUCCESS) { return result; }
		const VkDescriptorImageInfo image{.imageView = target.postProcess ? target.postImage.imageView : target.hdrImage.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL};
		const VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = target.toneMapSet,
			.descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .pImageInfo = &image};
		vkUpdateDescriptorSets(device.device, 1, &write, 0, nullptr);
		result = createToneMapGui(target);
		if (result != VK_SUCCESS) { retireToneMap(target); }
		return result;
	}

	/// @brief Only sRGB without mutable GUI views needs an alternate ImGui-compatible pipeline; default stays untouched.
	VkResult ForwardRenderer::createToneMapGui(WindowTarget &target) {
		const auto format = target.swapchain.guiFormat;
		if (!target.guiWindow || (format != VK_FORMAT_B8G8R8A8_SRGB && format != VK_FORMAT_R8G8B8A8_SRGB)) { return VK_SUCCESS; }
		const VkDescriptorSetLayoutBinding binding{.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
		const VkDescriptorSetLayoutCreateInfo setInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = 1, .pBindings = &binding};
		VulkanOwnedHandle<vk::raii::DescriptorSetLayout, VkDescriptorSetLayout> set;
		VkDescriptorSetLayout rawSet{};
		auto result = vkCreateDescriptorSetLayout(device.device, &setInfo, nullptr, &rawSet);
		result = set.assign(device.device.handle, result, rawSet);
		if (result != VK_SUCCESS) { return result; }
		const VkPushConstantRange push{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = 16};
		const VkPipelineLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 1, .pSetLayouts = &rawSet, .pushConstantRangeCount = 1, .pPushConstantRanges = &push};
		VkPipelineLayout rawLayout{};
		result = vkCreatePipelineLayout(device.device, &layoutInfo, nullptr, &rawLayout);
		result = target.toneMapGuiLayout.assign(device.device.handle, result, rawLayout);
		if (result != VK_SUCCESS) { return result; }
		VulkanShaderModule vertex{}, fragment{};
		result = vertex.create(device.device, simple_gui_output_vert_spv);
		if (result == VK_SUCCESS) { result = fragment.create(device.device, simple_gui_output_frag_spv); }
		if (result != VK_SUCCESS) { return result; }
		const std::array stages{
			VkPipelineShaderStageCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vertex.shaderModule, .pName = "guiOutputVertex"},
			VkPipelineShaderStageCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fragment.shaderModule, .pName = "guiOutputFragment"}};
		// Exact backend ABI: ImDrawVert and four vertex push floats; existing texture sets remain compatible.
		const VkVertexInputBindingDescription input{0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX};
		const std::array attributes{VkVertexInputAttributeDescription{0,0,VK_FORMAT_R32G32_SFLOAT,offsetof(ImDrawVert,pos)},
			VkVertexInputAttributeDescription{1,0,VK_FORMAT_R32G32_SFLOAT,offsetof(ImDrawVert,uv)},
			VkVertexInputAttributeDescription{2,0,VK_FORMAT_R8G8B8A8_UNORM,offsetof(ImDrawVert,col)}};
		const VkPipelineVertexInputStateCreateInfo vertices{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &input,
			.vertexAttributeDescriptionCount = 3, .pVertexAttributeDescriptions = attributes.data()};
		const VkPipelineInputAssemblyStateCreateInfo assembly{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
		const VkPipelineViewportStateCreateInfo viewport{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
		const VkPipelineRasterizationStateCreateInfo raster{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1};
		const VkPipelineMultisampleStateCreateInfo samples{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
		const VkPipelineColorBlendAttachmentState blend{.blendEnable = VK_TRUE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA, .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD, .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.alphaBlendOp = VK_BLEND_OP_ADD, .colorWriteMask = 15};
		const VkPipelineColorBlendStateCreateInfo blending{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1, .pAttachments = &blend};
		const std::array dynamic{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
		const VkPipelineDynamicStateCreateInfo dynamics{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = 2, .pDynamicStates = dynamic.data()};
		const VkPipelineRenderingCreateInfo rendering{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1, .pColorAttachmentFormats = &format};
		const VkGraphicsPipelineCreateInfo info{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &rendering,
			.stageCount = 2, .pStages = stages.data(), .pVertexInputState = &vertices, .pInputAssemblyState = &assembly,
			.pViewportState = &viewport, .pRasterizationState = &raster, .pMultisampleState = &samples,
			.pColorBlendState = &blending, .pDynamicState = &dynamics, .layout = rawLayout};
		VkPipeline raw{};
		result = vkCreateGraphicsPipelines(device.device, VK_NULL_HANDLE, 1, &info, nullptr, &raw);
		return target.toneMapGuiPipeline.pipeline.assign(device.device.handle, result, raw);
	}

	/// @brief Record optional vvppl -> Neutral -> swapchain; the legacy branch remains in RendererDraw.cpp.
	void ForwardRenderer::recordToneMap(WindowTarget &target, VkCommandBuffer command, std::uint32_t imageIndex) {
		constexpr VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1};
		VkImageMemoryBarrier hdr{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = static_cast<VkAccessFlags>(target.postProcess ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_SHADER_READ_BIT),
			.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .newLayout = VK_IMAGE_LAYOUT_GENERAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = target.hdrImage.image, .subresourceRange = range};
		vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			target.postProcess ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &hdr);
		if (target.postProcess) {
			// Previous frame's sampling must finish before the shared destination is overwritten in queue order.
			auto post = hdr;
			post.image = target.postImage.image; post.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			post.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; post.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &post);
			recordedPassOrder.push_back(RecordedPass::post_process);
			target.postProcess->apply(command, target.hdrImage.image, target.postImage.image, target.currentFrame);
			post.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; post.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			post.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &post);
		}
		const bool neutral = toneMapping_.op != ToneMappingOperator::none;
		// Measure Neutral including its output transition/store; the SMAA identity preparation is not a tone mapper.
		if (neutral) { writeGpuTimestamp(target, command, 12); }
		// COLOR_ATTACHMENT_OUTPUT matches this path's acquire wait, even with an earlier offscreen vvppl blit.
		auto output = hdr;
		output.image = smaaEnabled() ? target.ldrImage.image : target.swapchain.images[imageIndex];
		output.srcAccessMask = smaaEnabled() ? VK_ACCESS_SHADER_READ_BIT : 0;
		output.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; output.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		output.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		vkCmdPipelineBarrier(command, smaaEnabled() ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			0, 0, nullptr, 0, nullptr, 1, &output);
		const VkRenderingAttachmentInfo attachment{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = smaaEnabled() ? target.ldrImage.imageView : static_cast<VkImageView>(target.imageViews.ownedViews[imageIndex]),
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
		const VkRenderingInfo rendering{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = {.extent = target.swapchain.extent}, .layerCount = 1, .colorAttachmentCount = 1, .pColorAttachments = &attachment};
		const VkViewport viewport{0, 0, float(target.swapchain.extent.width), float(target.swapchain.extent.height), 0, 1};
		const VkRect2D scissor{.extent = target.swapchain.extent};
		recordedPassOrder.push_back(neutral ? RecordedPass::tone_mapping : RecordedPass::output_identity);
		debugLabels.begin(command, neutral ? "Khronos PBR Neutral" : "SMAA clamped identity", DebugLabelKind::post);
		vkCmdBeginRendering(command, &rendering);
		vkCmdSetViewport(command, 0, 1, &viewport); vkCmdSetScissor(command, 0, 1, &scissor);
		vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, target.toneMapPipeline.pipeline);
		vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, target.toneMapPipelineLayout, 0, 1, &target.toneMapSet, 0, nullptr);
		const std::array<std::uint32_t, 3> push{std::bit_cast<std::uint32_t>(float(toneMapping_.exposure)),
			unsigned(!smaaEnabled() && (target.swapchain.imageFormat == VK_FORMAT_B8G8R8A8_UNORM || target.swapchain.imageFormat == VK_FORMAT_R8G8B8A8_UNORM)), unsigned(neutral)};
		vkCmdPushConstants(command, target.toneMapPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), push.data());
		vkCmdDraw(command, 3, 1, 0, 0);
		vkCmdEndRendering(command);
		debugLabels.end(command);
		if (neutral) { writeGpuTimestamp(target, command, 13); }
		if (smaaEnabled()) { recordSmaa(target, command, imageIndex); }
	}
} // namespace vve::simple
