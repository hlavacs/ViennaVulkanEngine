module;
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>
#if __has_include(<backends/imgui_impl_vulkan.h>)
#include <backends/imgui_impl_vulkan.h>
#else
#include <imgui_impl_vulkan.h>
#endif

export module VEEngine.Simple.Renderer;
import std;
import VEEngine.Types;
import VEEngine.Simple.RenderPassContract;
import VEEngine.Simple.Math;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

namespace vve::simple::detail {

	static constexpr std::string_view forward_renderer_pass{"forward.color_pass"};											///< Forward color pass node.
	static constexpr std::array forward_renderer_shadow_deps{RenderMilestone::frame_begin()};				///< Spot shadow depth dependency.
	static constexpr std::array forward_renderer_pass_deps{RenderMilestone::shadow_depth()};				///< Forward color pass dependency.
	static constexpr std::array forward_renderer_scene_done_deps{forward_renderer_pass};						///< Scene-color milestone input.
	static constexpr std::array forward_renderer_finished_deps{RenderMilestone::scene_color()};			///< Frame-finished input.
	static constexpr std::array forward_renderer_pass_contracts{															///< Minimal renderer pass contract list.
		RenderPassContract{.name = RenderMilestone::frame_begin(), .outputs = "frame inputs", .milestone = true},
		RenderPassContract{.name = RenderMilestone::shadow_depth(),
									.depends_on = forward_renderer_shadow_deps,
									.inputs = "registered render resources",
									.outputs = "spot shadow depth array"},
		RenderPassContract{.name = forward_renderer_pass,
									.depends_on = forward_renderer_pass_deps,
									.inputs = "registered render resources",
									.outputs = "forward scene color"},
		RenderPassContract{.name = RenderMilestone::scene_color(),
									.depends_on = forward_renderer_scene_done_deps,
									.outputs = "forward scene color is ready",
									.milestone = true},
		RenderPassContract{.name = RenderMilestone::frame_finished(),
									.depends_on = forward_renderer_finished_deps,
									.outputs = "frame can be presented",
									.milestone = true}};

} // namespace vve::simple::detail

/**
	* @file
	* @brief Vulkan renderer skeleton for the simple forward renderer.
	*
	* Functional objects:
	* - ShadowLightMeta records CPU-side light-to-shadow-layer bindings before shader data grows.
	* - ForwardRenderer owns the current CPU scene, swapchain image stack, depth attachment, unbound shadow maps and shadow pipeline, optional object texture, default object texture, forward render pass, framebuffers, descriptor-set layout, pipeline layout, shader modules, graphics pipeline, command pool, frame command buffers, frame synchronization, per-frame uniform buffers, descriptor pool, per-frame descriptor sets, and uploaded per-object meshes needed before rendering.
	* - StubRenderer exposes the same common renderer lifetime and scene surface without owning Vulkan state.
	* - SelectedRenderer names the explicit renderer alternatives available to the simple render coordinator.
	*/
export namespace vve::simple {

	/// @brief Per-light CPU shadow binding metadata prepared by the forward renderer.
	struct ShadowLightMeta {
		std::uint32_t light_index{};								///< Source light index inside the scene light array.
		std::uint32_t light_type{};								///< Light type tag; 1 means spot and 2 means point light.
		std::uint32_t shadow_slot{};							///< Shadow resource slot selected for this light.
		std::uint32_t first_layer{};							///< First depth-array layer rendered for this light.
		std::uint32_t layer_count{};							///< Number of depth-array layers owned by this light.
		Mat4 view{identityMat4()};								///< Light view matrix used by the depth pass.
		Mat4 projection{identityMat4()};					///< Light projection matrix used by the depth pass.
		Scalar near_plane{};										///< Near clipping plane for light-space depth.
		Scalar far_plane{};										///< Far clipping plane for light-space depth.
		Scalar depth_bias{};										///< Depth compare bias mirrored by CPU diagnostics.
		std::uint32_t resolution{};							///< Square shadow-map side length in pixels.
	};

	/// @brief Empty debug-sample type kept so the current facade can still compile against simple.
	struct RenderDebugSample {
		std::uint32_t vertex_id{};														///< Source vertex id.
		Vec3 world{zeroVec3()};															///< Stub world-space position.
		Vec4 clip{};																		///< Stub clip-space position.
		Vec4 light_clip{};																///< Stub directional-light clip position.
		Vec4 spot_light_clip{};															///< Stub spot-light clip position.
		Vec4 point_light_clip{};														///< Stub point-light clip position.
		Vec3 ndc{zeroVec3()};															///< Stub normalized device coordinate.
		Vec3 light_ndc{zeroVec3()};													///< Stub directional-light NDC.
		Vec3 spot_light_ndc{zeroVec3()};												///< Stub spot-light NDC.
		Vec3 point_light_ndc{zeroVec3()};											///< Stub point-light NDC.
		Vec3 normal{zeroVec3()};														///< Stub normal.
		Vec3 direction_to_light{zeroVec3()};										///< Stub light direction.
		Vec3 ambient_lighting{zeroVec3()};											///< Stub ambient term.
		Vec3 direct_lighting{zeroVec3()};											///< Stub direct-light term.
		Vec3 point_lighting{zeroVec3()};												///< Stub point-light term.
		Vec3 spot_lighting{zeroVec3()};												///< Stub spot-light term.
		Vec3 final_lighting{zeroVec3()};												///< Stub final-light term.
		float depth{};																		///< Stub depth value.
		float light_depth{};																///< Stub directional-light depth.
		float spot_light_depth{};														///< Stub spot-light depth.
		float point_light_depth{};														///< Stub point-light depth.
		float sampled_shadow_depth{};													///< Stub sampled shadow depth.
		float shadow_depth_delta{};													///< Stub shadow delta.
		float shadow_bias{};																///< Stub shadow bias.
		float shadow_factor{};															///< Stub shadow factor.
		float sampled_spot_shadow_depth{};											///< Stub sampled spot shadow depth.
		float spot_shadow_depth_delta{};												///< Stub spot shadow delta.
		float spot_shadow_bias{};														///< Stub spot shadow bias.
		float spot_shadow_factor{};													///< Stub spot shadow factor.
		float sampled_point_shadow_depth{};											///< Stub sampled point shadow depth.
		float point_shadow_depth_delta{};											///< Stub point shadow delta.
		float point_shadow_bias{};														///< Stub point shadow bias.
		float point_shadow_factor{};													///< Stub point shadow factor.
		std::uint32_t point_shadow_face{};											///< Stub point shadow face.
		float n_dot_l{};																	///< Stub Lambert term.
		bool inside_light{};																///< Stub directional-light inclusion.
		bool inside_spot_light{};														///< Stub spot-light inclusion.
		bool inside_point_light{};														///< Stub point-light inclusion.
		bool valid{};																		///< Stub samples are never valid.
	};

	/// @brief Empty shadow proof type kept so the current facade can still compile against simple.
	struct RenderShadowDepthSample {
		std::uint32_t triangle_id{};													///< Source triangle id.
		std::uint32_t face_index{};													///< Shadow face id.
		Vec3 world{zeroVec3()};															///< World-space sample point.
		Vec3 light_ndc{zeroVec3()};													///< Light-space sample point.
		std::uint32_t pixel_x{};														///< Shadow-map x texel.
		std::uint32_t pixel_y{};														///< Shadow-map y texel.
		float expected_depth{};															///< CPU expected depth.
		float bias{};																		///< CPU compare bias.
		float shadow_factor{};															///< CPU shadow factor.
		float gpu_depth{};																///< GPU depth, absent in simple stubs.
		float error{};																		///< Absolute mismatch.
		bool has_gpu{};																	///< False for simple stubs.
		bool valid{};																		///< Stub samples are never valid.
	};

	/// @brief Minimal forward renderer owning Vulkan swapchain bring-up without draw state.
	struct ForwardRenderer {
		/// @brief Lightweight command-recording pass tag used by tests without introducing a render graph.
		enum class RecordedPass : std::uint8_t { directional_shadow, spot_shadow, point_shadow, forward_color };

		static constexpr std::uint32_t framesInFlight{2U}; ///< Number of independent frame command buffers to allocate.
		VulkanInstance instance{};             ///< Owned Vulkan instance wrapper.
		VulkanSurface surface{};               ///< Owned SDL-backed Vulkan surface wrapper.
		VulkanPhysicalDevice physicalDevice{}; ///< Selected borrowed Vulkan physical device wrapper.
		VulkanDevice device{};                 ///< Owned Vulkan logical device wrapper.
		VulkanSwapchain swapchain{};           ///< Owned swapchain wrapper for presentation images.
		VulkanImageViews imageViews{};         ///< Owned color image views for swapchain images.
		VulkanDepthImage depthImage{};         ///< Owned swapchain-sized depth attachment image and view.
		ShadowMap shadowMap{};                 ///< Owned unbound shadow-map image reserved for later shadow rendering.
		ShadowMap dirShadowArray{};            ///< Owned directional shadow-map texture array with one layer per active directional light.
		ShadowMap spotShadowMap{};             ///< Owned spot shadow-map image reserved for later shadow rendering.
		ShadowMap spotShadowArray{};           ///< Owned spot shadow-map texture array reserved for future multi-light shadows.
		ShadowMap pointShadowArray{};          ///< Owned point shadow-map texture array with six layers per shadowed point light.
		TextureImage objectTexture{};           ///< Owned optional base-color texture bound only when the loaded scene requests one.
		TextureImage defaultObjectTexture{};    ///< Owned opaque-white texture bound when the scene has no base-color texture.
		VulkanRenderPass renderPass{};         ///< Owned forward render pass for swapchain color output.
		VulkanFramebuffers framebuffers{};     ///< Owned swapchain framebuffers for render-pass attachments.
		VulkanDescriptorSetLayout descriptorSetLayout{}; ///< Owned frame-uniform and shadow-map descriptor-set layout.
		VulkanPipelineLayout pipelineLayout{}; ///< Owned graphics pipeline layout using the frame descriptor set.
		VulkanShaderModule vertShaderModule{}; ///< Owned forward vertex shader module.
		VulkanShaderModule fragShaderModule{}; ///< Owned forward fragment shader module.
		VulkanGraphicsPipeline graphicsPipeline{}; ///< Owned forward graphics pipeline for swapchain rendering.
		VulkanCommandPool commandPool{};       ///< Owned resettable command pool for the graphics queue family.
		VulkanDepthReadback spotShadowDepthReadback{}; ///< Owned one-layer spot shadow depth readback for debug samples.
		VulkanDepthReadback dirShadowDepthReadback{}; ///< Owned one-layer directional shadow depth readback for debug samples.
		VulkanDepthReadback pointShadowDepthReadback{}; ///< Owned one-layer point shadow depth readback for debug samples.
		VulkanCommandBuffers commandBuffers{}; ///< Owned primary command buffers, one for each frame in flight.
		VulkanFrameSync frameSync{};           ///< Owned per-frame semaphores and fences for rendering.
		VulkanUniformBuffers uniformBuffers{}; ///< Owned per-frame uniform buffers for camera and object data.
		VulkanDescriptorPool descriptorPool{}; ///< Owned descriptor pool for per-frame uniform and shadow-map descriptor sets.
		VulkanDescriptorSets descriptorSets{}; ///< Owned per-frame descriptor sets binding frame uniform buffers and the shadow map.
		std::vector<VulkanMesh> meshes{};      ///< Owned GPU meshes uploaded from the current scene objects.
		SDL_Window *window{nullptr};           ///< Borrowed SDL window used to create the Vulkan surface.
		Scene scene{}; ///< CPU scene data kept in STL containers until renderer upload exists.
		std::vector<ShadowLightMeta> shadowLightMeta{}; ///< Per-light spot shadow metadata prepared for debug access.
		std::vector<RecordedPass> recordedPassOrder{}; ///< Last frame's command-recording pass order diagnostic.
		Vec3 cameraEye{zero(), static_cast<Scalar>(6.0), static_cast<Scalar>(9.0)}; ///< World-space camera position used for the frame view matrix.
		Vec3 cameraTarget{zero(), one(), zero()}; ///< World-space point looked at by the frame view matrix.
		std::array<Mat4, kMaxShadowedSpotLights> spotLightViewProjs{}; ///< CPU spot-light matrices prepared for later multi-shadow rendering.
		std::size_t spotLightViewProjCount{}; ///< Number of active spot-light matrices copied from the current scene.
		std::optional<std::uint32_t> lastRenderedImageIndex{}; ///< Swapchain image index from the last acquired, rendered, and presented frame.
		std::optional<VkResult> lastReadbackCaptureResult{}; ///< Result from the optional in-frame readback capture.

