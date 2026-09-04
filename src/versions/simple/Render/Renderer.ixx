module;
#include <new>
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>

export module VEEngine.Simple.Renderer;
import std;
import VEEngine.Types;
import VEEngine.Simple.Math;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;
export import :RendererDebug;
export import :RendererShadowPrep;
export import :RendererResources;
export import :RendererDraw;

/**
	* @file
	* @brief Vulkan renderer skeleton for the simple forward renderer.
	*
	* Functional objects:
	* - ShadowLightMeta records CPU-side light-to-shadow-layer bindings before shader data grows.
	* - ForwardRendererDebug supplies per-light shadow-depth samples and PNG capture through the renderer debug partition.
	* - ForwardRendererShadowPrep supplies CPU shadow matrix and metadata preparation through the renderer shadow-prep partition.
	* - ForwardRendererResources supplies GPU resource creation, swapchain recreation, and teardown through the renderer resources partition.
	* - ForwardRendererDraw records per-frame shadow and color commands and submits the completed frame through the renderer draw partition.
	* - ForwardRenderer owns the current CPU scene, swapchain image stack, depth attachment, unbound shadow maps and shadow pipeline, optional object texture, default object texture, forward render pass, framebuffers, descriptor-set layout, pipeline layout, shader modules, graphics pipeline, command pool, frame command buffers, frame synchronization, per-frame uniform buffers, descriptor pool, per-frame descriptor sets, and uploaded per-object meshes needed before rendering.
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

	/// @brief Minimal forward renderer owning Vulkan swapchain bring-up without draw state.
	struct ForwardRenderer : ForwardRendererDebug<ForwardRenderer>, ForwardRendererShadowPrep<ForwardRenderer>, ForwardRendererResources<ForwardRenderer>, ForwardRendererDraw<ForwardRenderer> {
		/// @brief Lightweight command-recording pass tag used by tests without introducing a render graph.
		enum class RecordedPass : std::uint8_t { directional_shadow, spot_shadow, point_shadow, forward_color };

		static constexpr std::uint32_t framesInFlight{2U}; ///< Number of independent frame command buffers to allocate.
		VulkanInstance instance{};             ///< Owned Vulkan instance wrapper.
		VulkanSurface surface{};               ///< Owned SDL-backed Vulkan surface wrapper.
		VulkanPhysicalDevice physicalDevice{}; ///< Selected borrowed Vulkan physical device wrapper.
		VulkanDevice device{};                 ///< Owned Vulkan logical device wrapper.
		VulkanAllocator allocator{};           ///< Owned VMA allocator for every buffer and image below.
		VulkanSwapchain swapchain{};           ///< Owned swapchain wrapper for presentation images.
		VulkanImageViews imageViews{};         ///< Owned color image views for swapchain images.
		VulkanImage depthImage{};              ///< Owned swapchain-sized depth attachment image and view.
		ShadowMap dirShadowArray{};            ///< Owned directional shadow-map texture array with one layer per active directional light.
		ShadowMap spotShadowArray{};           ///< Owned spot shadow-map texture array with one layer per active spot light.
		ShadowMap pointShadowArray{};          ///< Owned point shadow-map texture array with six layers per shadowed point light.
		std::array<TextureImage, kMaxSceneTextures> objectTextures{}; ///< Owned base-color textures, one per Scene::textures entry.
		TextureImage defaultObjectTexture{};    ///< Owned opaque-white texture filling unused texture slots.
		VulkanDescriptorSetLayout descriptorSetLayout{}; ///< Owned frame-uniform and shadow-map descriptor-set layout.
		VulkanPipelineLayout pipelineLayout{}; ///< Owned graphics pipeline layout using the frame descriptor set.
		VulkanShaderModule vertShaderModule{}; ///< Owned forward vertex shader module.
		VulkanShaderModule fragShaderModule{}; ///< Owned forward fragment shader module.
		VulkanShaderModule shadowShaderModule{}; ///< Owned depth-only shadow vertex shader module.
		VulkanGraphicsPipeline graphicsPipeline{}; ///< Owned forward graphics pipeline for swapchain rendering.
		VulkanGraphicsPipeline shadowPipeline{};   ///< Owned depth-only pipeline shared by every shadow layer.
		VulkanCommandPool commandPool{};       ///< Owned resettable command pool for the graphics queue family.
		VulkanCommandBuffers commandBuffers{}; ///< Owned primary command buffers, one for each frame in flight.
		VulkanFrameSync frameSync{};           ///< Owned per-frame semaphores and fences for rendering.
		VulkanUniformBuffers uniformBuffers{}; ///< Owned per-frame uniform buffers for camera and object data.
		VulkanDescriptorPool descriptorPool{}; ///< Owned descriptor pool for per-frame uniform and shadow-map descriptor sets.
		VulkanDescriptorSets descriptorSets{}; ///< Owned per-frame descriptor sets binding frame uniform buffers and the shadow map.
		std::vector<VulkanMesh> meshes{};      ///< Owned GPU meshes uploaded from the current scene objects.
		SDL_Window *window{nullptr};           ///< Borrowed SDL window used to create the Vulkan surface.
		Scene scene{}; ///< CPU scene data kept in STL containers until renderer upload exists.
		std::vector<ShadowLightMeta> shadowLightMeta{}; ///< Per-light shadow slot/layer metadata prepared every frame.
		std::vector<RecordedPass> recordedPassOrder{}; ///< Last frame's command-recording pass order diagnostic.
		Vec3 cameraEye{zero(), static_cast<Scalar>(6.0), static_cast<Scalar>(9.0)}; ///< World-space camera position used for the frame view matrix.
		Vec3 cameraTarget{zero(), one(), zero()}; ///< World-space point looked at by the frame view matrix.
		std::optional<std::uint32_t> lastRenderedImageIndex{}; ///< Swapchain image index from the last acquired, rendered, and presented frame.

		~ForwardRenderer() { cleanup(); }

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

		/// @brief Reports the number of prepared spot shadow metadata rows.
		[[nodiscard]] std::size_t sceneShadowLightMetaCount() const { return shadowLightMeta.size(); }
		/// @brief Returns one prepared spot shadow metadata row by retained index.
		[[nodiscard]] std::optional<ShadowLightMeta> sceneShadowLightMeta(std::size_t index) const {
			if (index >= shadowLightMeta.size()) { return {}; }
			return shadowLightMeta[index];
		}

		/// @brief Chooses FIFO in Debug builds and mailbox in optimized builds.
		[[nodiscard]] static constexpr VulkanSwapchain::PresentModePreference defaultPresentMode() {
#ifdef VVE_SIMPLE_RELEASE_PRESENT_MAILBOX
			return VulkanSwapchain::PresentModePreference::mailbox;
#else
			return VulkanSwapchain::PresentModePreference::fifo;
#endif
		}

		/**
			* @brief Replaces the current CPU scene before future renderer upload.
			*
			* @param nextScene Scene data prepared by the caller.
		*/
		void loadScene(Scene nextScene) {
			scene = std::move(nextScene);								// Scene upload invalidates per-frame shadow metadata.
			shadowLightMeta.clear();									// Metadata is rebuilt during the next frame assembly.
			sceneGeometryDirty_.clear();
			sceneResourcesDirty_ = true;
			sceneRequiresFullUpload_ = true;
		}

		/// @brief Appends one backend object to the renderer-owned CPU scene mirror; the texture path is deduplicated into Scene::textures.
		void appendObject(Mesh backend_mesh, Mat4 model, std::optional<std::string> base_color_texture_source) {
			std::uint32_t textureIndex{kNoTexture};
			if (base_color_texture_source) {
				const std::filesystem::path path{*base_color_texture_source};
				const auto found = std::ranges::find(scene.textures, path);
				if (found != scene.textures.end()) {
					textureIndex = static_cast<std::uint32_t>(found - scene.textures.begin());
				} else if (scene.textures.size() < kMaxSceneTextures) {
					textureIndex = static_cast<std::uint32_t>(scene.textures.size());
					scene.textures.push_back(path);
				}
			}
			scene.objects.push_back(Object{.mesh = std::move(backend_mesh), .model = model, .baseColorTextureIndex = textureIndex});
			sceneResourcesDirty_ = true;
		}

		/// @brief Replaces positions for one fixed-topology backend mesh.
		[[nodiscard]] bool updateObjectMeshPositions(std::size_t index, const Vector<Vec3> &positions) {
			if (index >= scene.objects.size() ||
				scene.objects[index].mesh.vertices.size() != positions.size()) {
				return false;
			}
			for (std::size_t vertex{}; vertex < positions.size(); ++vertex) {
				scene.objects[index].mesh.vertices[vertex].position = {
					positions[vertex].x, positions[vertex].y, positions[vertex].z};
			}
			sceneGeometryDirty_.insert(index);
			sceneResourcesDirty_ = true;
			return true;
		}

		/// @brief Removes one backend object and schedules a compact GPU mesh rebuild.
		[[nodiscard]] bool removeObject(std::size_t index) {
			if (index >= scene.objects.size()) { return false; }
			scene.objects.erase(scene.objects.begin() + static_cast<std::ptrdiff_t>(index));
			sceneGeometryDirty_.clear();
			sceneResourcesDirty_ = true;
			sceneRequiresFullUpload_ = true;
			return true;
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

		/// @brief Common frame-render entry forwarding to the concrete Vulkan draw path.
		void renderFrame(VulkanReadback *readback = nullptr) { drawFrame(readback); }

	private:
		friend struct ForwardRendererResources<ForwardRenderer>;
		friend struct ForwardRendererDraw<ForwardRenderer>;
		std::uint32_t currentFrame{0U}; ///< Index of the frame synchronization set used by the next draw.
		VkDescriptorPool imguiDescriptorPool_{VK_NULL_HANDLE}; ///< Owned Dear ImGui descriptor pool reserved for backend texture descriptors.
		void *guiSystem_{nullptr}; ///< Non-owning, type-erased GUI system pointer reserved for later GUI integration.
		std::function<void(VkCommandBuffer)> guiRecord_; ///< Optional GUI recorder invoked during the forward color pass.
		std::vector<std::filesystem::path> uploadedTextures_{}; ///< Scene::textures as of the last GPU texture upload.
		bool sceneResourcesDirty_{true}; ///< CPU scene topology or texture changed after the last GPU synchronization.
		bool sceneRequiresFullUpload_{true}; ///< Removal or replacement requires rebuilding index-aligned GPU meshes.
		std::set<std::size_t> sceneGeometryDirty_{}; ///< Existing GPU meshes requiring a vertex-buffer refresh.
	};

} // namespace vve::simple
