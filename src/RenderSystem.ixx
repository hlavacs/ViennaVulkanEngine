export module VEEngine:RenderSystem;
import std;
import VEEngine.Simple;
import VEEngine.Error;
import VEEngine.Types;

/**
	* @file
	* @brief Public render-system facade backed by the selected engine implementation.
	*/
export namespace vve {

	template <typename... TSystems> class Engine;

	/// @brief One CPU/GPU shadow-depth comparison point recorded by the renderer for the world origin.
	struct RenderShadowDepthSample {
		std::uint32_t light_type{};		///< 1 spot, 2 point, 3 directional.
		std::uint32_t light_index{};		///< Dense index of the light inside its packed shadow slots.
		std::uint32_t face_index{};		///< Point-light cube face, 0 for other lights.
		std::uint32_t layer{};				///< Shadow-array layer read for this sample.
		Vec3 world{zeroVec3()};				///< World-space sample point.
		Vec3 light_ndc{zeroVec3()};		///< Sample point in light normalized device coordinates.
		std::uint32_t pixel_x{};			///< Shadow-map texel x, valid when has_gpu is true.
		std::uint32_t pixel_y{};			///< Shadow-map texel y, valid when has_gpu is true.
		float expected_depth{};				///< CPU light-space depth.
		float bias{};							///< Shadow compare bias.
		float shadow_factor{};				///< 0.35 when the GPU texel occludes the point, otherwise 1.
		float gpu_depth{};					///< Depth read back from the shadow map, valid when has_gpu is true.
		float error{};							///< Absolute CPU/GPU depth mismatch, valid when has_gpu is true.
		bool has_gpu{};						///< True once the GPU texel was read back.
	};

	class RenderSystem {
	public:
		RenderSystem(const RenderSystem &) = default;
		RenderSystem(RenderSystem &&) noexcept = default;
		RenderSystem &operator=(const RenderSystem &) = delete;
		RenderSystem &operator=(RenderSystem &&) noexcept = delete;