		~ForwardRenderer() { cleanup(); }

		/// @brief Returns the stable renderer-selection id for this concrete forward renderer.
		[[nodiscard]] RendererId id() const { return RendererId{.value = "forward"}; }

		/// @brief Declares this renderer's render passes and dependencies for graph merging.
		[[nodiscard]] std::span<const RenderPassContract> passes() const { return detail::forward_renderer_pass_contracts; }

		/// @brief Returns command-recorded pass tags from the most recent frame.
		[[nodiscard]] std::span<const RecordedPass> lastRecordedPassOrder() const { return recordedPassOrder; }

		/// @brief Releases renderer-owned resources through the existing cleanup path.
		void shutdown() { cleanup(); }

		/// @brief Stores the non-owning GUI system handle reserved for later GUI rendering integration.
		void setGuiSystem(void *gui) { guiSystem_ = gui; }

		/// @brief Stores the optional GUI command recorder used inside the forward color pass.
		void setGuiRecordSink(std::function<void(VkCommandBuffer)> sink) { guiRecord_ = std::move(sink); }

		/// @brief Reports whether the renderer currently owns a live Vulkan device.
		[[nodiscard]] bool initialized() const { return device.device != VK_NULL_HANDLE; }

		/// @brief Reports presented-frame diagnostics for the forward renderer.
		[[nodiscard]] std::uint64_t presentedFrameCount() const { return 0; }
		/// @brief Reports triangle-draw diagnostics for the forward renderer.
		[[nodiscard]] std::uint64_t triangleDrawCount() const { return 0; }
		/// @brief Reports triangle-vertex diagnostics for the forward renderer.
		[[nodiscard]] std::uint32_t triangleVertexCount() const { return 0; }
		/// @brief Reports scene-upload diagnostics for the forward renderer.
		[[nodiscard]] std::uint64_t sceneUploadCount() const { return 0; }
		/// @brief Reports scene-mesh draw diagnostics for the forward renderer.
		[[nodiscard]] std::uint64_t sceneMeshDrawCount() const { return 0; }
		/// @brief Reports scene-instance draw diagnostics for the forward renderer.
		[[nodiscard]] std::uint64_t sceneInstanceDrawCount() const { return 0; }
		/// @brief Reports scene-draw vertex diagnostics for the forward renderer.
		[[nodiscard]] std::uint32_t sceneDrawVertexCount() const { return 0; }
		/// @brief Reports scene-draw index diagnostics for the forward renderer.
		[[nodiscard]] std::uint32_t sceneDrawIndexCount() const { return 0; }
		/// @brief Reports the number of retained debug samples for the forward renderer.
		[[nodiscard]] std::size_t sceneDebugSampleCount() const { return 0; }
		/// @brief Returns a CPU debug sample when the forward renderer has retained one.
		[[nodiscard]] std::optional<RenderDebugSample> sceneCpuDebugSample(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns a GPU debug sample when the forward renderer has retained one.
		[[nodiscard]] std::optional<RenderDebugSample> sceneGpuDebugSample(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns clip-space debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugClipError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns depth debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugDepthError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns directional light-space debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugLightSpaceError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns spot light-space debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugSpotLightSpaceError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns point light-space debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugPointLightSpaceError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns lighting debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugLightingError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns directional shadow-sample debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugShadowSampleError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns spot shadow-sample debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugSpotShadowSampleError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns point shadow-sample debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugPointShadowSampleError(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports the number of retained directional shadow-depth samples for the forward renderer.
		[[nodiscard]] std::size_t sceneShadowDepthSampleCount() const { return directionalShadowDepthSampleCountStorage(); }
		/// @brief Returns a directional shadow-depth sample when the forward renderer has retained one.
		[[nodiscard]] std::optional<RenderShadowDepthSample> sceneShadowDepthSample(std::size_t index) const {
			if (index >= directionalShadowDepthSampleCountStorage()) { return {}; }
			return directionalShadowDepthSampleStorage()[index];
		}
		/// @brief Returns directional shadow-depth error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneShadowDepthError(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports the number of retained spot shadow-depth samples for the forward renderer.
		[[nodiscard]] std::size_t sceneSpotShadowDepthSampleCount() const { return spotShadowDepthSampleCountStorage(); }
		/// @brief Returns a spot shadow-depth sample when the forward renderer has retained one.
		[[nodiscard]] std::optional<RenderShadowDepthSample> sceneSpotShadowDepthSample(std::size_t index) const {
			if (index >= spotShadowDepthSampleCountStorage()) { return {}; }
			return spotShadowDepthSampleStorage()[index];
		}
		/// @brief Reports the number of prepared spot shadow metadata rows.
		[[nodiscard]] std::size_t sceneShadowLightMetaCount() const { return shadowLightMeta.size(); }
		/// @brief Returns one prepared spot shadow metadata row by retained index.
		[[nodiscard]] std::optional<ShadowLightMeta> sceneShadowLightMeta(std::size_t index) const {
			if (index >= shadowLightMeta.size()) { return {}; }
			return shadowLightMeta[index];
		}
		/// @brief Returns spot shadow-depth error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneSpotShadowDepthError(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports the number of retained point shadow-depth samples for the forward renderer.
		[[nodiscard]] std::size_t scenePointShadowDepthSampleCount() const { return pointShadowDepthSampleCountStorage(); }
		/// @brief Returns a point shadow-depth sample when the forward renderer has retained one.
		[[nodiscard]] std::optional<RenderShadowDepthSample> scenePointShadowDepthSample(std::size_t index) const {
			if (index >= pointShadowDepthSampleCountStorage()) { return {}; }
			return pointShadowDepthSampleStorage()[index];
		}
		/// @brief Returns point shadow-depth error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> scenePointShadowDepthError(std::size_t index) const {
			if (index >= pointShadowDepthSampleCountStorage()) { return {}; }
			return pointShadowDepthSampleStorage()[index].error;
		}
		/// @brief Reports prepared GPU target diagnostics for the forward renderer.
		[[nodiscard]] std::size_t preparedGpuTargetCount() const { return 0; }
		std::array<float, 4> clearColor{0.0F, 0.0F, 0.0F, 1.0F}; ///< Last clear color used by the renderer.
		/// @brief Reports the last clear color used by the forward renderer.
		[[nodiscard]] std::array<float, 4> lastClearColor() const { return clearColor; }

		/**
			* @brief Initializes the Vulkan instance, device, swapchain, image views, depth attachment, shadow map, render pass, framebuffers, descriptor-set layout, pipeline layout, shader modules, graphics pipeline, command pool, command buffers, frame synchronization, per-frame uniform buffers, descriptor pool, per-frame descriptor sets, and uploaded per-object meshes.
			*
			* @param sdlWindow Borrowed SDL window that owns the native platform surface.
			* @return VK_SUCCESS after graphics-pipeline bring-up, otherwise the first failing Vulkan result.
			*/
		[[nodiscard]] VkResult init(SDL_Window *sdlWindow) {
			// CMake provides the binary shader directory so runtime loading follows the generated SPIR-V files.
			const std::string shaderDir{VVE_SIMPLE_SHADER_DIR};
			const std::string vertSpirvPath{shaderDir + "/simple_forward.vert.spv"};
			const std::string fragSpirvPath{shaderDir + "/simple_forward.frag.spv"};
			const std::string shadowVertSpirvPath{shaderDir + "/simple_forward.shadow.vert.spv"};
			const std::string dirShadowVertSpirvPath{shaderDir + "/simple_forward.dir_shadow.vert.spv"}; ///< Directional shadow vertex shader.
			const std::string spotShadowVertSpirvPath{shaderDir + "/simple_forward.spot_shadow.vert.spv"}; ///< Spot shadow vertex shader.
			const std::string pointShadowVertSpirvPath{shaderDir + "/simple_forward.point_shadow.vert.spv"}; ///< Point shadow vertex shader.

			window = sdlWindow;
			if (window == nullptr) { return VK_ERROR_INITIALIZATION_FAILED; }

			VkResult result = instance.create();
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = surface.create(instance.instance, window);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = physicalDevice.select(instance.instance, surface.surface);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = device.create(physicalDevice);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			int width{};
			int height{};
			SDL_GetWindowSizeInPixels(window, &width, &height);

			result = swapchain.create(
				physicalDevice.physicalDevice,
				device.device,
				surface.surface,
				*physicalDevice.graphicsQueueFamily,
				*physicalDevice.presentQueueFamily,
				static_cast<std::uint32_t>(width),
				static_cast<std::uint32_t>(height));
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = imageViews.create(device.device, swapchain.images, swapchain.imageFormat);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = depthImage.create(physicalDevice.physicalDevice, device.device, swapchain.extent);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = descriptorSetLayout.create(device.device);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			VulkanVertexInputDescription vertexInput{}; // Fixed mesh vertex layout shared by forward and shadow pipelines.

			result = shadowMap.create(physicalDevice.physicalDevice, device.device);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = dirShadowArray.create(physicalDevice.physicalDevice, device.device, kMaxDirectionalLights);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = spotShadowMap.create(physicalDevice.physicalDevice, device.device);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = spotShadowArray.create(physicalDevice.physicalDevice, device.device, kMaxShadowedSpotLights);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			constexpr std::uint32_t pointShadowArrayLayerCount{static_cast<std::uint32_t>(kMaxShadowedPointLights * 6U)}; // Six cubemap-style faces per shadowed point light.
			result = pointShadowArray.create(physicalDevice.physicalDevice, device.device, pointShadowArrayLayerCount);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = pointShadowArray.createPipeline(descriptorSetLayout.descriptorSetLayout, pointShadowVertSpirvPath, "shadowVertexMainPoint", vertexInput);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = dirShadowArray.createPipeline(descriptorSetLayout.descriptorSetLayout, dirShadowVertSpirvPath, "shadowVertexMainDir", vertexInput);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = spotShadowArray.createPipeline(descriptorSetLayout.descriptorSetLayout, spotShadowVertSpirvPath, "shadowVertexMainSpot", vertexInput);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = renderPass.create(device.device, swapchain.imageFormat);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = framebuffers.create(device.device, renderPass.renderPass, imageViews.views, swapchain.extent, depthImage.view);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = pipelineLayout.create(device.device, descriptorSetLayout.descriptorSetLayout);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = shadowMap.createPipeline(descriptorSetLayout.descriptorSetLayout, shadowVertSpirvPath, "shadowVertexMain", vertexInput);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = spotShadowMap.createPipeline(descriptorSetLayout.descriptorSetLayout, spotShadowVertSpirvPath, "shadowVertexMainSpot", vertexInput);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = vertShaderModule.create(device.device, vertSpirvPath);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = fragShaderModule.create(device.device, fragSpirvPath);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = graphicsPipeline.create(
				device.device,
				renderPass.renderPass,
				pipelineLayout.pipelineLayout,
				vertShaderModule.shaderModule,
				fragShaderModule.shaderModule,
				vertexInput,
				swapchain.extent);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = commandPool.create(device.device, *physicalDevice.graphicsQueueFamily);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = spotShadowDepthReadback.create(physicalDevice.physicalDevice, device.device, device.graphicsQueue, commandPool.commandPool, VkExtent2D{.width = ShadowMap::resolution, .height = ShadowMap::resolution});
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = dirShadowDepthReadback.create(physicalDevice.physicalDevice, device.device, device.graphicsQueue, commandPool.commandPool, VkExtent2D{.width = ShadowMap::resolution, .height = ShadowMap::resolution});
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = pointShadowDepthReadback.create(physicalDevice.physicalDevice, device.device, device.graphicsQueue, commandPool.commandPool, VkExtent2D{.width = ShadowMap::resolution, .height = ShadowMap::resolution});
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = commandBuffers.create(device.device, commandPool.commandPool, framesInFlight);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = frameSync.create(device.device, framesInFlight, static_cast<std::uint32_t>(swapchain.images.size()));
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = uniformBuffers.create(physicalDevice.physicalDevice, device.device, framesInFlight);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = descriptorPool.create(device.device, framesInFlight);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			createImguiDescriptorPool();

			result = descriptorSets.create(device.device, descriptorPool.descriptorPool, descriptorSetLayout.descriptorSetLayout, framesInFlight);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			// Upload the optional scene base-color texture once descriptor sets and upload commands exist.
			const bool hasObjectTexture{scene.baseColorTexture.has_value() && objectTexture.create(physicalDevice.physicalDevice, device.device, device.graphicsQueue, commandPool.commandPool, *scene.baseColorTexture) == VK_SUCCESS};
			if (!hasObjectTexture) {
				constexpr std::array opaqueWhitePixel{std::byte{255U}, std::byte{255U}, std::byte{255U}, std::byte{255U}}; // Valid fallback for unused texture descriptors.
				result = defaultObjectTexture.create(physicalDevice.physicalDevice, device.device, device.graphicsQueue, commandPool.commandPool, std::span{opaqueWhitePixel}, VkExtent2D{.width = 1U, .height = 1U});
				if (result != VK_SUCCESS) { cleanup(); return result; }
			}
			const TextureImage &descriptorTexture{hasObjectTexture ? objectTexture : defaultObjectTexture};

			// Bind each frame descriptor set to its matching uniform buffer, shadow maps, and object texture slot.
			for (std::uint32_t frame{}; frame < framesInFlight; ++frame) {
				result = descriptorSets.writeUniformBuffer(frame, uniformBuffers.buffers[frame].buffer, sizeof(FrameUniforms));
				if (result != VK_SUCCESS) { cleanup(); return result; }
				result = descriptorSets.writeShadowMap(frame, shadowMap.view, shadowMap.sampler);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				result = descriptorSets.writeDirShadowArray(frame, dirShadowArray.view, dirShadowArray.sampler); // Bind directional shadow-array sampler.
				if (result != VK_SUCCESS) { cleanup(); return result; }
				result = descriptorSets.writeSpotShadowMap(frame, spotShadowMap.view, spotShadowMap.sampler);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				result = descriptorSets.writeSpotShadowArray(frame, spotShadowArray.view, spotShadowArray.sampler);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				result = descriptorSets.writePointShadowArray(frame, pointShadowArray.view, pointShadowArray.sampler);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				result = descriptorSets.writeObjectTexture(frame, descriptorTexture);
				if (result != VK_SUCCESS) { cleanup(); return result; }
			}

			// Upload one GPU mesh for each object in the current CPU scene.
			for (const Object &object : scene.objects) {
				VulkanMesh &mesh = meshes.emplace_back();
				result = mesh.create(physicalDevice.physicalDevice, device.device, object.mesh);
				if (result != VK_SUCCESS) { cleanup(); return result; }
			}

			return VK_SUCCESS;
		}

		/**
			* @brief Replaces the current CPU scene before future renderer upload.
			*
			* @param nextScene Scene data prepared by the caller.
		*/
		void loadScene(Scene nextScene) {
			scene = std::move(nextScene);								// Scene upload invalidates per-frame shadow metadata.
			shadowLightMeta.clear();									// Metadata is rebuilt during the next frame assembly.
		}

		/// @brief Appends one backend object to the renderer-owned CPU scene mirror.
		void appendObject(Mesh backend_mesh, Mat4 model, std::optional<std::string> base_color_texture_source) {
			const bool use_texture = base_color_texture_source.has_value();
			if (use_texture) { scene.baseColorTexture = *std::move(base_color_texture_source); }
			scene.objects.push_back(Object{.mesh = std::move(backend_mesh),
													 .model = model,
													 .useBaseColorTexture = use_texture ? 1U : 0U});
		}

		/// @brief Clears the renderer-owned CPU scene through the existing scene replacement path.
		void clearScene() { loadScene(Scene{}); }

		/**
			* @brief Stores the camera eye and target used by future frame uniform updates.
			*
			* @param eye World-space camera position.
			* @param target World-space point the camera looks at.
			*/
		void setCamera(Vec3 eye, Vec3 target) {
			cameraEye = eye;
			cameraTarget = target;
		}

		/**
			* @brief Draws one swapchain frame through the per-frame synchronization objects.
			*/
		void drawFrame(VulkanReadback *readback = nullptr) {
			lastReadbackCaptureResult.reset();
			const auto windowExtent = currentWindowPixelExtent();
			if (windowExtent.width == 0U || windowExtent.height == 0U) { return; }
			if (windowExtent.width != swapchain.extent.width || windowExtent.height != swapchain.extent.height) {
				if (recreateSwapchain(windowExtent) != VK_SUCCESS) { return; }
			}

			const std::size_t frameCount{frameSync.inFlightFences.size()}; // Existing sync count defines frames in flight.
			if (frameCount == 0U || frameSync.imageAvailableSemaphores.size() < frameCount || frameSync.renderFinishedSemaphores.empty()) { return; }
			if (commandBuffers.commandBuffers.size() < frameCount || device.device == VK_NULL_HANDLE || swapchain.swapchain == VK_NULL_HANDLE) { return; }
			if (currentFrame >= frameCount) { currentFrame = 0U; }

			const VkFence inFlightFence{frameSync.inFlightFences[currentFrame]};
			const VkSemaphore imageAvailableSemaphore{frameSync.imageAvailableSemaphores[currentFrame]};
			if (inFlightFence == VK_NULL_HANDLE || imageAvailableSemaphore == VK_NULL_HANDLE) { return; }

			VkResult result = vkWaitForFences(device.device, 1U, &inFlightFence, VK_TRUE, UINT64_MAX);
			if (result != VK_SUCCESS) { return; }

			std::uint32_t imageIndex{};
			result = vkAcquireNextImageKHR(device.device, swapchain.swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
			if (result == VK_ERROR_OUT_OF_DATE_KHR) { (void)recreateSwapchain(currentWindowPixelExtent()); return; }
			if (result != VK_SUCCESS) { return; }
			if (imageIndex >= frameSync.renderFinishedSemaphores.size()) { return; }
			const VkSemaphore renderFinishedSemaphore{frameSync.renderFinishedSemaphores[imageIndex]}; // Present-wait semaphore follows the acquired swapchain image.
			if (renderFinishedSemaphore == VK_NULL_HANDLE) { return; }

			result = vkResetFences(device.device, 1U, &inFlightFence);
			if (result != VK_SUCCESS) { return; }

			const Scalar aspectRatio{swapchain.extent.height == 0U ? one() : static_cast<Scalar>(swapchain.extent.width) / static_cast<Scalar>(swapchain.extent.height)}; ///< Live swapchain aspect with a zero-height guard.
			const PointLight light{scene.pointLight};
			const DirectionalLight dirLight{scene.directionalLight}; ///< Directional light data uploaded for future shader use.
			const SpotLight spot{scene.spotLight}; ///< Spot light data uploaded for future shader use.
			const Vec3 dirLightDirection{normalize(dirLight.direction)}; ///< Normalized world-space direction keeps uniform packing stable.
			const Vec3 spotDirection{normalize(spot.direction)}; ///< Normalized world-space direction keeps uniform packing stable.
			const Vec3 lightCenter{zeroVec3()}; ///< Origin-centered debug scene framing.
			const Vec3 shadowSurfaceLightDir{normalize(subtract(light.position, lightCenter))}; ///< Point-light direction approximated by one shadow map.
			const Vec3 lightEye{light.position}; ///< Place the shadow camera at the point light for the current simple approximation.
			const Scalar lightExtent{static_cast<Scalar>(4.0)}; ///< Light-space half-size covers the 4x4 floor and tall cube.
			const Scalar spotLightFov{std::clamp(spot.outerConeAngle.radians * static_cast<Scalar>(2), static_cast<Scalar>(0.001), static_cast<Scalar>(3.0))}; ///< Spot-light cone field of view.
			const Scalar spotLightFar{std::isfinite(spot.range.value) && spot.range.value > zero() ? spot.range.value : static_cast<Scalar>(0.001)}; ///< Positive spot-light far plane.
			spotLightViewProjCount = std::min(scene.spotLights.size(), spotLightViewProjs.size());
			std::uint32_t activeSpotLightViewProjCount{}; ///< Enabled spot-light count packed into the frame uniform.
			constexpr Scalar kSpotShadowNearPlane{static_cast<Scalar>(0.1)}; ///< Shared spot shadow near plane.
			constexpr float kSpotShadowCompareBias{0.001F}; ///< CPU mirror of the shader-side compare bias.
			constexpr std::size_t kPointShadowFaceCount{6U}; ///< One square shadow view per cubemap direction.
			const std::size_t pointLightShadowCount{std::min(scene.pointLights.size(), kMaxShadowedPointLights)}; ///< Clamped point-light count for CPU metadata.
			shadowLightMeta.clear(); ///< Rebuild CPU metadata from the active scene lights each frame.
			shadowLightMeta.reserve(spotLightViewProjCount + pointLightShadowCount * kPointShadowFaceCount); ///< Reserve all spot and point rows.
			const Vec4 spotLightPositionRange{spot.position.x, spot.position.y, spot.position.z, spot.range.value}; ///< Spot xyz position with range in w.
			const Vec4 spotLightColorIntensity{spot.color.x, spot.color.y, spot.color.z, spot.intensity.value}; ///< Spot rgb color with intensity in w.
			const Vec4 spotLightDirection{spotDirection.x, spotDirection.y, spotDirection.z, zero()}; ///< Spot xyz direction with unused w.
			Vec4 spotLightConeAmbient{std::cos(spot.innerConeAngle.radians), std::cos(spot.outerConeAngle.radians), zero(), spot.ambient}; ///< Spot x inner cosine, y outer cosine, z active count, w ambient.
			std::array<Vec4, kMaxShadowedSpotLights> spotLightPositionRanges{}; ///< Per-spot xyz positions with ranges in w.
			std::array<Vec4, kMaxShadowedSpotLights> spotLightColorIntensities{}; ///< Per-spot rgb colors with intensities in w.
			std::array<Vec4, kMaxShadowedSpotLights> spotLightDirections{}; ///< Per-spot xyz directions with unused w.
			std::array<Vec4, kMaxShadowedSpotLights> spotLightConeAmbients{}; ///< Per-spot cone cosines, active count, and ambient.
			for (std::size_t spotIndex{}; spotIndex < spotLightViewProjCount; ++spotIndex) {
				const SpotLight &activeSpot = scene.spotLights[spotIndex]; ///< Scene spot light copied into a future shadow slot.
				if (!activeSpot.enabled) { continue; }
				const std::size_t packedSpotIndex{activeSpotLightViewProjCount++}; ///< Dense shader slot for this enabled spot light.
				const Vec3 activeSpotDirection{normalize(activeSpot.direction)}; ///< Normalized direction mirrors the legacy single-light path.
				const Scalar activeSpotLightFov{std::clamp(activeSpot.outerConeAngle.radians * static_cast<Scalar>(2), static_cast<Scalar>(0.001), static_cast<Scalar>(3.0))}; ///< Spot-light cone field of view.
				const Scalar activeSpotLightFar{std::isfinite(activeSpot.range.value) && activeSpot.range.value > zero() ? activeSpot.range.value : static_cast<Scalar>(0.001)}; ///< Positive spot-light far plane.
				const Mat4 activeSpotView{lookAt(activeSpot.position, add(activeSpot.position, activeSpotDirection), Vec3{zero(), one(), zero()})}; ///< Light-space view for this spot slot.
				const Mat4 activeSpotProjection{perspectiveVulkan(activeSpotLightFov, one(), kSpotShadowNearPlane, activeSpotLightFar)}; ///< Light-space projection for this spot slot.
				spotLightViewProjs[packedSpotIndex] = multiply(activeSpotProjection, activeSpotView);
				shadowLightMeta.push_back(ShadowLightMeta{.light_index = static_cast<std::uint32_t>(packedSpotIndex),
																		 .light_type = 1U,
																		 .shadow_slot = static_cast<std::uint32_t>(packedSpotIndex),
																		 .first_layer = static_cast<std::uint32_t>(packedSpotIndex),
																		 .layer_count = 1U,
																		 .view = activeSpotView,
																		 .projection = activeSpotProjection,
																		 .near_plane = kSpotShadowNearPlane,
																		 .far_plane = activeSpotLightFar,
																		 .depth_bias = static_cast<Scalar>(kSpotShadowCompareBias),
																		 .resolution = ShadowMap::resolution}); ///< One spot light maps to one array layer.
				spotLightPositionRanges[packedSpotIndex] = Vec4{activeSpot.position.x, activeSpot.position.y, activeSpot.position.z, activeSpot.range.value};
				spotLightColorIntensities[packedSpotIndex] = Vec4{activeSpot.color.x, activeSpot.color.y, activeSpot.color.z, activeSpot.intensity.value};
				spotLightDirections[packedSpotIndex] = Vec4{activeSpotDirection.x, activeSpotDirection.y, activeSpotDirection.z, zero()};
				spotLightConeAmbients[packedSpotIndex] = Vec4{std::cos(activeSpot.innerConeAngle.radians), std::cos(activeSpot.outerConeAngle.radians), zero(), activeSpot.ambient};
			}
			spotLightConeAmbient.z = static_cast<Scalar>(activeSpotLightViewProjCount);
			for (std::size_t spotIndex{}; spotIndex < activeSpotLightViewProjCount; ++spotIndex) { spotLightConeAmbients[spotIndex].z = static_cast<Scalar>(activeSpotLightViewProjCount); }
			constexpr std::array<Vec3, kPointShadowFaceCount> pointShadowFaceDirections{Vec3{one(), zero(), zero()},
																																									Vec3{-one(), zero(), zero()},
																																									Vec3{zero(), one(), zero()},
																																									Vec3{zero(), -one(), zero()},
																																									Vec3{zero(), zero(), one()},
																																									Vec3{zero(), zero(), -one()}}; ///< Cubemap face forward vectors.
			constexpr std::array<Vec3, kPointShadowFaceCount> pointShadowFaceUps{Vec3{zero(), -one(), zero()},
																																		 Vec3{zero(), -one(), zero()},
																																		 Vec3{zero(), zero(), one()},
																																		 Vec3{zero(), zero(), -one()},
																																		 Vec3{zero(), -one(), zero()},
																																		 Vec3{zero(), -one(), zero()}}; ///< Cubemap face up vectors.
			const std::uint32_t firstPointShadowLayer{static_cast<std::uint32_t>(kMaxShadowedSpotLights)}; ///< Point layers start after all spot slots.
			constexpr Scalar kPointShadowFov{static_cast<Scalar>(1.5707963267948966)}; ///< Square cubemap face field of view.
			std::size_t activePointLightCount{}; ///< Enabled point-light count packed into shader-visible arrays.
			// Append six independent light-space views for every shadowed point light.
			for (std::size_t pointIndex{}; pointIndex < pointLightShadowCount; ++pointIndex) {
				const PointLight &activePoint = scene.pointLights[pointIndex]; ///< Scene point light copied into six shadow faces.
				if (!activePoint.enabled) { continue; }
				const std::size_t packedPointIndex{activePointLightCount++}; ///< Dense shader slot for this enabled point light.
				const Scalar activePointLightFar{std::isfinite(activePoint.range) && activePoint.range > zero() ? activePoint.range : static_cast<Scalar>(0.001)}; ///< Positive point-light far plane.
				const Mat4 activePointProjection{perspectiveVulkan(kPointShadowFov, one(), kSpotShadowNearPlane, activePointLightFar)}; ///< Shared square projection for all faces.
				for (std::size_t faceIndex{}; faceIndex < kPointShadowFaceCount; ++faceIndex) {
					const Vec3 faceTarget{add(activePoint.position, pointShadowFaceDirections[faceIndex])}; ///< Face center in world space.
					const Mat4 activePointView{lookAt(activePoint.position, faceTarget, pointShadowFaceUps[faceIndex])}; ///< Face-specific point-light view.
					shadowLightMeta.push_back(ShadowLightMeta{.light_index = static_cast<std::uint32_t>(packedPointIndex),
																			 .light_type = 2U,
																			 .shadow_slot = static_cast<std::uint32_t>(packedPointIndex),
																			 .first_layer = firstPointShadowLayer + static_cast<std::uint32_t>(packedPointIndex * kPointShadowFaceCount + faceIndex),
																			 .layer_count = 1U,
																			 .view = activePointView,
																			 .projection = activePointProjection,
																			 .near_plane = kSpotShadowNearPlane,
																			 .far_plane = activePointLightFar,
																			 .depth_bias = static_cast<Scalar>(kSpotShadowCompareBias),
																			 .resolution = ShadowMap::resolution}); ///< One point face maps to one array layer.
				}
			}
			std::array<Mat4, kMaxShadowedPointLights * kPointShadowFaceCount> pointLightFaceViewProjs{}; ///< Per-point-face matrices uploaded for future point shadows.
			std::array<Vec4, kMaxShadowedPointLights> pointLightPositionRanges{}; ///< Per-point xyz positions with ranges in w.
			std::array<Vec4, kMaxShadowedPointLights> pointLightColorIntensities{}; ///< Per-point rgb colors with intensities in w.
			// Copy clamped point-light properties into the fixed shader-visible arrays.
			std::size_t pointUniformIndex{}; ///< Dense uniform slot for enabled point lights.
			for (std::size_t pointIndex{}; pointIndex < pointLightShadowCount; ++pointIndex) {
				const PointLight &activePoint = scene.pointLights[pointIndex]; ///< Scene point light copied into the frame uniform slot.
				if (!activePoint.enabled) { continue; }
				pointLightPositionRanges[pointUniformIndex] = Vec4{activePoint.position.x, activePoint.position.y, activePoint.position.z, activePoint.range};
				pointLightColorIntensities[pointUniformIndex] = Vec4{activePoint.color.x, activePoint.color.y, activePoint.color.z, activePoint.intensity};
				++pointUniformIndex;
			}
			// Mirror point-light metadata rows into the GPU uniform without rebuilding views or projections.
			for (const ShadowLightMeta &shadowMeta : shadowLightMeta) {
				if (shadowMeta.light_type != 2U || shadowMeta.first_layer < firstPointShadowLayer) { continue; }
				const std::uint32_t pointFaceIndex{shadowMeta.first_layer - firstPointShadowLayer}; ///< Dense point-face array index derived from the metadata layer.
				if (pointFaceIndex < pointLightFaceViewProjs.size()) { pointLightFaceViewProjs[pointFaceIndex] = multiply(shadowMeta.projection, shadowMeta.view); }
			}
			const std::size_t directionalLightCount{std::min(scene.directionalLights.size(), kMaxDirectionalLights)}; ///< Clamped directional-light count fits the fixed uniform arrays.
			std::uint32_t activeDirectionalLightCount{}; ///< Enabled directional-light count packed into the frame uniform.
			std::array<Mat4, kMaxDirectionalLights> dirLightViewProjArray{}; ///< Per-directional shadow matrices used by depth passes and sampling.
			std::array<Vec4, kMaxDirectionalLights> directionalLightDirections{}; ///< Per-directional xyz directions with unused w.
			std::array<Vec4, kMaxDirectionalLights> directionalLightColorIntensities{}; ///< Per-directional rgb colors with intensities in w.
			std::array<Vec4, kMaxDirectionalLights> directionalLightAmbients{}; ///< Per-directional active count and ambient.
			// Build one directional light-space matrix per active light.
			for (std::size_t directionalIndex{}; directionalIndex < directionalLightCount; ++directionalIndex) {
				const DirectionalLight &activeDirectional = scene.directionalLights[directionalIndex]; ///< Scene directional light copied into a fixed uniform slot.
				if (!activeDirectional.enabled) { continue; }
				const std::size_t packedDirectionalIndex{activeDirectionalLightCount++}; ///< Dense shader slot for this enabled directional light.
				const Vec3 activeDirectionalDirection{normalize(activeDirectional.direction)}; ///< Normalized direction mirrors the legacy single-light path.
				const Vec3 activeDirectionalEye{subtract(lightCenter, scale(activeDirectionalDirection, lightExtent))}; ///< Directional shadow camera aimed at the scene origin.
				dirLightViewProjArray[packedDirectionalIndex] = multiply(orthoVulkan(-lightExtent, lightExtent, -lightExtent, lightExtent, static_cast<Scalar>(0.1), static_cast<Scalar>(16.0)), lookAt(activeDirectionalEye, lightCenter, Vec3{zero(), one(), zero()}));
				directionalLightDirections[packedDirectionalIndex] = Vec4{activeDirectionalDirection.x, activeDirectionalDirection.y, activeDirectionalDirection.z, zero()};
				directionalLightColorIntensities[packedDirectionalIndex] = Vec4{activeDirectional.color.x, activeDirectional.color.y, activeDirectional.color.z, activeDirectional.intensity.value};
				directionalLightAmbients[packedDirectionalIndex] = Vec4{zero(), zero(), zero(), activeDirectional.ambient};
			}
			for (std::size_t directionalIndex{}; directionalIndex < activeDirectionalLightCount; ++directionalIndex) { directionalLightAmbients[directionalIndex].z = static_cast<Scalar>(activeDirectionalLightCount); }
			spotShadowDepthSampleCountStorage() = spotLightViewProjCount;
			const Vec3 spotShadowDebugPoint{zero(), zero(), zero()}; ///< Fixed world point used for CPU-only shadow diagnostics.
			constexpr float kCpuSpotShadowFactor{1.0F}; ///< Unshadowed placeholder because has_gpu is false here.
			pointShadowDepthSampleCountStorage() = pointLightShadowCount;
			constexpr float kCpuPointShadowFactor{1.0F}; ///< Unshadowed placeholder because has_gpu is false here.
			// Keep one CPU-only point sample per active point light using the shader's dominant-axis face order.
			for (std::size_t pointIndex{}; pointIndex < pointLightShadowCount; ++pointIndex) {
				const PointLight &activePoint = scene.pointLights[pointIndex]; ///< Source point light for this diagnostic slot.
				const Vec3 lightToPoint{subtract(spotShadowDebugPoint, activePoint.position)}; ///< Shader-matching vector from light to sample.
				const Vec3 absLightToPoint{std::abs(lightToPoint.x), std::abs(lightToPoint.y), std::abs(lightToPoint.z)}; ///< Dominant-axis selector inputs.
				const std::uint32_t faceIndex{absLightToPoint.x >= absLightToPoint.y && absLightToPoint.x >= absLightToPoint.z
																	? (lightToPoint.x >= zero() ? 0U : 1U)
																	: (absLightToPoint.y >= absLightToPoint.z ? (lightToPoint.y >= zero() ? 2U : 3U)
																													  : (lightToPoint.z >= zero() ? 4U : 5U))}; ///< Face order: +X, -X, +Y, -Y, +Z, -Z.
				const std::uint32_t pointFaceLayer{static_cast<std::uint32_t>(pointIndex * kPointShadowFaceCount + faceIndex)}; ///< Dense point-face uniform index.
				const std::uint32_t selectedLayer{firstPointShadowLayer + pointFaceLayer}; ///< Dense depth-array layer after spot slots.
				const Vec4 lightClip{multiply(pointLightFaceViewProjs[pointFaceLayer], Vec4{spotShadowDebugPoint.x, spotShadowDebugPoint.y, spotShadowDebugPoint.z, one()})}; ///< Clip point before perspective divide.
				const Scalar invW{std::abs(lightClip.w) > static_cast<Scalar>(0.001) ? one() / lightClip.w : static_cast<Scalar>(1000.0)}; ///< Shader-equivalent small-w guard.
				const Vec3 lightNdc{lightClip.x * invW, lightClip.y * invW, lightClip.z * invW}; ///< Point-face NDC point used for CPU comparison.
				pointShadowDepthSampleStorage()[pointIndex] = RenderShadowDepthSample{.triangle_id = static_cast<std::uint32_t>(pointIndex),
																									  .face_index = faceIndex,
																									  .world = spotShadowDebugPoint,
																									  .light_ndc = lightNdc,
																									  .pixel_x = selectedLayer,
																									  .expected_depth = lightNdc.z,
																									  .bias = kSpotShadowCompareBias,
																									  .shadow_factor = kCpuPointShadowFactor,
																									  .gpu_depth = -1.0F,
																									  .error = -1.0F,
																									  .has_gpu = false,
																									  .valid = true}; ///< Slot is triangle_id; layer is pixel_x until point readback exists.
			}
			for (std::size_t spotIndex{}; spotIndex < spotLightViewProjCount; ++spotIndex) {
				const Vec4 lightClip{multiply(spotLightViewProjs[spotIndex], Vec4{spotShadowDebugPoint.x, spotShadowDebugPoint.y, spotShadowDebugPoint.z, one()})}; ///< Clip point before perspective divide.
				const Scalar invW{lightClip.w != zero() ? one() / lightClip.w : zero()}; ///< Zero-w guard keeps invalid clips explicit.
				const Vec3 lightNdc{lightClip.x * invW, lightClip.y * invW, lightClip.z * invW}; ///< Shader-comparable NDC point.
				spotShadowDepthSampleStorage()[spotIndex] = RenderShadowDepthSample{.face_index = static_cast<std::uint32_t>(spotIndex),
																								 .world = spotShadowDebugPoint,
																								 .light_ndc = lightNdc,
																								 .expected_depth = lightNdc.z,
																								 .bias = kSpotShadowCompareBias,
																								 .shadow_factor = kCpuSpotShadowFactor,
																								 .gpu_depth = -1.0F,
																								 .error = -1.0F,
																								 .has_gpu = false,
																								 .valid = true}; ///< Slot is face_index; GPU depth is unavailable here.
			}
			if (activeSpotLightViewProjCount == 0U) {
				spotLightViewProjs[0] = multiply(perspectiveVulkan(spotLightFov, one(), kSpotShadowNearPlane, spotLightFar), lookAt(spot.position, add(spot.position, spotDirection), Vec3{zero(), one(), zero()})); ///< Legacy empty-vector fallback.
			}
			const FrameUniforms frameUniforms{ // Shared camera matrices keep the sample cubes inside Vulkan clip space.
				.view = lookAt(cameraEye, cameraTarget, Vec3{zero(), one(), zero()}),
				.projection = perspectiveVulkan(static_cast<Scalar>(0.7853981633974483), aspectRatio, static_cast<Scalar>(0.1), static_cast<Scalar>(100.0)),
				.lightViewProj = multiply(orthoVulkan(-lightExtent, lightExtent, -lightExtent, lightExtent, static_cast<Scalar>(0.1), static_cast<Scalar>(16.0)), lookAt(lightEye, lightCenter, Vec3{zero(), one(), zero()})),
				.dirLightViewProjArray = dirLightViewProjArray, ///< Per-directional matrices used by depth passes and fragment sampling.
				.spotLightViewProjs = spotLightViewProjs,
				.pointLightFaceViewProjs = pointLightFaceViewProjs, ///< Point-face matrices are sourced from shadow metadata rows.
				.pointLightPositionRanges = pointLightPositionRanges,
				.pointLightColorIntensities = pointLightColorIntensities,
				.lightPositionRange = Vec4{light.position.x, light.position.y, light.position.z, light.range},
				.lightColorIntensity = Vec4{light.color.x, light.color.y, light.color.z, light.intensity},
				.lightShadowAmbient = Vec4{shadowSurfaceLightDir.x, shadowSurfaceLightDir.y, shadowSurfaceLightDir.z, light.ambient},
				.dirLightDirection = Vec4{dirLightDirection.x, dirLightDirection.y, dirLightDirection.z, zero()}, ///< Directional light vector with unused w.
				.dirLightColorIntensity = Vec4{dirLight.color.x, dirLight.color.y, dirLight.color.z, dirLight.intensity.value}, ///< Directional tint with intensity in w.
				.dirLightShadowAmbient = Vec4{zero(), zero(), zero(), dirLight.ambient}, ///< Directional ambient term packed in w.
				.spotLightPositionRange = spotLightPositionRange,
				.spotLightColorIntensity = spotLightColorIntensity,
				.spotLightDirection = spotLightDirection,
				.spotLightConeAmbient = spotLightConeAmbient,
				.spotLightPositionRanges = spotLightPositionRanges,
				.spotLightColorIntensities = spotLightColorIntensities,
				.spotLightDirections = spotLightDirections,
				.spotLightConeAmbients = spotLightConeAmbients,
				.directionalLightDirections = directionalLightDirections,
				.directionalLightColorIntensities = directionalLightColorIntensities,
				.directionalLightAmbients = directionalLightAmbients,
				.activeDirectionalLightCount = activeDirectionalLightCount,
			};
			directionalShadowDepthSampleCountStorage() = 1U;
			const Vec3 directionalShadowDebugPoint{zero(), zero(), zero()}; ///< Fixed world point shared with spot shadow diagnostics.
			constexpr float kDirectionalShadowCompareBias{0.001F}; ///< CPU mirror of the shader-side directional compare bias.
			const Vec4 dirLightClip{multiply(frameUniforms.dirLightViewProjArray[0], Vec4{directionalShadowDebugPoint.x, directionalShadowDebugPoint.y, directionalShadowDebugPoint.z, one()})}; ///< Clip point before perspective divide.
			const Scalar dirInvW{dirLightClip.w != zero() ? one() / dirLightClip.w : zero()}; ///< Zero-w guard matches the spot debug path.
			const Vec3 dirLightNdc{dirLightClip.x * dirInvW, dirLightClip.y * dirInvW, dirLightClip.z * dirInvW}; ///< Shader-comparable directional NDC point.
			directionalShadowDepthSampleStorage()[0] = RenderShadowDepthSample{.face_index = 0U,
																									 .world = directionalShadowDebugPoint,
																									 .light_ndc = dirLightNdc,
																									 .expected_depth = dirLightNdc.z,
																									 .bias = kDirectionalShadowCompareBias,
																									 .shadow_factor = 1.0F,
																									 .gpu_depth = -1.0F,
																									 .error = -1.0F,
																									 .has_gpu = false,
																									 .valid = true}; ///< Directional sample is CPU-only until readback is added.
			result = uniformBuffers.update(currentFrame, frameUniforms);
			if (result != VK_SUCCESS) { return; }

			result = recordCommandBuffer(currentFrame, imageIndex);
			if (result != VK_SUCCESS) { return; }

			const VkPipelineStageFlags waitStage{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
			const VkSubmitInfo submitInfo{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = 1U,
				.pWaitSemaphores = &imageAvailableSemaphore,
				.pWaitDstStageMask = &waitStage,
				.commandBufferCount = 1U,
				.pCommandBuffers = &commandBuffers.commandBuffers[currentFrame],
				.signalSemaphoreCount = 1U,
				.pSignalSemaphores = &renderFinishedSemaphore,
			};
			result = vkQueueSubmit(device.graphicsQueue, 1U, &submitInfo, inFlightFence);
			if (result != VK_SUCCESS) { return; }
			if (spotLightViewProjCount != 0U) { fillSpotShadowGpuDepthSamples(); }
			if (directionalShadowDepthSampleCountStorage() != 0U) { fillDirectionalShadowGpuDepthSamples(); }
			if (pointShadowDepthSampleCountStorage() != 0U) { fillPointShadowGpuDepthSamples(); }
			if (readback != nullptr && imageIndex < swapchain.images.size()) {
				lastReadbackCaptureResult = readback->capture(swapchain.images[imageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			}

			const VkPresentInfoKHR presentInfo{
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.waitSemaphoreCount = 1U,
				.pWaitSemaphores = &renderFinishedSemaphore,
				.swapchainCount = 1U,
				.pSwapchains = &swapchain.swapchain,
				.pImageIndices = &imageIndex,
			};
			result = vkQueuePresentKHR(device.presentQueue, &presentInfo);
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
				(void)recreateSwapchain(currentWindowPixelExtent());
				return;
			}
			if (result != VK_SUCCESS) { return; }

			lastRenderedImageIndex = imageIndex;
			currentFrame = static_cast<std::uint32_t>((currentFrame + 1U) % frameCount);
		}

		/// @brief Common frame-render entry forwarding to the concrete Vulkan draw path.
		void renderFrame(VulkanReadback *readback = nullptr) { drawFrame(readback); }

		/// @brief Copies the last presented swapchain image and writes it as a deterministic PNG.
		[[nodiscard]] auto captureFrameToPng(const std::filesystem::path &output_path) -> std::expected<void, Error> {
			if (!lastRenderedImageIndex) { return std::unexpected(Error::missing_object); }
			if (*lastRenderedImageIndex >= swapchain.images.size()) {
				return std::unexpected(Error::internal_error);
			}

			if (const auto parent = output_path.parent_path(); !parent.empty()) {
				auto error = std::error_code{};
				std::filesystem::create_directories(parent, error);
				if (error) { return std::unexpected(Error::io_error); }
			}

			auto readback = VulkanReadback{};
			VkResult result = readback.create(physicalDevice.physicalDevice, device.device,
														 device.graphicsQueue, commandPool.commandPool,
														 swapchain.extent, swapchain.imageFormat);
			if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }

			result = vkDeviceWaitIdle(device.device);
			if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }
			const auto image = swapchain.images[*lastRenderedImageIndex];
			result = readback.capture(image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }

			const auto output = output_path.string();
			if (!writeReadbackPng(readback.pixelBytes(), swapchain.extent, swapchain.imageFormat, output)) {
				return std::unexpected(Error::io_error);
			}
			return {};
		}

		/**
		* @brief Releases Vulkan device resources in reverse creation order.
			*/
		void cleanup() {
			if (device.device != VK_NULL_HANDLE) { (void)vkDeviceWaitIdle(device.device); }
			recordedPassOrder.clear();
			for (auto mesh = meshes.rbegin(); mesh != meshes.rend(); ++mesh) { mesh->cleanup(); }
			meshes.clear();
			objectTexture.cleanup();
			defaultObjectTexture.cleanup();
			descriptorSets.cleanup();
			if (imguiDescriptorPool_ != VK_NULL_HANDLE) {
				vkDestroyDescriptorPool(device.device, imguiDescriptorPool_, nullptr);
				imguiDescriptorPool_ = VK_NULL_HANDLE;
			}
			descriptorPool.cleanup();
			uniformBuffers.cleanup();
			frameSync.cleanup();
			commandBuffers.cleanup();
			pointShadowDepthReadback.cleanup();
			dirShadowDepthReadback.cleanup();
			spotShadowDepthReadback.cleanup();
			commandPool.cleanup();
			graphicsPipeline.cleanup();
			fragShaderModule.cleanup();
			vertShaderModule.cleanup();
			spotShadowMap.cleanupPipeline();
			shadowMap.cleanupPipeline();
			pipelineLayout.cleanup();
			descriptorSetLayout.cleanup();
			framebuffers.cleanup();
			renderPass.cleanup();
			pointShadowArray.cleanup();
			spotShadowArray.cleanup();
			spotShadowMap.cleanup();
			dirShadowArray.cleanup();
			shadowMap.cleanup();
			depthImage.cleanup();
			imageViews.cleanup();
			swapchain.cleanup();
			device.cleanup();
			surface.cleanup();
			instance.cleanup();
			window = nullptr;
		}

	private:
		std::uint32_t currentFrame{0U}; ///< Index of the frame synchronization set used by the next draw.
		VkDescriptorPool imguiDescriptorPool_{VK_NULL_HANDLE}; ///< Owned Dear ImGui descriptor pool reserved for backend texture descriptors.
		void *guiSystem_{nullptr}; ///< Non-owning, type-erased GUI system pointer reserved for later GUI integration.
		std::function<void(VkCommandBuffer)> guiRecord_; ///< Optional GUI recorder invoked during the forward color pass.

		[[nodiscard]] static std::array<RenderShadowDepthSample, 1U> &directionalShadowDepthSampleStorage() {
			static std::array<RenderShadowDepthSample, 1U> samples{}; ///< CPU-only directional shadow sample.
			return samples;
		}

		[[nodiscard]] static std::size_t &directionalShadowDepthSampleCountStorage() {
			static std::size_t count{}; ///< Number of valid CPU-only directional shadow samples.
			return count;
		}

		[[nodiscard]] static std::array<RenderShadowDepthSample, kMaxShadowedSpotLights> &spotShadowDepthSampleStorage() {
			static std::array<RenderShadowDepthSample, kMaxShadowedSpotLights> samples{}; ///< CPU-only spot shadow samples.
			return samples;
		}

		[[nodiscard]] static std::size_t &spotShadowDepthSampleCountStorage() {
			static std::size_t count{}; ///< Number of valid CPU-only spot shadow samples.
			return count;
		}

		[[nodiscard]] static std::array<RenderShadowDepthSample, kMaxShadowedPointLights> &pointShadowDepthSampleStorage() {
			static std::array<RenderShadowDepthSample, kMaxShadowedPointLights> samples{}; ///< CPU-only point shadow samples.
			return samples;
		}

		[[nodiscard]] static std::size_t &pointShadowDepthSampleCountStorage() {
			static std::size_t count{}; ///< Number of valid CPU-only point shadow samples.
			return count;
		}

		/**
			* @brief Reads one rendered spot shadow texel per active debug sample into the retained diagnostics.
			*/
		void fillSpotShadowGpuDepthSamples() {
			if (device.device == VK_NULL_HANDLE || spotShadowArray.image == VK_NULL_HANDLE) { return; }
			if (spotShadowDepthReadback.extent.width == 0U || spotShadowDepthReadback.extent.height == 0U) { return; }
			if (vkDeviceWaitIdle(device.device) != VK_SUCCESS) { return; }

			const std::size_t sampleCount{std::min({spotLightViewProjCount, spotShadowDepthSampleCountStorage(), spotShadowArray.layerFramebuffers.size()})}; // Only rendered spot slots are read back.
			for (std::size_t spotIndex{}; spotIndex < sampleCount; ++spotIndex) {
				RenderShadowDepthSample &sample = spotShadowDepthSampleStorage()[spotIndex]; // Existing CPU sample owns NDC and expected depth.
				const auto texel = spotShadowDebugTexel(sample.light_ndc);
				if (spotShadowDepthReadback.capture(spotShadowArray.image, static_cast<std::uint32_t>(spotIndex)) != VK_SUCCESS) { continue; }

				const std::optional<float> depth = spotShadowDepthReadback.depthAt(texel.first, texel.second);
				if (!depth) { continue; }
				sample.pixel_x = texel.first;
				sample.pixel_y = texel.second;
				sample.gpu_depth = *depth;
				sample.has_gpu = true;
				sample.error = std::abs(sample.gpu_depth - sample.expected_depth);
			}
		}

		/**
			* @brief Reads the rendered directional shadow texel into the retained diagnostics.
			*/
		void fillDirectionalShadowGpuDepthSamples() {
			if (device.device == VK_NULL_HANDLE || dirShadowArray.image == VK_NULL_HANDLE) { return; }
			if (dirShadowDepthReadback.extent.width == 0U || dirShadowDepthReadback.extent.height == 0U) { return; }
			if (vkDeviceWaitIdle(device.device) != VK_SUCCESS) { return; }

			RenderShadowDepthSample &sample = directionalShadowDepthSampleStorage()[0]; // Single directional light uses layer zero.
			const auto texel = spotShadowDebugTexel(sample.light_ndc);
			if (dirShadowDepthReadback.capture(dirShadowArray.image, 0U) != VK_SUCCESS) { return; }

			const std::optional<float> depth = dirShadowDepthReadback.depthAt(texel.first, texel.second);
			if (!depth) { return; }
			sample.pixel_x = texel.first;
			sample.pixel_y = texel.second;
			sample.gpu_depth = *depth;
			sample.has_gpu = true;
			sample.error = std::abs(sample.gpu_depth - sample.expected_depth);
		}

		/**
			* @brief Reads one rendered point shadow texel per active debug sample into the retained diagnostics.
			*/
		void fillPointShadowGpuDepthSamples() {
			if (device.device == VK_NULL_HANDLE || pointShadowArray.image == VK_NULL_HANDLE) { return; }
			if (pointShadowDepthReadback.extent.width == 0U || pointShadowDepthReadback.extent.height == 0U) { return; }
			if (vkDeviceWaitIdle(device.device) != VK_SUCCESS) { return; }

			constexpr std::uint32_t firstPointShadowLayer{static_cast<std::uint32_t>(kMaxShadowedSpotLights)}; // CPU samples store the combined spot-plus-point layer.
			const std::size_t sampleCount{std::min(pointShadowDepthSampleCountStorage(), pointShadowArray.layerFramebuffers.size())}; // Only retained point-light samples are read.
			for (std::size_t pointIndex{}; pointIndex < sampleCount; ++pointIndex) {
				RenderShadowDepthSample &sample = pointShadowDepthSampleStorage()[pointIndex]; // Existing CPU sample owns selected face, layer, and NDC.
				if (sample.pixel_x < firstPointShadowLayer) { continue; }
				const std::uint32_t pointArrayLayer{sample.pixel_x - firstPointShadowLayer}; // Point texture array stores only point faces.
				if (pointArrayLayer >= pointShadowArray.layerFramebuffers.size()) { continue; }

				const auto texel = spotShadowDebugTexel(sample.light_ndc);
				if (pointShadowDepthReadback.capture(pointShadowArray.image, pointArrayLayer) != VK_SUCCESS) { continue; }

				const std::optional<float> depth = pointShadowDepthReadback.depthAt(texel.first, texel.second);
				if (!depth) { continue; }
				sample.pixel_y = texel.second;
				sample.gpu_depth = *depth;
				sample.error = sample.expected_depth - sample.gpu_depth;
				sample.has_gpu = true;
				sample.shadow_factor = sample.light_ndc.z - sample.bias > sample.gpu_depth ? 0.35F : 1.0F;
			}
		}

		/**
			* @brief Converts shader shadow-map NDC coordinates to one clamped integer texel.
			*
			* @param lightNdc Existing debug point in light normalized device coordinates.
			* @return X and Y texel coordinates inside the fixed shadow-map resolution.
			*/
		[[nodiscard]] static std::pair<std::uint32_t, std::uint32_t> spotShadowDebugTexel(Vec3 lightNdc) {
			const auto toTexel = [](Scalar ndc) {
				if (!std::isfinite(ndc)) { return 0U; }
				const Scalar uv = std::clamp(ndc * static_cast<Scalar>(0.5) + static_cast<Scalar>(0.5), zero(), one());
				const Scalar scaled = uv * static_cast<Scalar>(ShadowMap::resolution);
				return static_cast<std::uint32_t>(std::min<std::uint64_t>(static_cast<std::uint64_t>(scaled), ShadowMap::resolution - 1U));
			};
			return {toTexel(lightNdc.x), toTexel(lightNdc.y)};
		}

		[[nodiscard]] VkExtent2D currentWindowPixelExtent() const {
			if (window == nullptr) { return {}; }
			int width{};
			int height{};
			SDL_GetWindowSizeInPixels(window, &width, &height);
			return VkExtent2D{
				.width = static_cast<std::uint32_t>(std::max(width, 0)),
				.height = static_cast<std::uint32_t>(std::max(height, 0)),
			};
		}

		[[nodiscard]] VkResult recreateSwapchain(VkExtent2D requestedExtent) {
			if (requestedExtent.width == 0U || requestedExtent.height == 0U) { return VK_NOT_READY; }
			if (physicalDevice.physicalDevice == VK_NULL_HANDLE || device.device == VK_NULL_HANDLE ||
				 surface.surface == VK_NULL_HANDLE || !physicalDevice.graphicsQueueFamily ||
				 !physicalDevice.presentQueueFamily || vertShaderModule.shaderModule == VK_NULL_HANDLE ||
				 fragShaderModule.shaderModule == VK_NULL_HANDLE || pipelineLayout.pipelineLayout == VK_NULL_HANDLE) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			VkResult result = vkDeviceWaitIdle(device.device);
			if (result != VK_SUCCESS) { return result; }

			graphicsPipeline.cleanup();
			framebuffers.cleanup();
			renderPass.cleanup();
			depthImage.cleanup();
			imageViews.cleanup();
			frameSync.cleanup();
			swapchain.cleanup();

			result = swapchain.create(physicalDevice.physicalDevice, device.device, surface.surface,
											  *physicalDevice.graphicsQueueFamily, *physicalDevice.presentQueueFamily,
											  requestedExtent.width, requestedExtent.height);
			if (result != VK_SUCCESS) { return result; }

			result = imageViews.create(device.device, swapchain.images, swapchain.imageFormat);
			if (result != VK_SUCCESS) { return result; }

			result = depthImage.create(physicalDevice.physicalDevice, device.device, swapchain.extent);
			if (result != VK_SUCCESS) { return result; }

			result = renderPass.create(device.device, swapchain.imageFormat);
			if (result != VK_SUCCESS) { return result; }

			result = framebuffers.create(device.device, renderPass.renderPass, imageViews.views, swapchain.extent, depthImage.view);
			if (result != VK_SUCCESS) { return result; }

			VulkanVertexInputDescription vertexInput{};
			result = graphicsPipeline.create(device.device, renderPass.renderPass, pipelineLayout.pipelineLayout,
													 vertShaderModule.shaderModule, fragShaderModule.shaderModule, vertexInput,
													 swapchain.extent);
			if (result != VK_SUCCESS) { return result; }

			result = frameSync.create(device.device, framesInFlight, static_cast<std::uint32_t>(swapchain.images.size()));
			if (result != VK_SUCCESS) { return result; }

			currentFrame = 0U;
			lastRenderedImageIndex.reset();
			return VK_SUCCESS;
		}

		/// @brief Creates the dedicated Dear ImGui descriptor pool when the Vulkan device is available.
		void createImguiDescriptorPool() {
			if (imguiDescriptorPool_ != VK_NULL_HANDLE || device.device == VK_NULL_HANDLE) { return; }

			constexpr std::uint32_t maxSets{16U}; // Small immediate-mode UI pool kept separate from renderer descriptors.
			const VkDescriptorPoolSize poolSize{
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = maxSets,
			};
			const VkDescriptorPoolCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
				.maxSets = maxSets,
				.poolSizeCount = 1U,
				.pPoolSizes = &poolSize,
			};

			if (vkCreateDescriptorPool(device.device, &createInfo, nullptr, &imguiDescriptorPool_) != VK_SUCCESS) {
				imguiDescriptorPool_ = VK_NULL_HANDLE;
			}
		}

	public:
		/// @brief Builds dormant Dear ImGui Vulkan backend data from the renderer-owned Vulkan objects.
		[[nodiscard]] ImGui_ImplVulkan_InitInfo makeImguiInitInfo() const {
			ImGui_ImplVulkan_InitInfo info{};
			info.ApiVersion = VK_API_VERSION_1_4;
			info.Instance = instance.instance;
			info.PhysicalDevice = physicalDevice.physicalDevice;
			info.Device = device.device;
			info.QueueFamily = physicalDevice.graphicsQueueFamily.value_or(0U);
			info.Queue = device.graphicsQueue;
			info.DescriptorPool = imguiDescriptorPool_;
			info.RenderPass = renderPass.renderPass;
			info.MinImageCount = static_cast<std::uint32_t>(swapchain.images.size());
			info.ImageCount = static_cast<std::uint32_t>(swapchain.images.size());
			info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
			return info;
		}

	private:
		/**
			* @brief Records one forward render pass for a swapchain image into a frame command buffer.
			*
			* @param frameIndex Index selecting the per-frame command buffer and descriptor set.
			* @param imageIndex Index selecting the swapchain framebuffer.
			* @return VK_SUCCESS when command recording succeeds, otherwise the first failing Vulkan result.
			*/
		[[nodiscard]] VkResult recordCommandBuffer(std::uint32_t frameIndex, std::uint32_t imageIndex) {
			recordedPassOrder.clear();
			if (frameIndex >= commandBuffers.commandBuffers.size() || frameIndex >= descriptorSets.descriptorSets.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (imageIndex >= framebuffers.framebuffers.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (shadowMap.renderPass == VK_NULL_HANDLE || shadowMap.framebuffer == VK_NULL_HANDLE || shadowMap.pipeline == VK_NULL_HANDLE || shadowMap.pipelineLayout == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (spotShadowMap.renderPass == VK_NULL_HANDLE || spotShadowMap.framebuffer == VK_NULL_HANDLE || spotShadowMap.pipeline == VK_NULL_HANDLE || spotShadowMap.pipelineLayout == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkCommandBuffer commandBuffer{commandBuffers.commandBuffers[frameIndex]};
			VkResult result = vkResetCommandBuffer(commandBuffer, 0U);
			if (result != VK_SUCCESS) { return result; }

			const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
			result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
			if (result != VK_SUCCESS) { return result; }

			const auto drawUploadedObjects = [&](VkPipelineLayout activePipelineLayout, std::uint32_t spotLightIndex = 0U, std::uint32_t dirLightIndex = 0U) {
				std::size_t objectIndex{}; // Meshes and scene objects share submission order.
				for (const VulkanMesh &mesh : meshes) {
					if (objectIndex >= scene.objects.size()) { break; }

					const VkBuffer vertexBuffers[]{mesh.vertexBuffer.buffer};
					const VkDeviceSize offsets[]{0U};
					const Object &object = scene.objects[objectIndex];
					const ObjectPushConstants pushConstants{.model = object.model, .useBaseColorTexture = object.useBaseColorTexture, .spotLightIndex = spotLightIndex, .dirLightIndex = dirLightIndex};
					vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, vertexBuffers, offsets);
					vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.buffer, 0U, VK_INDEX_TYPE_UINT32);
					vkCmdPushConstants(commandBuffer, activePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0U, sizeof(ObjectPushConstants), &pushConstants);
					vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1U, 0U, 0, 0U);
					++objectIndex;
				}
			};
			const VkClearValue shadowClear{.depthStencil = {.depth = 1.0F, .stencil = 0U}}; // Shadow depth starts at the far plane.
			const VkRenderPassBeginInfo shadowPassInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = shadowMap.renderPass,
				.framebuffer = shadowMap.framebuffer,
				.renderArea = {.offset = {0, 0}, .extent = {.width = ShadowMap::resolution, .height = ShadowMap::resolution}},
				.clearValueCount = 1U,
				.pClearValues = &shadowClear,
			};

			const bool legacyDirectionalShadowEnabled{!scene.directionalLights.empty() ? scene.directionalLights.front().enabled : scene.directionalLight.enabled}; // Legacy single shadow map mirrors the first directional light.
			if (legacyDirectionalShadowEnabled) {
				recordedPassOrder.push_back(RecordedPass::directional_shadow);
				vkCmdBeginRenderPass(commandBuffer, &shadowPassInfo, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowMap.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowMap.pipelineLayout, 0U, 1U, &descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
				drawUploadedObjects(shadowMap.pipelineLayout);
				vkCmdEndRenderPass(commandBuffer);
			}

			const std::size_t directionalLightCount{std::min(scene.directionalLights.size(), kMaxDirectionalLights)}; // Clamped directional-light count mirrors frame uniform upload.
			const std::size_t dirShadowArrayLayerCount{dirShadowArray.layerFramebuffers.size()}; // Every sampled directional layer must reach shader-read layout.
			// Clear each allocated directional layer before any draw binds the descriptor set that exposes the whole array.
			for (std::size_t dirLightIndex{}; dirLightIndex < dirShadowArrayLayerCount; ++dirLightIndex) {
				const VkRenderPassBeginInfo dirShadowArrayClearInfo{
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
					.renderPass = dirShadowArray.renderPass,
					.framebuffer = dirShadowArray.layerFramebuffers[dirLightIndex],
					.renderArea = {.offset = {0, 0}, .extent = {.width = ShadowMap::resolution, .height = ShadowMap::resolution}},
					.clearValueCount = 1U,
					.pClearValues = &shadowClear,
				};

				vkCmdBeginRenderPass(commandBuffer, &dirShadowArrayClearInfo, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdEndRenderPass(commandBuffer);
			}

			std::size_t activeDirectionalShadowPassCount{}; // Enabled directional lights are packed into dense shadow layers by drawFrame().
			for (std::size_t dirLightIndex{}; dirLightIndex < directionalLightCount; ++dirLightIndex) { if (scene.directionalLights[dirLightIndex].enabled) { ++activeDirectionalShadowPassCount; } }
			const std::size_t dirShadowArrayPassCount{std::min(activeDirectionalShadowPassCount, dirShadowArrayLayerCount)}; // Active directional array layers receive one geometry depth pass each.
			// Render active directional lights after all sampled array layers have a defined layout.
			for (std::size_t dirLightIndex{}; dirLightIndex < dirShadowArrayPassCount; ++dirLightIndex) {
				const VkRenderPassBeginInfo dirShadowArrayPassInfo{
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
					.renderPass = dirShadowArray.renderPass,
					.framebuffer = dirShadowArray.layerFramebuffers[dirLightIndex],
					.renderArea = {.offset = {0, 0}, .extent = {.width = ShadowMap::resolution, .height = ShadowMap::resolution}},
					.clearValueCount = 1U,
					.pClearValues = &shadowClear,
				};

				recordedPassOrder.push_back(RecordedPass::directional_shadow);
				vkCmdBeginRenderPass(commandBuffer, &dirShadowArrayPassInfo, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, dirShadowArray.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, dirShadowArray.pipelineLayout, 0U, 1U, &descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
				drawUploadedObjects(dirShadowArray.pipelineLayout, 0U, static_cast<std::uint32_t>(dirLightIndex));
				vkCmdEndRenderPass(commandBuffer);
			}

			const VkRenderPassBeginInfo spotShadowPassInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = spotShadowMap.renderPass,
				.framebuffer = spotShadowMap.framebuffer,
				.renderArea = {.offset = {0, 0}, .extent = {.width = ShadowMap::resolution, .height = ShadowMap::resolution}},
				.clearValueCount = 1U,
				.pClearValues = &shadowClear,
			};

			const bool legacySpotShadowEnabled{!scene.spotLights.empty() ? scene.spotLights.front().enabled : scene.spotLight.enabled}; // Legacy single shadow map mirrors the first spot light.
			if (legacySpotShadowEnabled) {
				recordedPassOrder.push_back(RecordedPass::spot_shadow);
				vkCmdBeginRenderPass(commandBuffer, &spotShadowPassInfo, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spotShadowMap.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spotShadowMap.pipelineLayout, 0U, 1U, &descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
				drawUploadedObjects(spotShadowMap.pipelineLayout);
				vkCmdEndRenderPass(commandBuffer);
			}

			const std::size_t spotShadowArrayLayerCount{spotShadowArray.layerFramebuffers.size()}; // Every sampled array layer must reach shader-read layout.
			// Clear each allocated spot layer before any draw binds the descriptor set that exposes the whole array.
			for (std::size_t spotLightIndex{}; spotLightIndex < spotShadowArrayLayerCount; ++spotLightIndex) {
				const VkRenderPassBeginInfo spotShadowArrayClearInfo{
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
					.renderPass = spotShadowArray.renderPass,
					.framebuffer = spotShadowArray.layerFramebuffers[spotLightIndex],
					.renderArea = {.offset = {0, 0}, .extent = {.width = ShadowMap::resolution, .height = ShadowMap::resolution}},
					.clearValueCount = 1U,
					.pClearValues = &shadowClear,
				};

				vkCmdBeginRenderPass(commandBuffer, &spotShadowArrayClearInfo, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdEndRenderPass(commandBuffer);
			}

			std::size_t activeSpotShadowPassCount{}; // Enabled spot lights are packed into dense shadow layers by drawFrame().
			for (std::size_t spotLightIndex{}; spotLightIndex < spotLightViewProjCount; ++spotLightIndex) { if (scene.spotLights[spotLightIndex].enabled) { ++activeSpotShadowPassCount; } }
			const std::size_t spotShadowArrayPassCount{std::min(activeSpotShadowPassCount, spotShadowArrayLayerCount)}; // Active array layers receive one geometry depth pass each.
			// Render active spot lights after all sampled array layers have a defined layout.
			for (std::size_t spotLightIndex{}; spotLightIndex < spotShadowArrayPassCount; ++spotLightIndex) {
				const VkRenderPassBeginInfo spotShadowArrayPassInfo{
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
					.renderPass = spotShadowArray.renderPass,
					.framebuffer = spotShadowArray.layerFramebuffers[spotLightIndex],
					.renderArea = {.offset = {0, 0}, .extent = {.width = ShadowMap::resolution, .height = ShadowMap::resolution}},
					.clearValueCount = 1U,
					.pClearValues = &shadowClear,
				};

				recordedPassOrder.push_back(RecordedPass::spot_shadow);
				vkCmdBeginRenderPass(commandBuffer, &spotShadowArrayPassInfo, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spotShadowArray.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spotShadowArray.pipelineLayout, 0U, 1U, &descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
				drawUploadedObjects(spotShadowArray.pipelineLayout, static_cast<std::uint32_t>(spotLightIndex));
				vkCmdEndRenderPass(commandBuffer);
			}

			const std::size_t pointShadowArrayLayerCount{pointShadowArray.layerFramebuffers.size()}; // Six point-shadow faces per light must reach shader-read layout.
			// Clear each allocated point-shadow face layer before the color pass starts.
			for (std::size_t pointShadowLayerIndex{}; pointShadowLayerIndex < pointShadowArrayLayerCount; ++pointShadowLayerIndex) {
				const VkRenderPassBeginInfo pointShadowArrayClearInfo{
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
					.renderPass = pointShadowArray.renderPass,
					.framebuffer = pointShadowArray.layerFramebuffers[pointShadowLayerIndex],
					.renderArea = {.offset = {0, 0}, .extent = {.width = ShadowMap::resolution, .height = ShadowMap::resolution}},
					.clearValueCount = 1U,
					.pClearValues = &shadowClear,
				};

				vkCmdBeginRenderPass(commandBuffer, &pointShadowArrayClearInfo, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdEndRenderPass(commandBuffer);
			}

			constexpr std::size_t pointShadowFaceCount{6U}; // One point light owns six cubemap-style shadow faces.
			const std::size_t pointShadowLightCount{std::min(scene.pointLights.size(), kMaxShadowedPointLights)}; // Fixed point-light cap keeps the array small.
			std::size_t activePointShadowLightCount{}; // Enabled point lights are packed into dense cubemap-style layers by drawFrame().
			for (std::size_t pointIndex{}; pointIndex < pointShadowLightCount; ++pointIndex) { if (scene.pointLights[pointIndex].enabled) { ++activePointShadowLightCount; } }
			const std::size_t activePointFaceCount{activePointShadowLightCount * pointShadowFaceCount}; // Active faces use dense pointIndex * 6 + faceIndex layers.
			// Render active point-light faces after every point layer has a defined shader-read transition path.
			for (std::size_t faceGlobalIndex{}; faceGlobalIndex < activePointFaceCount; ++faceGlobalIndex) {
				const std::size_t pointIndex{faceGlobalIndex / pointShadowFaceCount}; // Source point light for this face.
				const std::size_t faceIndex{faceGlobalIndex % pointShadowFaceCount}; // Local cubemap-style face index.
				const std::size_t pointFaceLayerIndex{pointIndex * pointShadowFaceCount + faceIndex}; // Matches pointLightFaceViewProjs and layerFramebuffers.
				const VkRenderPassBeginInfo pointShadowArrayPassInfo{
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
					.renderPass = pointShadowArray.renderPass,
					.framebuffer = pointShadowArray.layerFramebuffers[pointFaceLayerIndex],
					.renderArea = {.offset = {0, 0}, .extent = {.width = ShadowMap::resolution, .height = ShadowMap::resolution}},
					.clearValueCount = 1U,
					.pClearValues = &shadowClear,
				};

				recordedPassOrder.push_back(RecordedPass::point_shadow);
				vkCmdBeginRenderPass(commandBuffer, &pointShadowArrayPassInfo, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pointShadowArray.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pointShadowArray.pipelineLayout, 0U, 1U, &descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
				drawUploadedObjects(pointShadowArray.pipelineLayout, static_cast<std::uint32_t>(pointFaceLayerIndex));
				vkCmdEndRenderPass(commandBuffer);
			}

			constexpr std::array<float, 4U> skyBackgroundColor{0.45F, 0.70F, 1.00F, 1.00F}; // Sky background for the forward color pass.
			const std::array<VkClearValue, 2U> clearValues{{{.color = {.float32 = {skyBackgroundColor[0], skyBackgroundColor[1], skyBackgroundColor[2], skyBackgroundColor[3]}}}, {.depthStencil = {.depth = 1.0F, .stencil = 0U}}}}; // Index 1 clears depth to the far plane.
			const VkRenderPassBeginInfo renderPassInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = renderPass.renderPass,
				.framebuffer = framebuffers.framebuffers[imageIndex],
				.renderArea = {.offset = {0, 0}, .extent = swapchain.extent},
				.clearValueCount = static_cast<std::uint32_t>(clearValues.size()),
				.pClearValues = clearValues.data(),
			};

			recordedPassOrder.push_back(RecordedPass::forward_color);
			vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.pipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.pipelineLayout, 0U, 1U, &descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
			drawUploadedObjects(pipelineLayout.pipelineLayout);
			if (guiRecord_) { guiRecord_(commandBuffer); }

			vkCmdEndRenderPass(commandBuffer);
			result = vkEndCommandBuffer(commandBuffer);
			if (result != VK_SUCCESS) { return result; }

			return VK_SUCCESS;
		}
	};

	/// @brief No-op renderer used as an explicit placeholder for future renderer selection.
	struct StubRenderer {
		Scene scene{}; ///< CPU scene mirror kept so scene submission has the same storage shape.

		/// @brief Returns the stable renderer-selection id for this placeholder renderer.
		[[nodiscard]] RendererId id() const { return RendererId{.value = "stub"}; }

		/// @brief Declares this renderer's render passes and dependencies for graph merging.
		[[nodiscard]] std::span<const RenderPassContract> passes() const { return detail::forward_renderer_pass_contracts; }

		/// @brief Accepts the normal renderer setup entry point without creating resources.
		[[nodiscard]] VkResult init(SDL_Window *sdlWindow) { (void)sdlWindow; return VK_SUCCESS; }

		/// @brief Releases no resources because the stub owns no renderer backend.
		void shutdown() {}

		/// @brief Reports that the placeholder renderer never owns a live backend.
		[[nodiscard]] bool initialized() const { return false; }

		/// @brief Reports presented-frame diagnostics for the stub renderer.
		[[nodiscard]] std::uint64_t presentedFrameCount() const { return 0; }
		/// @brief Reports triangle-draw diagnostics for the stub renderer.
		[[nodiscard]] std::uint64_t triangleDrawCount() const { return 0; }
		/// @brief Reports triangle-vertex diagnostics for the stub renderer.
		[[nodiscard]] std::uint32_t triangleVertexCount() const { return 0; }
		/// @brief Reports scene-upload diagnostics for the stub renderer.
		[[nodiscard]] std::uint64_t sceneUploadCount() const { return 0; }
		/// @brief Reports scene-mesh draw diagnostics for the stub renderer.
		[[nodiscard]] std::uint64_t sceneMeshDrawCount() const { return 0; }
		/// @brief Reports scene-instance draw diagnostics for the stub renderer.
		[[nodiscard]] std::uint64_t sceneInstanceDrawCount() const { return 0; }
		/// @brief Reports scene-draw vertex diagnostics for the stub renderer.
		[[nodiscard]] std::uint32_t sceneDrawVertexCount() const { return 0; }
		/// @brief Reports scene-draw index diagnostics for the stub renderer.
		[[nodiscard]] std::uint32_t sceneDrawIndexCount() const { return 0; }
		/// @brief Reports the number of retained debug samples for the stub renderer.
		[[nodiscard]] std::size_t sceneDebugSampleCount() const { return 0; }
		/// @brief Returns a CPU debug sample when the stub renderer has retained one.
		[[nodiscard]] std::optional<RenderDebugSample> sceneCpuDebugSample(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns a GPU debug sample when the stub renderer has retained one.
		[[nodiscard]] std::optional<RenderDebugSample> sceneGpuDebugSample(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns clip-space debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugClipError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns depth debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugDepthError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns directional light-space debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugLightSpaceError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns spot light-space debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugSpotLightSpaceError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns point light-space debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugPointLightSpaceError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns lighting debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugLightingError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns directional shadow-sample debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugShadowSampleError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns spot shadow-sample debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugSpotShadowSampleError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns point shadow-sample debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugPointShadowSampleError(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports the number of retained directional shadow-depth samples for the stub renderer.
		[[nodiscard]] std::size_t sceneShadowDepthSampleCount() const { return 0; }
		/// @brief Returns a directional shadow-depth sample when the stub renderer has retained one.
		[[nodiscard]] std::optional<RenderShadowDepthSample> sceneShadowDepthSample(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns directional shadow-depth error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneShadowDepthError(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports the number of retained spot shadow-depth samples for the stub renderer.
		[[nodiscard]] std::size_t sceneSpotShadowDepthSampleCount() const { return 0; }
		/// @brief Returns a spot shadow-depth sample when the stub renderer has retained one.
		[[nodiscard]] std::optional<RenderShadowDepthSample> sceneSpotShadowDepthSample(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports the number of prepared shadow metadata rows for the stub renderer.
		[[nodiscard]] std::size_t sceneShadowLightMetaCount() const { return 0; }
		/// @brief Returns a prepared shadow metadata row when the stub renderer has one.
		[[nodiscard]] std::optional<ShadowLightMeta> sceneShadowLightMeta(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns spot shadow-depth error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneSpotShadowDepthError(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports the number of retained point shadow-depth samples for the stub renderer.
		[[nodiscard]] std::size_t scenePointShadowDepthSampleCount() const { return 0; }
		/// @brief Returns a point shadow-depth sample when the stub renderer has retained one.
		[[nodiscard]] std::optional<RenderShadowDepthSample> scenePointShadowDepthSample(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns point shadow-depth error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> scenePointShadowDepthError(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports prepared GPU target diagnostics for the stub renderer.
		[[nodiscard]] std::size_t preparedGpuTargetCount() const { return 0; }
		std::array<float, 4> clearColor{0.0F, 0.0F, 0.0F, 1.0F}; ///< Last clear color used by the renderer.
		/// @brief Reports the last clear color used by the stub renderer.
		[[nodiscard]] std::array<float, 4> lastClearColor() const { return clearColor; }

		/// @brief Replaces the stored CPU scene for parity with concrete renderer submission.
		void loadScene(Scene nextScene) { scene = std::move(nextScene); }

		/// @brief Appends one backend object to the stored CPU scene mirror.
		void appendObject(Mesh backend_mesh, Mat4 model, std::optional<std::string> base_color_texture_source) {
			const bool use_texture = base_color_texture_source.has_value();
			if (use_texture) { scene.baseColorTexture = *std::move(base_color_texture_source); }
			scene.objects.push_back(Object{.mesh = std::move(backend_mesh),
													 .model = model,
													 .useBaseColorTexture = use_texture ? 1U : 0U});
		}

		/// @brief Clears the stored CPU scene through the common scene replacement path.
		void clearScene() { loadScene(Scene{}); }

		/// @brief Accepts a frame-render request without producing GPU work.
		void renderFrame(VulkanReadback *readback = nullptr) { (void)readback; }

		/// @brief Reports that the stub has no swapchain image to capture.
		[[nodiscard]] auto captureFrameToPng(const std::filesystem::path &output_path) -> std::expected<void, Error> {
			(void)output_path;
			return std::unexpected(Error::missing_object);
		}
	};

	/// @brief Explicit renderer alternatives selectable by the simple render coordinator.
	using SelectedRenderer = std::variant<ForwardRenderer, StubRenderer>;
	static_assert(std::is_default_constructible_v<SelectedRenderer>);
	static_assert(std::is_constructible_v<SelectedRenderer, std::in_place_type_t<ForwardRenderer>>);
	static_assert(std::is_constructible_v<SelectedRenderer, std::in_place_type_t<StubRenderer>>);

} // namespace vve::simple
