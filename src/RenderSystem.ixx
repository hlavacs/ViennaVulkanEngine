export module VEEngine:RenderSystem;
import std;
import VEEngine.Error;
import VEEngine.Types;

/**
	* @file
	* @brief Public render-system facade backed by the selected engine implementation.
	*/
export namespace vve {

	template <typename... TSystems> class Engine;

	struct RenderDebugSample {
		std::uint32_t vertex_id{};					///< Source vertex id.
		Vec3 world{zeroVec3()};						///< World-space vertex position.
		Vec4 clip{};									///< Clip-space position.
	      Vec4 light_clip{};						///< Directional-light clip-space position.
	      Vec4 spot_light_clip{};					///< Spot-light clip-space position.
	      Vec4 point_light_clip{};				///< Point-light face clip-space position.
	      Vec3 ndc{zeroVec3()};					///< Normalized device coordinate.
	      Vec3 light_ndc{zeroVec3()};			///< Directional-light normalized device coordinate.
	      Vec3 spot_light_ndc{zeroVec3()};		///< Spot-light normalized device coordinate.
	      Vec3 point_light_ndc{zeroVec3()};	///< Point-light face normalized device coordinate.
		Vec3 normal{zeroVec3()};					///< Normal used for lighting.
		Vec3 direction_to_light{zeroVec3()};	///< Direction from surface to light.
		Vec3 ambient_lighting{zeroVec3()};		///< Ambient light contribution.
		Vec3 direct_lighting{zeroVec3()};		///< Direct light contribution.
		Vec3 point_lighting{zeroVec3()};			///< Point-light contribution.
		Vec3 spot_lighting{zeroVec3()};			///< Spot-light contribution.
		Vec3 final_lighting{zeroVec3()};			///< Ambient plus direct lighting.
		float depth{};									///< Vulkan depth value.
	      float light_depth{};						///< Directional-light depth value.
	      float spot_light_depth{};				///< Spot-light depth value.
	      float point_light_depth{};				///< Point-light face depth value.
		float sampled_shadow_depth{};				///< Shadow-map depth sampled by the shader.
		float shadow_depth_delta{};				///< Light depth minus sampled shadow depth.
		float shadow_bias{};							///< Bias used by the shadow comparison.
		float shadow_factor{};						///< One when lit, zero when shadowed.
		float sampled_spot_shadow_depth{};		///< Spot shadow-map depth sampled by the shader.
		float spot_shadow_depth_delta{};			///< Spot depth minus sampled spot shadow depth.
	      float spot_shadow_bias{};				///< Bias used by the spot shadow comparison.
	      float spot_shadow_factor{};			///< One when spot-lit, zero when spot-shadowed.
	      float sampled_point_shadow_depth{};	///< Point shadow-map depth sampled by the shader.
	      float point_shadow_depth_delta{};	///< Point depth minus sampled point shadow depth.
	      float point_shadow_bias{};				///< Bias used by the point shadow comparison.
	      float point_shadow_factor{};			///< One when point-lit, zero when point-shadowed.
	      std::uint32_t point_shadow_face{};	///< Selected point shadow face.
	      float n_dot_l{};							///< Lambert cosine term.
	      bool inside_light{};						///< Whether the sample is inside the light projection.
	      bool inside_spot_light{};				///< Whether the sample is inside the spot projection.
	      bool inside_point_light{};				///< Whether the sample is inside the selected point face.
	      bool valid{};								///< Whether this slot contains a sample.
	   };

	/// @brief Public CPU/GPU comparison point for downloaded shadow-depth data.
	struct RenderShadowDepthSample {
	      std::uint32_t triangle_id{};			///< Source triangle used for the centroid sample.
	      std::uint32_t face_index{};			///< Point-shadow face, or zero for 2D light maps.
	      Vec3 world{zeroVec3()};					///< World-space centroid.
		Vec3 light_ndc{zeroVec3()};				///< Directional-light normalized device coordinate.
		std::uint32_t pixel_x{};					///< Shadow-map texel x coordinate.
		std::uint32_t pixel_y{};					///< Shadow-map texel y coordinate.
		float expected_depth{};						///< CPU-computed light-space depth.
		float bias{};									///< Bias used by the shadow comparison.
		float shadow_factor{};						///< Final directional shadow factor.
		float gpu_depth{};							///< Downloaded shadow-map depth.
		float error{};									///< Absolute CPU/GPU depth mismatch.
		bool has_gpu{};								///< Whether the depth image was downloaded.
		bool valid{};									///< Whether this slot contains a sample.
	};