		auto clearScene()																													-> void;
		[[nodiscard]] auto loadSampleScene()																						-> std::expected<void, Error>;
		auto setCamera(Camera camera, PixelExtent extent)																		-> void;
		void setDirectionalLight(Direction direction_to_light, LinearColor color,
											LightIntensity intensity, LinearColor ambient);
		inline void setDirectionalLight(const DirectionalLight &light) {
			setDirectionalLight(light.direction, light.color, light.intensity, light.ambient);
		}																																		///< Applies a directional light descriptor.
		void addDirectionalLight(Direction direction_to_light, LinearColor color,
											LightIntensity intensity, LinearColor ambient);
		inline void addDirectionalLight(const DirectionalLight &light) {
			addDirectionalLight(light.direction, light.color, light.intensity, light.ambient);
		}																																		///< Adds a directional light descriptor.
		auto setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range)	-> void;
		auto setPointLight(Position position, LinearColor color, LightIntensity intensity,
								 LightRange range, LinearColor ambient)															-> void;
		inline void setPointLight(const PointLight &light) {
			setPointLight(light.position, light.color, light.intensity, light.range, light.ambient);
		}																																		///< Applies a point light descriptor.
		void addPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range);
		void addPointLight(Position position, LinearColor color, LightIntensity intensity,
								 LightRange range, LinearColor ambient);
		inline void addPointLight(const PointLight &light) {
			addPointLight(light.position, light.color, light.intensity, light.range, light.ambient);
		}																																		///< Adds a point light descriptor.
		void setSpotLight(Position position, Direction direction, LinearColor color,
								LightIntensity intensity, LightRange range, SpotConeAngle cone);
		void setSpotLight(Position position, Direction direction, LinearColor color,
								LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient);
		inline void setSpotLight(const SpotLight &light) {
			setSpotLight(light.position, light.direction, light.color, light.intensity, light.range, light.cone, light.ambient);
		}																																		///< Applies a spotlight descriptor.
		void addSpotLight(Position position, Direction direction, LinearColor color,
								LightIntensity intensity, LightRange range, SpotConeAngle cone);
		void addSpotLight(Position position, Direction direction, LinearColor color,
								LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient);
		[[nodiscard]] std::expected<RenderObjectHandle, Error> addPlane(Vec2 half_extent, LinearColor color,
																						 Transform transform = {});
		[[nodiscard]] inline std::expected<RenderObjectHandle, Error> addPlane(const PlaneDescriptor &plane) {
			return addPlane(plane.half_extent, plane.color, plane.transform);
		}																																		///< Adds a plane descriptor.
		[[nodiscard]] std::expected<RenderObjectHandle, Error> addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color,
																						 Transform transform = {});
		[[nodiscard]] inline std::expected<RenderObjectHandle, Error> addCuboid(const CuboidDescriptor &cuboid) {
			return addCuboid(cuboid.minimum, cuboid.maximum, cuboid.color, cuboid.transform);
		}																																		///< Adds a cuboid descriptor.
		[[nodiscard]] std::expected<RenderObjectHandle, Error> addTriangleMesh(
			Vector<Vec3> positions, Vector<std::uint32_t> indices, LinearColor color,
			Transform transform = {});
		[[nodiscard]] std::expected<RenderObjectHandle, Error> addTexturedCuboid(Vec3 minimum, Vec3 maximum,
																									  std::filesystem::path base_color_texture,
																									  Transform transform = {});
		[[nodiscard]] inline std::expected<RenderObjectHandle, Error> addTexturedCuboid(const TexturedCuboidDescriptor &cuboid) {
			return addTexturedCuboid(cuboid.minimum, cuboid.maximum, cuboid.base_color_texture, cuboid.transform);
		}																																		///< Adds a textured cuboid descriptor.
		[[nodiscard]] auto removeObject(RenderObjectHandle handle)															-> std::expected<void, Error>;
		[[nodiscard]] auto setObjectVisible(RenderObjectHandle handle, bool visible)							-> std::expected<void, Error>;
		[[nodiscard]] auto setObjectCastsShadow(RenderObjectHandle handle, bool casts_shadow)				-> std::expected<void, Error>;
		[[nodiscard]] auto setObjectUnlit(RenderObjectHandle handle, bool unlit)								-> std::expected<void, Error>;
		[[nodiscard]] auto objectVisible(RenderObjectHandle handle) const										-> std::expected<bool, Error>;
		[[nodiscard]] auto setObjectTransform(RenderObjectHandle handle, Transform transform)			-> std::expected<void, Error>;
		[[nodiscard]] auto objectTransform(RenderObjectHandle handle) const									-> std::expected<Transform, Error>;
		[[nodiscard]] auto setObjectMeshPositions(RenderObjectHandle handle, Vector<Vec3> positions)
			-> std::expected<void, Error>;
		[[nodiscard]] auto sceneInstanceObjects(RenderSceneInstanceHandle instance) const		-> std::expected<Vector<RenderObjectHandle>, Error>;
		[[nodiscard]] auto objectSourceScene(RenderObjectHandle handle) const							-> std::expected<RenderSceneInstanceHandle, Error>;
		[[nodiscard]] auto objectSourceNode(RenderObjectHandle handle) const							-> std::expected<NodeHandle, Error>;
		[[nodiscard]] auto instantiateScene(SceneHandle scene, SceneInstantiationOptions options = {})	-> std::expected<RenderSceneInstanceHandle, Error>;
		[[nodiscard]] auto removeSceneInstance(RenderSceneInstanceHandle instance)					-> std::expected<void, Error>;
		[[nodiscard]] auto removeScene(SceneHandle handle)															-> std::expected<void, Error>;
		[[nodiscard]] auto purgeUnusedAssets()																				-> std::size_t;
		[[nodiscard]] auto sceneMeshCount() const																					-> std::size_t;
		[[nodiscard]] auto sceneMaterialCount() const																			-> std::size_t;
		[[nodiscard]] auto sceneDirectionalLightCount() const																-> std::size_t;
		[[nodiscard]] auto scenePointLightCount() const																		-> std::size_t;
		[[nodiscard]] auto sceneSpotLightCount() const																			-> std::size_t;
		[[nodiscard]] auto sceneCameraCount() const																				-> std::size_t;
		[[nodiscard]] auto sceneInstanceCount() const																			-> std::size_t;
		[[nodiscard]] auto sceneVertexCount() const																				-> std::size_t;
		[[nodiscard]] auto sceneIndexCount() const																				-> std::size_t;
		[[nodiscard]] auto hasSceneCamera() const																					-> bool;
		[[nodiscard]] auto hasSceneDirectionalLight() const																	-> bool;
		[[nodiscard]] auto hasScenePointLight() const																			-> bool;
		[[nodiscard]] auto hasSceneSpotLight() const																				-> bool;
		[[nodiscard]] auto shadowDepthSamples() const																	-> Vector<RenderShadowDepthSample>;
		auto setShadowDepthReadback(bool enabled)																		-> void;
		[[nodiscard]] auto captureFrameToPng(const std::filesystem::path &output_path)						-> std::expected<void, Error>;
		[[nodiscard]] auto renderedFrameCount() const																			-> std::uint64_t;
		[[nodiscard]] auto renderingFramesPerSecond() const																-> double;
		[[nodiscard]] auto lastRenderedWindowCount() const																		-> std::size_t;

	private:
		template <typename... TSystems> friend class Engine;

		using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::RenderSystem;	///< Wrapped implementation class.
		explicit RenderSystem(Impl &implementation) noexcept;

		Impl &impl_;	///< Non-owning reference to the wrapped implementation.
	};	///< Public render-system wrapper.

} // namespace vve
