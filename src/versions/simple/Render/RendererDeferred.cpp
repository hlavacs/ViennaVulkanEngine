module;
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>
#include "simple_gbuffer_frag_spv.h"
#include "simple_gbuffer_msaa_frag_spv.h"
#include "simple_gbuffer_gi_frag_spv.h"
#include "simple_gbuffer_gi_msaa_frag_spv.h"
#include "simple_lighting_vert_spv.h"
#include "simple_lighting_frag_spv.h"
#include "simple_lighting_msaa_frag_spv.h"
#include "simple_lighting_gi_frag_spv.h"
#include "simple_lighting_gi_msaa_frag_spv.h"

module VVEngine.Simple.Renderer;
import std;
import VVEngine.Simple.Types;
import VVEngine.Simple.Vulkan;

/// @file
/// @brief Deferred capability checks, pipelines and window-local sampled G-buffer descriptors. Scene ownership remains shared.
namespace vve::simple {
	namespace {
		/// @brief Lighting reads attributes and depth without filtering or a sampler; set 0 remains the shared scene set.
		auto deferredBindings(bool gi) {
			std::vector<VkDescriptorSetLayoutBinding> bindings(gi ? 11U : 4U);
			for (auto i : std::views::iota(0U, static_cast<unsigned>(bindings.size()))) {
				bindings[i] = {.binding = i, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
					.descriptorCount = 1U, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
			}
			if (gi) { bindings.back().descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; }
			return bindings;
		}
	}

	/// @brief Rejects unavailable attachment formats, coverage counts or sample shading without affecting the forward path.
	VkResult ForwardRenderer::deferredSupport(VkPhysicalDevice physical, VkSampleCountFlagBits samples, bool gi) {
		if (physical == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }
		VkPhysicalDeviceProperties properties{};
		VkPhysicalDeviceFeatures features{};
		vkGetPhysicalDeviceProperties(physical, &properties);
		vkGetPhysicalDeviceFeatures(physical, &features);
		if (properties.limits.maxColorAttachments < 3U || properties.limits.maxFragmentOutputAttachments < 3U ||
			properties.limits.maxPerStageDescriptorSampledImages < (gi ? 15U : 8U) ||
			(samples != VK_SAMPLE_COUNT_1_BIT && !features.sampleRateShading)) { return VK_ERROR_FEATURE_NOT_PRESENT; }
		// Check the complete usage combination; separate format feature bits do not guarantee multisample sampling.
		for (const auto format : std::array{gbufferFormats[0], depthFormat}) {
			const VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT |
				(format == depthFormat ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
			VkImageFormatProperties image{};
			const auto result = vkGetPhysicalDeviceImageFormatProperties(physical, format, VK_IMAGE_TYPE_2D,
				VK_IMAGE_TILING_OPTIMAL, usage, 0U, &image);
			if (result != VK_SUCCESS || (image.sampleCounts & samples) == 0U) { return VK_ERROR_FORMAT_NOT_SUPPORTED; }
		}
		if (gi) {
			VkImageFormatProperties image{};
			const auto ao = vkGetPhysicalDeviceImageFormatProperties(physical, VK_FORMAT_R16_SFLOAT, VK_IMAGE_TYPE_2D,
				VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &image);
			if (ao != VK_SUCCESS) { return ao; }
			const auto result = vkGetPhysicalDeviceImageFormatProperties(physical, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TYPE_2D,
				VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0, &image);
			if (result != VK_SUCCESS) { return result; }
		}
		return VK_SUCCESS;
	}

	/// @brief Checks the same surface/device selection used at startup before accepting a requested deferred backend.
	VkResult ForwardRenderer::probeDeferred(SDL_Window *window, bool gi) const {
		VulkanInstance probe{};
		auto result = probe.create();
		if (result != VK_SUCCESS) { return result; }
		VulkanSurface surface{};
		result = surface.create(probe.instance, window);
		if (result != VK_SUCCESS) { return result; }
		VulkanPhysicalDevice physical{};
		result = physical.select(probe.instance, surface.surface);
		return result == VK_SUCCESS ? deferredSupport(physical.physicalDevice, sampleCount(), gi) : result;
	}

	/// @brief Startup-only selection; an idempotent selection is safe even after frames have been submitted.
	auto ForwardRenderer::setRendererBackend(RendererBackend backend) -> std::expected<void, Error> {
		if (backend != RendererBackend::forward && backend != RendererBackend::deferred && backend != RendererBackend::hybrid_gi) {
			return std::unexpected(Error::invalid_argument);
		}
		if (backend == backend_) { return {}; }
		if (initialized()) { return std::unexpected(Error::already_initialized); }
		backend_ = backend;
		return {};
	}

	/// @brief Builds deferred-only pipelines using the shared material/lighting shaders and existing RAII pipeline owners.
	VkResult ForwardRenderer::createDeferredPipelines(std::uint32_t textureCount) {
		const auto bindings = deferredBindings(backend_ == RendererBackend::hybrid_gi);
		const VkDescriptorSetLayoutCreateInfo info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = static_cast<std::uint32_t>(bindings.size()), .pBindings = bindings.data()};
		VkDescriptorSetLayout raw{};
		auto result = vkCreateDescriptorSetLayout(device.device, &info, nullptr, &raw);
		result = deferredLayout.assign(device.device.handle, result, raw);
		if (result != VK_SUCCESS) { return result; }
		result = deferredPipelineLayout.create(device.device, descriptorSetLayout.descriptorSetLayout, deferredLayout);
		if (result != VK_SUCCESS) { return result; }
		const auto geometryCode = backend_ == RendererBackend::hybrid_gi
			? (sampleCount() == VK_SAMPLE_COUNT_1_BIT ? std::span<const std::uint32_t>{simple_gbuffer_gi_frag_spv} : std::span<const std::uint32_t>{simple_gbuffer_gi_msaa_frag_spv})
			: (sampleCount() == VK_SAMPLE_COUNT_1_BIT ? std::span<const std::uint32_t>{simple_gbuffer_frag_spv} : std::span<const std::uint32_t>{simple_gbuffer_msaa_frag_spv});
		result = gbufferShader.create(device.device, geometryCode);
		if (result != VK_SUCCESS) { return result; }
		result = lightingVertexShader.create(device.device, simple_lighting_vert_spv);
		if (result != VK_SUCCESS) { return result; }
		const bool gi = backend_ == RendererBackend::hybrid_gi;
		const auto lightingCode = gi ? (sampleCount() == VK_SAMPLE_COUNT_1_BIT ? std::span<const std::uint32_t>{simple_lighting_gi_frag_spv} : std::span<const std::uint32_t>{simple_lighting_gi_msaa_frag_spv})
			: (sampleCount() == VK_SAMPLE_COUNT_1_BIT ? std::span<const std::uint32_t>{simple_lighting_frag_spv} : std::span<const std::uint32_t>{simple_lighting_msaa_frag_spv});
		result = lightingShader.create(device.device, lightingCode);
		if (result != VK_SUCCESS) { return result; }
		const VulkanVertexInputDescription vertexInput{};
		const bool perSample = sampleCount() != VK_SAMPLE_COUNT_1_BIT;
		result = gbufferPipeline.create(device.device, pipelineLayout.pipelineLayout, vertShaderModule.shaderModule, "vertexMain",
			gbufferShader.shaderModule, vertexInput, gbufferFormats[0], depthFormat, textureCount, false, sampleCount(), gbufferFormats, perSample ? "gbufferMsaaMain" : "gbufferMain", false, perSample,
			VK_NULL_HANDLE, VK_NULL_HANDLE, "terrainDomainMain", gi);
		if (result != VK_SUCCESS) { return result; }
		if (device.tessellationShader) {
			result = terrainGbufferPipeline.create(device.device, pipelineLayout.pipelineLayout, terrainVertexShader.shaderModule, "terrainVertexMain",
				gbufferShader.shaderModule, vertexInput, gbufferFormats[0], depthFormat, textureCount, false, sampleCount(), gbufferFormats,
				perSample ? "gbufferMsaaMain" : "gbufferMain", false, perSample, terrainHullShader.shaderModule, terrainDomainShader.shaderModule, "terrainDomainMain");
			if (result != VK_SUCCESS) { return result; }
		}
		result = lightingPipeline.create(device.device, deferredPipelineLayout.pipelineLayout, lightingVertexShader.shaderModule, "lightingVertexMain",
			lightingShader.shaderModule, vertexInput, VK_FORMAT_R16G16B16A16_SFLOAT, depthFormat, textureCount, false, sampleCount(), {}, "lightingMain", true, perSample);
		if (result == VK_SUCCESS && gi) { result = createProbePipelines(textureCount); }
		return result == VK_SUCCESS && gi ? createAoPipelines(textureCount) : result;
	}

