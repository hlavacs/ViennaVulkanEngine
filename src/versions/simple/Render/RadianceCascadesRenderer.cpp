module;
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>
#include "../shaders/simple_shared.h"
// Build-generated uint32_t arrays are defined only in this translation unit.
#include "simple_rc_primary_raygen_spv.h"
#include "simple_rc_primary_miss_spv.h"
#include "simple_rc_primary_closest_hit_spv.h"

module VVEngine.Simple.RadianceCascadesRenderer;
import std;
import VVEngine.Simple.Types;
import VVEngine.Simple.Vulkan;
import VVEngine.Simple.RenderResources;

/// @file
/// @brief RadianceCascadesRenderer lifecycle, scene upload, frame loop and PNG capture.
///
/// Functional objects:
/// - init / cleanup: own Vulkan instance, ray tracing device, allocator, swapchain and frame synchronization.
/// - createPipeline: descriptor set, ray tracing pipeline and shader binding table of the primary rays.
/// - voxelize / triangleTouchesBox: CPU voxelization of the traced instances.
/// - rebuildScene / buildScene / buildStructure / uploadVoxels: voxel volumes, geometry upload, one bottom-level
///   structure per mesh, one top-level structure over the instances, and the material buffer.
/// - renderFrame / drawFrame / recordFrame / recordResolve: acquire, record, submit and present one frame.
/// - captureFrameToPng: renders one more frame and copies its swapchain image into a PNG.
///
/// The ray tracing commands are extension functions the Vulkan loader does not export; they are called through the
/// dispatch table of the device's vk::raii owner.

namespace vve::simple {

	namespace {
		constexpr std::uint64_t frameFenceTimeoutNs{1'000'000'000ULL}; ///< A frame slot busy for longer skips the frame.
		constexpr std::uint64_t acquireTimeoutNs{100'000'000ULL};      ///< No swapchain image within this time skips the frame.
		constexpr VkFormat outputFormat{VK_FORMAT_R16G16B16A16_SFLOAT}; ///< Linear HDR; the blit to an sRGB swapchain encodes it.

		/// @brief Push constants of every stage; the layout matches RcFrame in simple_radiance_cascades.slang.
		struct FrameConstants {
			Vec4 origin{};     ///< Camera position.
			Vec4 right{};      ///< Camera right, scaled by tan(fov / 2) * aspect.
			Vec4 up{};         ///< Camera up, scaled by tan(fov / 2).
			Vec4 forward{};    ///< Normalized view direction.
			Vec4 background{}; ///< Linear colour of rays that hit nothing.
			Vec4 volume{};     ///< Voxel volume origin (xyz) and voxel size (w).
			Vec4 grid{};       ///< Voxels per axis (xyz) and the RadianceCascadesView (w).
		};
		constexpr VkShaderStageFlags constantStages{VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR};

		/// @brief Conservative triangle-box overlap by the separating axis test (Akenine-Moeller); touching counts as overlap.
		[[nodiscard]] bool triangleTouchesBox(std::array<Vec3, 3> triangle, Vec3 centre, Vec3 half) {
			for (auto &corner : triangle) { corner = subtract(corner, centre); }
			const auto separated = [&](Vec3 axis) {
				const Scalar p0{dot(axis, triangle[0])}, p1{dot(axis, triangle[1])}, p2{dot(axis, triangle[2])};
				const Scalar radius{half.x * std::abs(axis.x) + half.y * std::abs(axis.y) + half.z * std::abs(axis.z)};
				return std::min({p0, p1, p2}) > radius || std::max({p0, p1, p2}) < -radius;
			};
			const std::array edges{subtract(triangle[1], triangle[0]), subtract(triangle[2], triangle[1]), subtract(triangle[0], triangle[2])};
			// Box face normals, the nine edge cross products, then the triangle plane.
			for (const Vec3 axis : {Vec3{one(), zero(), zero()}, Vec3{zero(), one(), zero()}, Vec3{zero(), zero(), one()}}) {
				if (separated(axis)) { return false; }
				for (const Vec3 &edge : edges) { if (separated(cross(axis, edge))) { return false; } }
			}
			return !separated(cross(edges[0], edges[1]));
		}

		/// @brief Rounds a size or address up to a multiple of the alignment.
		[[nodiscard]] constexpr VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) {
			return (value + alignment - 1U) / alignment * alignment;
		}

		/// @brief Address shaders and acceleration structure builds use to reach a buffer.
		[[nodiscard]] VkDeviceAddress bufferAddress(VkDevice device, VkBuffer buffer) {
			const VkBufferDeviceAddressInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer};
			return vkGetBufferDeviceAddress(device, &info);
		}