	/// @brief Public CPU/GPU comparison point for spot-light shadow-depth data.
	struct SpotShadowDepthSample {
		std::uint32_t triangle_id{};			///< Source triangle used for the sample.
		std::uint32_t face_index{};			///< Spot shadow slot index.
		Vec3 world{zeroVec3()};					///< World-space sample position.
		Vec3 light_ndc{zeroVec3()};				///< Spot-light normalized device coordinate.
		std::uint32_t pixel_x{};					///< Shadow-map texel x coordinate.
		std::uint32_t pixel_y{};					///< Shadow-map texel y coordinate.
		float expected_depth{};					///< CPU-computed light-space depth.
		float bias{};									///< Bias used by the shadow comparison.
		float shadow_factor{};					///< Final spot shadow factor.
		float gpu_depth{};							///< Downloaded shadow-map depth.
		float error{};									///< Absolute CPU/GPU depth mismatch.
		bool has_gpu{};								///< Whether the depth image was downloaded.
		bool valid{};									///< Whether this slot contains a sample.
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
		auto setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range)	-> void;
		auto setPointLight(Position position, LinearColor color, LightIntensity intensity,
								 LightRange range, LinearColor ambient)															-> void;
		inline void setPointLight(const PointLight &light) {
			setPointLight(light.position, light.color, light.intensity, light.range, light.ambient);
		}																																		///< Applies a point light descriptor.
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
		[[nodiscard]] std::expected<void, Error> addPlane(Vec2 half_extent, LinearColor color,
																			Transform transform = {});
		[[nodiscard]] inline std::expected<void, Error> addPlane(const PlaneDescriptor &plane) {
			return addPlane(plane.half_extent, plane.color, plane.transform);
		}																																		///< Adds a plane descriptor.
		[[nodiscard]] std::expected<void, Error> addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color,
																			Transform transform = {});
		[[nodiscard]] inline std::expected<void, Error> addCuboid(const CuboidDescriptor &cuboid) {
			return addCuboid(cuboid.minimum, cuboid.maximum, cuboid.color, cuboid.transform);
		}																																		///< Adds a cuboid descriptor.
		[[nodiscard]] std::expected<void, Error> addTexturedCuboid(Vec3 minimum, Vec3 maximum,
																					 std::filesystem::path base_color_texture,
																					 Transform transform = {});
		[[nodiscard]] inline std::expected<void, Error> addTexturedCuboid(const TexturedCuboidDescriptor &cuboid) {
			return addTexturedCuboid(cuboid.minimum, cuboid.maximum, cuboid.base_color_texture, cuboid.transform);
		}																																		///< Adds a textured cuboid descriptor.
		[[nodiscard]] auto sceneMeshCount() const																					-> std::size_t;
		[[nodiscard]] auto sceneMaterialCount() const																			-> std::size_t;
		[[nodiscard]] auto sceneInstanceCount() const																			-> std::size_t;
		[[nodiscard]] auto sceneVertexCount() const																				-> std::size_t;
		[[nodiscard]] auto sceneIndexCount() const																				-> std::size_t;
		[[nodiscard]] auto sceneShadowDepthSampleCount() const														-> std::size_t;
		[[nodiscard]] auto sceneShadowDepthSample(std::size_t index) const									-> std::optional<RenderShadowDepthSample>;
		[[nodiscard]] auto sceneSpotShadowDepthSampleCount() const													-> std::size_t;
		[[nodiscard]] auto sceneSpotShadowDepthSample(std::size_t index) const								-> std::optional<SpotShadowDepthSample>;
		[[nodiscard]] auto sceneSpotShadowDepthSampleSlot(std::size_t index) const						-> std::optional<std::uint32_t>;
		[[nodiscard]] auto sceneSpotShadowDepthSampleExpectedDepth(std::size_t index) const		-> std::optional<float>;
		[[nodiscard]] auto sceneSpotShadowDepthSampleBias(std::size_t index) const						-> std::optional<float>;
		[[nodiscard]] auto sceneSpotShadowDepthSampleFactor(std::size_t index) const					-> std::optional<float>;
		[[nodiscard]] auto sceneSpotShadowDepthGpuDepth(std::size_t index) const						-> std::optional<float>;
		[[nodiscard]] auto sceneSpotShadowDepthHasGpu(std::size_t index) const							-> std::optional<bool>;
		[[nodiscard]] auto sceneSpotShadowDepthError(std::size_t index) const							-> std::optional<float>;
		[[nodiscard]] auto hasSceneCamera() const																					-> bool;
		[[nodiscard]] auto hasSceneDirectionalLight() const																	-> bool;
		[[nodiscard]] auto hasScenePointLight() const																			-> bool;
		[[nodiscard]] auto hasSceneSpotLight() const																				-> bool;
		[[nodiscard]] auto captureFrameToPng(const std::filesystem::path &output_path)						-> std::expected<void, Error>;
		[[nodiscard]] auto renderedFrameCount() const																			-> std::uint64_t;
		[[nodiscard]] auto lastRenderedWindowCount() const																		-> std::size_t;

	private:
		template <typename... TSystems> friend class Engine;

		explicit RenderSystem(void *implementation) noexcept;

		void *impl_{};									///< Opaque non-owning implementation pointer.
	};	///< Public render-system wrapper.

} // namespace vve