	/// @brief Replaces only idle window-local descriptors; both frame slots read the same attachments in queue order.
	VkResult ForwardRenderer::createDeferredDescriptors(WindowTarget &target) {
		target.deferredPool.cleanup();
		target.deferredSet = VK_NULL_HANDLE;
		const bool gi = backend_ == RendererBackend::hybrid_gi;
		if (gi) {
			const auto result = createProbeResources(target);
			if (result != VK_SUCCESS) { return result; }
			const auto ao = createAoResources(target);
			if (ao != VK_SUCCESS) { return ao; }
		}
		auto result = target.deferredPool.create(device.device, 1U, deferredBindings(gi));
		if (result != VK_SUCCESS) { return result; }
		if (gi && failGiAllocation()) { return VK_ERROR_OUT_OF_DEVICE_MEMORY; }
		const VkDescriptorSetLayout layout = deferredLayout;
		const VkDescriptorSetAllocateInfo allocate{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = target.deferredPool.descriptorPool, .descriptorSetCount = 1U, .pSetLayouts = &layout};
		result = vkAllocateDescriptorSets(device.device, &allocate, &target.deferredSet);
		if (result != VK_SUCCESS) { return result; }
		const std::array views{target.gbufferImages[0].imageView, target.gbufferImages[1].imageView,
			target.gbufferImages[2].imageView, target.depthImage.imageView, target.probes.coefficients.imageView, target.probes.radiance.imageView,
			target.aoImages[1].imageView, target.aoImages[0].imageView,
			target.coarseProbes.coefficients.imageView, target.coarseProbes.radiance.imageView, target.probeShadow.imageView};
		// Descriptor image layouts agree with the transitions between geometry and lighting.
		for (auto i : std::views::iota(0U, gi ? 11U : 4U)) {
			const VkDescriptorImageInfo image{.sampler = i == 10U ? target.probeShadow.shadowSampler : VK_NULL_HANDLE,
				.imageView = views[i], .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
			const VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = target.deferredSet,
				.dstBinding = i, .descriptorCount = 1U, .descriptorType = i == 10U ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .pImageInfo = &image};
			vkUpdateDescriptorSets(device.device, 1U, &write, 0U, nullptr);
		}
		return VK_SUCCESS;
	}
} // namespace vve::simple