		/// @brief Layout transition with explicit stages and access, for images the ray tracing stage writes.
		void imageBarrier(VkCommandBuffer command, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
			VkPipelineStageFlags sourceStage, VkAccessFlags sourceAccess, VkPipelineStageFlags targetStage, VkAccessFlags targetAccess) {
			const VkImageMemoryBarrier barrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = sourceAccess,
				.dstAccessMask = targetAccess,
				.oldLayout = oldLayout,
				.newLayout = newLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = image,
				.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1U, .layerCount = 1U},
			};
			vkCmdPipelineBarrier(command, sourceStage, targetStage, 0U, 0U, nullptr, 0U, nullptr, 1U, &barrier);
		}

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
		if (result == VK_SUCCESS) { stage = "ray tracing pipeline"; result = createPipeline(); }
		if (result != VK_SUCCESS) {
			cleanup();
			return fail(stage, result);
		}
		initialized_ = true;
		if (const auto scene = rebuildScene(); !scene) {
			cleanup();
			return std::unexpected(scene.error());
		}
		return {};
	}

	/// @brief Waits for the device, then releases every Vulkan object in reverse creation order.
	auto RadianceCascadesRenderer::cleanup() -> void {
		waitIdle();
		cascades_.clear();
		destroyScene();
		shader_table_.cleanup();
		if (device_.device != VK_NULL_HANDLE) {
			vkDestroyPipeline(device_.device, pipeline_, nullptr);
			vkDestroyPipelineLayout(device_.device, pipeline_layout_, nullptr);
			descriptor_pool_.cleanup(); // Frees descriptor_set_.
			vkDestroyDescriptorSetLayout(device_.device, set_layout_, nullptr);
		}
		pipeline_ = VK_NULL_HANDLE;
		pipeline_layout_ = VK_NULL_HANDLE;
		descriptor_set_ = VK_NULL_HANDLE;
		set_layout_ = VK_NULL_HANDLE;
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
		else { writeDescriptors(); } // The output image is a new object.
		return result;
	}

	/// @brief Creates the descriptor set, the ray tracing pipeline of the primary rays and its shader binding table.
	VkResult RadianceCascadesRenderer::createPipeline() {
		const auto &dispatch = *device_.device.handle.getDispatcher();
		const std::array bindings{
			VkDescriptorSetLayoutBinding{.binding = VVE_RC_SCENE_BINDING, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
				.descriptorCount = 1U, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR},
			VkDescriptorSetLayoutBinding{.binding = VVE_RC_OUTPUT_BINDING, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1U, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR},
			VkDescriptorSetLayoutBinding{.binding = VVE_RC_MATERIAL_BINDING, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = 1U, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
			VkDescriptorSetLayoutBinding{.binding = VVE_RC_VOXEL_ALBEDO_BINDING, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1U, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
			VkDescriptorSetLayoutBinding{.binding = VVE_RC_VOXEL_EMISSIVE_BINDING, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1U, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
			VkDescriptorSetLayoutBinding{.binding = VVE_RC_VOXEL_NORMAL_BINDING, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1U, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
		};
		const VkDescriptorSetLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = static_cast<std::uint32_t>(bindings.size()), .pBindings = bindings.data()};
		VkResult result = vkCreateDescriptorSetLayout(device_.device, &layoutInfo, nullptr, &set_layout_);
		if (result == VK_SUCCESS) { result = descriptor_pool_.create(device_.device, 1U, bindings); }
		if (result != VK_SUCCESS) { return result; }
		const VkDescriptorPool pool{descriptor_pool_.descriptorPool};
		const VkDescriptorSetAllocateInfo setInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = pool,
			.descriptorSetCount = 1U, .pSetLayouts = &set_layout_};
		result = vkAllocateDescriptorSets(device_.device, &setInfo, &descriptor_set_);
		if (result != VK_SUCCESS) { return result; }

		const VkPushConstantRange constants{.stageFlags = constantStages, .size = sizeof(FrameConstants)};
		const VkPipelineLayoutCreateInfo pipelineLayoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1U,
			.pSetLayouts = &set_layout_, .pushConstantRangeCount = 1U, .pPushConstantRanges = &constants};
		result = vkCreatePipelineLayout(device_.device, &pipelineLayoutInfo, nullptr, &pipeline_layout_);
		if (result != VK_SUCCESS) { return result; }

		// Stage order is also the shader group order: ray generation, miss, closest hit.
		std::array<VulkanShaderModule, 3> modules{};
		result = modules[0].create(device_.device, simple_rc_primary_raygen_spv);
		if (result == VK_SUCCESS) { result = modules[1].create(device_.device, simple_rc_primary_miss_spv); }
		if (result == VK_SUCCESS) { result = modules[2].create(device_.device, simple_rc_primary_closest_hit_spv); }
		if (result != VK_SUCCESS) { return result; }
		const auto stage = [&modules](std::size_t index, VkShaderStageFlagBits bit, const char *entry) {
			return VkPipelineShaderStageCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = bit,
				.module = modules[index].shaderModule, .pName = entry};
		};
		const std::array stages{stage(0U, VK_SHADER_STAGE_RAYGEN_BIT_KHR, "primaryRaygen"), stage(1U, VK_SHADER_STAGE_MISS_BIT_KHR, "primaryMiss"),
			stage(2U, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "primaryClosestHit")};
		const auto group = [](VkRayTracingShaderGroupTypeKHR type, std::uint32_t general, std::uint32_t closestHit) {
			return VkRayTracingShaderGroupCreateInfoKHR{.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = type,
				.generalShader = general, .closestHitShader = closestHit, .anyHitShader = VK_SHADER_UNUSED_KHR, .intersectionShader = VK_SHADER_UNUSED_KHR};
		};
		const std::array groups{group(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0U, VK_SHADER_UNUSED_KHR),
			group(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1U, VK_SHADER_UNUSED_KHR),
			group(VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2U)};
		const VkRayTracingPipelineCreateInfoKHR pipelineInfo{
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
			.stageCount = static_cast<std::uint32_t>(stages.size()),
			.pStages = stages.data(),
			.groupCount = static_cast<std::uint32_t>(groups.size()),
			.pGroups = groups.data(),
			.maxPipelineRayRecursionDepth = 1U, // Only the ray generation stage traces.
			.layout = pipeline_layout_,
		};
		result = dispatch.vkCreateRayTracingPipelinesKHR(device_.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1U, &pipelineInfo, nullptr, &pipeline_);
		if (result != VK_SUCCESS) { return result; }

		// Shader binding table: one aligned region with one handle per group.
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR limits{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
		VkPhysicalDeviceProperties2 properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &limits};
		vkGetPhysicalDeviceProperties2(physical_device_.physicalDevice, &properties);
		const VkDeviceSize handleSize{limits.shaderGroupHandleSize};
		const VkDeviceSize stride{alignUp(handleSize, limits.shaderGroupHandleAlignment)};
		const VkDeviceSize regionSize{alignUp(stride, limits.shaderGroupBaseAlignment)};
		auto handles = std::vector<std::byte>(groups.size() * handleSize);
		result = dispatch.vkGetRayTracingShaderGroupHandlesKHR(device_.device, pipeline_, 0U, static_cast<std::uint32_t>(groups.size()), handles.size(), handles.data());
		if (result != VK_SUCCESS) { return result; }
		// The buffer's own address has no guaranteed alignment, so reserve one alignment of slack and start at the next boundary.
		result = shader_table_.create(allocator_, groups.size() * regionSize + limits.shaderGroupBaseAlignment,
			VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, BufferMemory::upload);
		if (result != VK_SUCCESS) { return result; }
		const VkDeviceAddress address{bufferAddress(device_.device, shader_table_.buffer)};
		const VkDeviceAddress first{alignUp(address, limits.shaderGroupBaseAlignment)};
		for (std::size_t index{}; index < groups.size(); ++index) {
			std::memcpy(static_cast<std::byte *>(shader_table_.mapped) + (first - address) + index * regionSize, handles.data() + index * handleSize, handleSize);
			shader_regions_[index] = {.deviceAddress = first + index * regionSize, .stride = stride, .size = stride};
		}
		return VK_SUCCESS;
	}

	/// @brief Voxelizes the bound scene, then rebuilds the voxel volumes, acceleration structures and material buffer.
	/// @return Error::not_initialized before init, the voxelize errors (the previous scene stays), or
	/// Error::platform_error when Vulkan fails (the renderer then has no scene).
	auto RadianceCascadesRenderer::rebuildScene() -> std::expected<void, Error> {
		if (!initialized_) { return std::unexpected(Error::not_initialized); }
		auto voxels = scene_ != nullptr ? voxelize(*scene_, settings_.voxel_size) : RadianceCascadesVoxels{};
		if (!voxels) { return std::unexpected(voxels.error()); }
		if (const VkResult result = buildScene(*voxels); result != VK_SUCCESS) {
			waitIdle();
			destroyScene(); // Never leave a structure without its voxel volumes.
			return fail("scene", result);
		}
		return {};
	}

	/// @brief Voxelizes the traced instances into a grid snapped to multiples of voxelSize, with one empty voxel of
	/// padding on every side. A triangle marks every voxel it touches, faces included, so a surface lying on a voxel
	/// boundary marks both neighbours and every hit point lies in a marked voxel.
	/// @return Error::invalid_argument for a voxel size that is not finite and positive, Error::capacity_exceeded when the
	/// grid needs more than maxVoxelsPerAxis along an axis or more than maxVoxels in total.
	auto RadianceCascadesRenderer::voxelize(const RenderScene &scene, Scalar voxelSize) -> std::expected<RadianceCascadesVoxels, Error> {
		if (!std::isfinite(voxelSize) || voxelSize <= zero()) { return std::unexpected(Error::invalid_argument); }
		auto voxels = RadianceCascadesVoxels{.voxel_size = voxelSize};

		// World-space triangles of every traced instance, with their material (null selects white).
		auto triangles = std::vector<std::pair<std::array<Vec3, 3>, const RenderMaterial *>>{};
		Vec3 lower{std::numeric_limits<Scalar>::max()};
		Vec3 upper{std::numeric_limits<Scalar>::lowest()};
		for (const auto &instance : scene.instances()) {
			const RenderMesh *const mesh = scene.findMesh(instance.mesh);
			if (!traced(instance) || mesh == nullptr || !mesh->vertices) { continue; }
			const auto &vertices = *mesh->vertices;
			const RenderMaterial *const material = scene.findMaterial(instance.material);
			for (std::size_t first{}; first + 2U < mesh->indices.size(); first += 3U) {
				std::array<Vec3, 3> triangle{};
				bool valid{true};
				for (std::size_t corner{}; corner < 3U; ++corner) {
					const std::uint32_t index{mesh->indices[first + corner]};
					if (index >= vertices.size()) { valid = false; break; }
					triangle[corner] = Vec3{multiply(instance.world_transform, Vec4{vertices[index].position, one()})};
					for (int axis{}; axis < 3; ++axis) {
						lower[axis] = std::min(lower[axis], triangle[corner][axis]);
						upper[axis] = std::max(upper[axis], triangle[corner][axis]);
					}
				}
				if (valid) { triangles.emplace_back(triangle, material); }
			}
		}
		if (triangles.empty()) { return voxels; }

		std::uint64_t total{1U};
		for (int axis{}; axis < 3; ++axis) {
			const auto firstVoxel = static_cast<std::int64_t>(std::floor(lower[axis] / voxelSize)) - 1;
			const auto lastVoxel = static_cast<std::int64_t>(std::floor(upper[axis] / voxelSize)) + 1;
			const auto count = lastVoxel - firstVoxel + 1;
			if (count > std::int64_t{maxVoxelsPerAxis}) { return std::unexpected(Error::capacity_exceeded); }
			voxels.size[axis] = static_cast<std::uint32_t>(count);
			voxels.origin[axis] = static_cast<Scalar>(firstVoxel) * voxelSize;
			total *= voxels.size[axis];
		}
		if (total > maxVoxels) { return std::unexpected(Error::capacity_exceeded); }
		voxels.albedo.assign(total, Vec4{});
		voxels.emissive.assign(total, Vec4{});
		voxels.normal.assign(total, Vec4{});
		auto hits = std::vector<std::uint32_t>(total);

		// The box test is grown by a thousandth of a voxel, so float error at shared faces never drops a voxel.
		constexpr Scalar slack{static_cast<Scalar>(1e-3)};
		const Vec3 half{voxelSize * (static_cast<Scalar>(0.5) + slack)};
		for (const auto &[triangle, material] : triangles) {
			const Vec3 faceNormal{cross(subtract(triangle[1], triangle[0]), subtract(triangle[2], triangle[0]))};
			const Scalar area{length(faceNormal)};
			const Vec3 unitNormal{area > zero() ? scale(faceNormal, one() / area) : zeroVec3()};
			const Vec3 base{material != nullptr ? material->base_color.value : oneVec3()};
			const Vec3 emitted{material != nullptr ? material->emissive.value : zeroVec3()};
			std::array<std::uint32_t, 3> from{}, to{};
			for (int axis{}; axis < 3; ++axis) {
				const Scalar low{std::min({triangle[0][axis], triangle[1][axis], triangle[2][axis]})};
				const Scalar high{std::max({triangle[0][axis], triangle[1][axis], triangle[2][axis]})};
				const auto cell = [&](Scalar value) {
					const auto index = static_cast<std::int64_t>(std::floor((value - voxels.origin[axis]) / voxelSize));
					return static_cast<std::uint32_t>(std::clamp<std::int64_t>(index, 0, std::int64_t{voxels.size[axis]} - 1));
				};
				from[axis] = cell(low - voxelSize * slack);
				to[axis] = cell(high + voxelSize * slack);
			}
			for (std::uint32_t z{from[2]}; z <= to[2]; ++z) {
				for (std::uint32_t y{from[1]}; y <= to[1]; ++y) {
					for (std::uint32_t x{from[0]}; x <= to[0]; ++x) {
						const Vec3 cellIndex{static_cast<Scalar>(x), static_cast<Scalar>(y), static_cast<Scalar>(z)};
						const Vec3 centre{add(voxels.origin, scale(add(cellIndex, Vec3{static_cast<Scalar>(0.5)}), voxelSize))};
						if (!triangleTouchesBox(triangle, centre, half)) { continue; }
						const std::size_t index{x + std::size_t{voxels.size[0]} * (y + std::size_t{voxels.size[1]} * z)};
						voxels.albedo[index] = add(voxels.albedo[index], Vec4{base, zero()});
						voxels.emissive[index] = add(voxels.emissive[index], Vec4{emitted, zero()});
						voxels.normal[index] = add(voxels.normal[index], Vec4{unitNormal, zero()});
						++hits[index];
					}
				}
			}
		}
		for (std::size_t index{}; index < total; ++index) {
			if (hits[index] == 0U) { continue; }
			const Scalar weight{one() / static_cast<Scalar>(hits[index])};
			voxels.albedo[index] = Vec4{scale(Vec3{voxels.albedo[index]}, weight), one()};
			voxels.emissive[index] = Vec4{scale(Vec3{voxels.emissive[index]}, weight), zero()};
			const Vec3 sum{voxels.normal[index]};
			const Scalar sumLength{length(sum)};
			voxels.normal[index] = sumLength > static_cast<Scalar>(1e-4) ? Vec4{scale(sum, one() / sumLength), zero()} : Vec4{};
		}
		return voxels;
	}

	/// @brief Uploads the three voxel volumes as RGBA32F 2D array images with one layer per z slice.
	VkResult RadianceCascadesRenderer::uploadVoxels(const RadianceCascadesVoxels &voxels) {
		static_assert(sizeof(Vec4) == 4U * sizeof(float)); // RGBA32F texels; the simple engine requires float math.
		if (voxels.size[0] == 0U) { return VK_SUCCESS; }
		const VkExtent2D extent{.width = voxels.size[0], .height = voxels.size[1]};
		const std::uint32_t layers{voxels.size[2]};
		VulkanUploadBatch batch{};
		VkResult result = batch.begin(device_.device, device_.graphicsQueue, command_pool_.commandPool);
		const std::array sources{&voxels.albedo, &voxels.emissive, &voxels.normal};
		for (std::size_t image{}; image < voxels_.size() && result == VK_SUCCESS; ++image) {
			// A grid one voxel deep still needs an array view for the Texture2DArray bindings.
			result = voxels_[image].create(allocator_, device_.device, extent, VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, layers, false, true);
			if (result != VK_SUCCESS) { break; }
			const auto staging = batch.stage(allocator_, std::as_bytes(std::span{*sources[image]}));
			if (!staging) { result = staging.error(); break; }
			transitionImage(batch.commandBuffer, voxels_[image].image, VK_IMAGE_ASPECT_COLOR_BIT, 0U, layers,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			const VkBufferImageCopy region{
				.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = layers},
				.imageExtent = {.width = extent.width, .height = extent.height, .depth = 1U},
			};
			vkCmdCopyBufferToImage(batch.commandBuffer, *staging, voxels_[image].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U, &region);
			transitionImage(batch.commandBuffer, voxels_[image].image, VK_IMAGE_ASPECT_COLOR_BIT, 0U, layers,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		if (result == VK_SUCCESS) { result = batch.submit(); }
		if (result != VK_SUCCESS) { return result; }
		voxel_origin_ = voxels.origin;
		voxel_size_ = voxels.voxel_size;
		voxel_grid_ = voxels.size;
		return VK_SUCCESS;
	}

	/// @brief Releases the acceleration structures and every scene buffer; the device must be idle.
	void RadianceCascadesRenderer::destroyScene() {
		if (device_.device != VK_NULL_HANDLE) {
			const auto &dispatch = *device_.device.handle.getDispatcher();
			dispatch.vkDestroyAccelerationStructureKHR(device_.device, top_level_, nullptr);
			for (const auto structure : bottom_levels_) { dispatch.vkDestroyAccelerationStructureKHR(device_.device, structure, nullptr); }
		}
		top_level_ = VK_NULL_HANDLE;
		bottom_levels_.clear();
		scene_buffers_.clear();
		materials_.cleanup();
		for (auto &volume : voxels_) { volume.cleanup(); }
		voxel_origin_ = zeroVec3();
		voxel_size_ = zero();
		voxel_grid_ = {};
		traced_instances_ = 0U;
	}

	/// @brief Creates one acceleration structure, builds it and waits for the build.
	/// Its storage buffer joins scene_buffers_; the scratch buffer lives only for the build.
	auto RadianceCascadesRenderer::buildStructure(VkAccelerationStructureTypeKHR type, const VkAccelerationStructureGeometryKHR &geometry,
		std::uint32_t primitives) -> std::expected<VkAccelerationStructureKHR, VkResult> {
		const auto &dispatch = *device_.device.handle.getDispatcher();
		VkAccelerationStructureBuildGeometryInfoKHR build{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
			.type = type,
			.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
			.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
			.geometryCount = 1U,
			.pGeometries = &geometry,
		};
		VkAccelerationStructureBuildSizesInfoKHR sizes{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
		dispatch.vkGetAccelerationStructureBuildSizesKHR(device_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build, &primitives, &sizes);

		VulkanBuffer storage{};
		VkResult result = storage.create(allocator_, sizes.accelerationStructureSize,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, BufferMemory::device_local);
		if (result != VK_SUCCESS) { return std::unexpected(result); }
		const VkAccelerationStructureCreateInfoKHR createInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
			.buffer = storage.buffer, .size = sizes.accelerationStructureSize, .type = type};
		VkAccelerationStructureKHR structure{VK_NULL_HANDLE};
		result = dispatch.vkCreateAccelerationStructureKHR(device_.device, &createInfo, nullptr, &structure);
		if (result != VK_SUCCESS) { return std::unexpected(result); }

		// The scratch address must be aligned; reserve one alignment of slack and start at the next boundary.
		VkPhysicalDeviceAccelerationStructurePropertiesKHR limits{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
		VkPhysicalDeviceProperties2 properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &limits};
		vkGetPhysicalDeviceProperties2(physical_device_.physicalDevice, &properties);
		const VkDeviceSize alignment{std::max<VkDeviceSize>(limits.minAccelerationStructureScratchOffsetAlignment, 1U)};
		VulkanBuffer scratch{};
		result = scratch.create(allocator_, sizes.buildScratchSize + alignment,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, BufferMemory::device_local);
		if (result == VK_SUCCESS) {
			build.dstAccelerationStructure = structure;
			build.scratchData.deviceAddress = alignUp(bufferAddress(device_.device, scratch.buffer), alignment);
			const VkAccelerationStructureBuildRangeInfoKHR range{.primitiveCount = primitives};
			const VkAccelerationStructureBuildRangeInfoKHR *const ranges{&range};
			result = submitOnce(device_.device, device_.graphicsQueue, command_pool_.commandPool, [&](VkCommandBuffer command) {
				dispatch.vkCmdBuildAccelerationStructuresKHR(command, 1U, &build, &ranges);
			});
		}
		if (result != VK_SUCCESS) {
			dispatch.vkDestroyAccelerationStructureKHR(device_.device, structure, nullptr);
			return std::unexpected(result);
		}
		scene_buffers_.push_back(std::move(storage));
		return structure;
	}

	/// @brief Uploads the bound scene: voxel volumes, material rows, mesh geometry, one bottom-level structure per mesh
	/// and the top-level structure over the traced instances. An empty scene leaves top_level_ null.
	VkResult RadianceCascadesRenderer::buildScene(const RadianceCascadesVoxels &voxels) {
		waitIdle();
		destroyScene();
		if (scene_ == nullptr) { return VK_SUCCESS; }
		if (const VkResult uploaded = uploadVoxels(voxels); uploaded != VK_SUCCESS) { return uploaded; }
		const auto &dispatch = *device_.device.handle.getDispatcher();
		const auto &meshes = scene_->meshes();
		const auto &materials = scene_->materials();

		// Material rows keep RenderScene order, so an instance's row is its material's position; the last row is white.
		auto rows = std::vector<std::array<Vec4, 2>>{};
		rows.reserve(materials.size() + 1U);
		for (const auto &material : materials) {
			rows.push_back({Vec4{material.base_color.value, one()}, Vec4{material.emissive.value, zero()}});
		}
		rows.push_back({Vec4{one(), one(), one(), one()}, Vec4{}});
		VkResult result = materials_.create(allocator_, rows.size() * sizeof(rows.front()), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, BufferMemory::upload);
		if (result == VK_SUCCESS) { result = materials_.upload(rows.data(), rows.size() * sizeof(rows.front())); }
		if (result != VK_SUCCESS) { return result; }

		// Geometry: device-local vertex and index buffers, filled through one upload batch.
		constexpr VkBufferUsageFlags geometryUsage{VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
		auto geometry = std::vector<std::array<VkBuffer, 2>>(meshes.size()); // Vertex and index buffer per mesh; null without triangles.
		{
			VulkanUploadBatch batch{};
			result = batch.begin(device_.device, device_.graphicsQueue, command_pool_.commandPool);
			if (result != VK_SUCCESS) { return result; }
			const auto upload = [&](std::span<const std::byte> bytes) -> std::expected<VkBuffer, VkResult> {
				VulkanBuffer buffer{};
				if (const VkResult created = buffer.create(allocator_, bytes.size(), geometryUsage, BufferMemory::device_local); created != VK_SUCCESS) {
					return std::unexpected(created);
				}
				const auto staging = batch.stage(allocator_, bytes);
				if (!staging) { return std::unexpected(staging.error()); }
				const VkBufferCopy region{.size = bytes.size()};
				vkCmdCopyBuffer(batch.commandBuffer, *staging, buffer.buffer, 1U, &region);
				return scene_buffers_.emplace_back(std::move(buffer)).buffer;
			};
			for (std::size_t index{}; index < meshes.size(); ++index) {
				const auto &mesh = meshes[index];
				if (!mesh.vertices || mesh.vertices->empty() || mesh.indices.size() < 3U) { continue; }
				const auto vertices = upload(std::as_bytes(std::span{*mesh.vertices}));
				if (!vertices) { return vertices.error(); }
				const auto indices = upload(std::as_bytes(std::span{mesh.indices}));
				if (!indices) { return indices.error(); }
				geometry[index] = {*vertices, *indices};
			}
			result = batch.submit();
			if (result != VK_SUCCESS) { return result; }
		}

		bottom_levels_.assign(meshes.size(), VK_NULL_HANDLE);
		for (std::size_t index{}; index < meshes.size(); ++index) {
			if (geometry[index][0] == VK_NULL_HANDLE) { continue; }
			const auto &mesh = meshes[index];
			const VkAccelerationStructureGeometryKHR triangles{
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
				.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
				.geometry = {.triangles = {
					.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
					.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT, // RenderVertex starts with its position.
					.vertexData = {.deviceAddress = bufferAddress(device_.device, geometry[index][0])},
					.vertexStride = sizeof(RenderVertex),
					.maxVertex = static_cast<std::uint32_t>(mesh.vertices->size() - 1U),
					.indexType = VK_INDEX_TYPE_UINT32,
					.indexData = {.deviceAddress = bufferAddress(device_.device, geometry[index][1])},
				}},
				.flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
			};
			const auto structure = buildStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, triangles, static_cast<std::uint32_t>(mesh.indices.size() / 3U));
			if (!structure) { return structure.error(); }
			bottom_levels_[index] = *structure;
		}

		// Instances: the same ones the voxelizer sees.
		auto entries = std::vector<VkAccelerationStructureInstanceKHR>{};
		for (const auto &instance : scene_->instances()) {
			if (!traced(instance)) { continue; }
			const auto mesh = std::ranges::find(meshes, instance.mesh, &RenderMesh::handle);
			if (mesh == meshes.end() || bottom_levels_[static_cast<std::size_t>(mesh - meshes.begin())] == VK_NULL_HANDLE) { continue; }
			const auto material = std::ranges::find(materials, instance.material, &RenderMaterial::handle);
			const VkAccelerationStructureDeviceAddressInfoKHR addressInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
				.accelerationStructure = bottom_levels_[static_cast<std::size_t>(mesh - meshes.begin())]};
			auto &entry = entries.emplace_back();
			// Vulkan wants the upper three rows; Mat4 is indexed by column first.
			for (std::size_t row{}; row < 3U; ++row) {
				for (std::size_t column{}; column < 4U; ++column) {
					entry.transform.matrix[row][column] = instance.world_transform[static_cast<int>(column)][static_cast<int>(row)];
				}
			}
			entry.instanceCustomIndex = static_cast<std::uint32_t>(material - materials.begin()); // A missing material selects the white row.
			entry.mask = 0xFFU;
			entry.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			entry.accelerationStructureReference = dispatch.vkGetAccelerationStructureDeviceAddressKHR(device_.device, &addressInfo);
		}
		if (!entries.empty()) {
			VulkanBuffer instances{};
			result = instances.create(allocator_, entries.size() * sizeof(entries.front()),
				VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, BufferMemory::upload);
			if (result == VK_SUCCESS) { result = instances.upload(entries.data(), entries.size() * sizeof(entries.front())); }
			if (result != VK_SUCCESS) { return result; }
			const VkAccelerationStructureGeometryKHR instanceGeometry{
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
				.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
				.geometry = {.instances = {
					.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
					.data = {.deviceAddress = bufferAddress(device_.device, instances.buffer)},
				}},
				.flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
			};
			const auto structure = buildStructure(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, instanceGeometry, static_cast<std::uint32_t>(entries.size()));
			if (!structure) { return structure.error(); }
			top_level_ = *structure;
			traced_instances_ = entries.size();
		}
		writeDescriptors();
		return VK_SUCCESS;
	}

	/// @brief Points the descriptor set at the current output image and, when a scene exists, its structure and materials.
	/// The device must be idle; frames without a scene never use the set.
	void RadianceCascadesRenderer::writeDescriptors() {
		if (descriptor_set_ == VK_NULL_HANDLE) { return; }
		const VkDescriptorImageInfo outputInfo{.imageView = output_.imageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
		const VkWriteDescriptorSetAccelerationStructureKHR structureInfo{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
			.accelerationStructureCount = 1U, .pAccelerationStructures = &top_level_};
		const VkDescriptorBufferInfo materialInfo{.buffer = materials_.buffer, .range = VK_WHOLE_SIZE};
		const auto volumeInfo = [this](std::size_t image) {
			return VkDescriptorImageInfo{.imageView = voxels_[image].imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
		};
		const std::array volumeInfos{volumeInfo(0U), volumeInfo(1U), volumeInfo(2U)};
		const auto volumeWrite = [this, &volumeInfos](std::uint32_t binding, std::size_t image) {
			return VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptor_set_, .dstBinding = binding,
				.descriptorCount = 1U, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .pImageInfo = &volumeInfos[image]};
		};
		const std::array writes{
			VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptor_set_, .dstBinding = VVE_RC_OUTPUT_BINDING,
				.descriptorCount = 1U, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outputInfo},
			VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &structureInfo, .dstSet = descriptor_set_,
				.dstBinding = VVE_RC_SCENE_BINDING, .descriptorCount = 1U, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR},
			VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptor_set_, .dstBinding = VVE_RC_MATERIAL_BINDING,
				.descriptorCount = 1U, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &materialInfo},
			volumeWrite(VVE_RC_VOXEL_ALBEDO_BINDING, 0U),
			volumeWrite(VVE_RC_VOXEL_EMISSIVE_BINDING, 1U),
			volumeWrite(VVE_RC_VOXEL_NORMAL_BINDING, 2U),
		};
		// Without a top-level structure only the output image is written; with one, the voxel volumes exist too.
		vkUpdateDescriptorSets(device_.device, top_level_ == VK_NULL_HANDLE ? 1U : static_cast<std::uint32_t>(writes.size()), writes.data(), 0U, nullptr);
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

	/// @brief Traces one camera ray per pixel into the output image, which it leaves in the general layout.
	/// The hit currently returns the material colour; the irradiance of the merged cascade 0 is added in a later step.
	void RadianceCascadesRenderer::recordResolve(VkCommandBuffer command) {
		const auto &dispatch = *device_.device.handle.getDispatcher();
		// Camera basis: the image plane sits one unit in front of the camera.
		const Vec3 forward{normalize(camera_.forward.value)};
		const bool vertical{std::abs(forward.y) > static_cast<Scalar>(0.999)};
		const Vec3 right{normalize(cross(forward, vertical ? Vec3{zero(), zero(), one()} : Vec3{zero(), one(), zero()}))};
		const Vec3 up{cross(right, forward)};
		const Scalar halfHeight{std::tan(camera_.fov_y.radians * static_cast<Scalar>(0.5))};
		const Scalar aspect{static_cast<Scalar>(swapchain_.extent.width) / static_cast<Scalar>(swapchain_.extent.height)};
		const FrameConstants constants{
			.origin = Vec4{camera_.position.value, one()},
			.right = Vec4{scale(right, halfHeight * aspect), zero()},
			.up = Vec4{scale(up, halfHeight), zero()},
			.forward = Vec4{forward, zero()},
			.background = Vec4{background_color_.value, one()},
			.volume = Vec4{voxel_origin_, voxel_size_},
			.grid = Vec4{static_cast<Scalar>(voxel_grid_[0]), static_cast<Scalar>(voxel_grid_[1]), static_cast<Scalar>(voxel_grid_[2]),
				static_cast<Scalar>(std::to_underlying(view_))},
		};
		// Every pixel is written, so the previous contents are not needed.
		imageBarrier(command, output_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0U,
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_ACCESS_SHADER_WRITE_BIT);
		vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_);
		vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_layout_, 0U, 1U, &descriptor_set_, 0U, nullptr);
		vkCmdPushConstants(command, pipeline_layout_, constantStages, 0U, sizeof(constants), &constants);
		const VkStridedDeviceAddressRegionKHR callable{};
		dispatch.vkCmdTraceRaysKHR(command, &shader_regions_[0], &shader_regions_[1], &shader_regions_[2], &callable,
			swapchain_.extent.width, swapchain_.extent.height, 1U);
	}

	/// @brief Records one frame: fill the output image (ray traced scene, or the background colour without one), then
	/// blit it to the swapchain image.
	void RadianceCascadesRenderer::recordFrame(VkCommandBuffer command, std::uint32_t imageIndex) {
		constexpr VkImageAspectFlags colour{VK_IMAGE_ASPECT_COLOR_BIT};
		const VkImage target{swapchain_.images[imageIndex]};
		if (top_level_ != VK_NULL_HANDLE) {
			record(command);
			imageBarrier(command, output_.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		} else {
			const auto background = background_color_.value;
			const VkClearColorValue clear{.float32 = {background.x, background.y, background.z, 1.0F}};
			const VkImageSubresourceRange range{.aspectMask = colour, .levelCount = 1U, .layerCount = 1U};
			transitionImage(command, output_.image, colour, 0U, 1U, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			vkCmdClearColorImage(command, output_.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1U, &range);
			transitionImage(command, output_.image, colour, 0U, 1U, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		}
		// The previous contents of the swapchain image are not needed.
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
