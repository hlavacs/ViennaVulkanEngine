export module VEEngine.Simple.Scene;
import std;
import VEEngine.Simple.Math;
import VEEngine.Simple.Mesh;
import VEEngine.Types;

/**
	* @file
	* @brief CPU scene instance data for the simple forward renderer.
	*
	* Functional objects:
	* - Object pairs one CPU mesh with its model transform before any renderer upload exists.
	* - kMaxShadowedSpotLights bounds the first multi-spot-light CPU storage step.
	* - kMaxShadowedPointLights bounds the first point-light shadow metadata step.
	* - Directional shadow constants define the fixed cascaded shadow-map layout and coverage.
	* - Scene stores the CPU drawable list for simple multi-object renderer tests.
	* - makePlane creates a minimal XZ floor mesh for renderer and shadow debug scenes.
	* - makeSampleScene creates a few translated cubes without touching Vulkan state.
	*/
export namespace vve::simple {

	inline constexpr std::size_t kMaxSceneTextures{8U};       ///< Base-color texture slots bound to the forward pass; mirrors the shader array size.
	inline constexpr std::uint32_t kNoTexture{0xFFFFFFFFU};    ///< Object texture index meaning "untextured".
	inline constexpr std::size_t kMaxShadowedSpotLights{10U}; ///< Small fixed cap for the first spot-shadow data model.
	inline constexpr std::size_t kMaxShadowedPointLights{10U}; ///< Small fixed cap for point-shadow CPU metadata.
	inline constexpr std::size_t kMaxDirectionalLights{10U};  ///< Directional-light cap; four cascades each occupy forty shadow-map layers.
	inline constexpr std::size_t kNumShadowCascades{4U};      ///< Directional shadow cascades assigned to every packed light.
	inline constexpr std::size_t kShadowMatrixSpotBase{0U};   ///< First spot-light matrix inside the shared shadow matrix array (one per packed spot light).
	inline constexpr std::size_t kShadowMatrixPointBase{kMaxShadowedSpotLights}; ///< First point-face matrix (six per packed point light).
	inline constexpr std::size_t kShadowMatrixDirBase{kShadowMatrixPointBase + kMaxShadowedPointLights * 6U}; ///< First directional cascade matrix (kNumShadowCascades per packed light).
	inline constexpr std::size_t kShadowMatrixCount{kShadowMatrixDirBase + kMaxDirectionalLights * kNumShadowCascades}; ///< Size of the shared shadow matrix array; mirrors the shader.
	inline constexpr Scalar shadowDistance{60.0F};            ///< Maximum camera distance covered by directional shadows.
	inline constexpr Scalar zBackoff{40.0F};                  ///< Extra light-space depth behind each camera frustum slice.

	/// @brief Host-side drawable object with geometry and a local-to-world transform.
	struct Object {
		Mesh mesh{};                              ///< CPU geometry used by one drawable object.
		Mat4 model{};                             ///< Model matrix placing the mesh in world space.
		std::uint32_t baseColorTextureIndex{kNoTexture}; ///< Index into Scene::textures, or kNoTexture for a plain vertex-colored object.
		bool visible{true};                       ///< True when command recording should draw this object.
		bool castsShadow{true};                   ///< False excludes the object from every shadow depth pass.
		bool unlit{false};                        ///< True renders the object in its flat base color without lighting.
	};

	/// @brief Point light parameters used by the simple forward pass.
	struct PointLight {
		Vec3 position{2.0F, 3.5F, -2.0F}; ///< World-space light position.
		Vec3 color{1.0F, 0.96F, 0.82F};   ///< RGB light tint applied to the direct component.
		Scalar intensity{3.0F};           ///< Multiplier for diffuse and specular lighting.
		Scalar range{7.0F};               ///< Distance where direct light fades to zero.
		Scalar ambient{0.18F};            ///< Scene-wide ambient term for unlit surfaces.
		bool enabled{true};               ///< True when this light participates in rendering.
	};

	/// @brief Directional light parameters used by future simple forward shading.
	struct DirectionalLight {
		Vec3 direction{-0.45F, -0.8F, 0.35F};        ///< World-space direction from the light toward the scene.
		Vec3 color{0.95F, 0.98F, 1.0F};              ///< RGB light tint applied to the direct component.
		LightIntensity intensity{.value = 1.4F};     ///< Multiplier for directional diffuse and specular lighting.
		Scalar ambient{0.06F};                       ///< Ambient term contributed by this light.
		bool enabled{true};                          ///< True when this light participates in rendering.
	};

	/// @brief Spot light parameters used by future simple forward shading.
	struct SpotLight {
		Vec3 position{0.0F, 4.0F, 3.0F};             ///< World-space light position.
		Vec3 direction{0.0F, -0.85F, -0.45F};        ///< World-space direction from the light toward the scene.
		Vec3 color{1.0F, 0.9F, 0.72F};               ///< RGB light tint applied to the direct component.
		LightIntensity intensity{.value = 4.0F};     ///< Multiplier for spotlight diffuse and specular lighting.
		LightRange range{.value = 8.0F};             ///< Distance where direct light fades to zero.
		SpotConeAngle innerConeAngle{.radians = 0.35F}; ///< Angle where the spot light remains fully bright.
		SpotConeAngle outerConeAngle{.radians = 0.65F}; ///< Angle where the spot light fades to zero.
		Scalar ambient{0.04F};                       ///< Ambient term contributed by this light.
		bool enabled{true};                          ///< True when this light participates in rendering.
	};

	/// @brief Host-side scene container with drawable objects in submission order.
	struct Scene {
		std::vector<Object> objects{};                                ///< Drawable objects owned by this CPU scene.
		std::vector<std::filesystem::path> textures{};                ///< Unique base-color image paths indexed by Object::baseColorTextureIndex (at most kMaxSceneTextures).
		std::vector<PointLight> pointLights{};                        ///< Point lights; at most kMaxShadowedPointLights are rendered.
		std::vector<DirectionalLight> directionalLights{};            ///< Directional lights; at most kMaxDirectionalLights are rendered.
		std::vector<SpotLight> spotLights{};                          ///< Spot lights; at most kMaxShadowedSpotLights are rendered.
		Scalar ambient{PointLight{}.ambient};                         ///< Scene-wide ambient term applied to every lit surface.
	};

	/**
		* @brief Creates a two-triangle plane centered at the local origin in the XZ plane.
		*
		* @param halfExtent Positive X and Z half size of the generated floor mesh.
		* @return Mesh containing one flat quad with neutral debug coloring.
		*/
	Mesh makePlane(Vec2 halfExtent) {
		constexpr std::array<float, 3> planeColor{0.1F, 0.6F, 0.2F}; ///< Uniform green ground color avoids vertex gradients.
		return Mesh{
			.vertices{
				{{{-halfExtent.x, 0.0F, -halfExtent.y}}, planeColor, {{0.0F, 0.0F}}}, ///< Back left floor corner.
				{{{halfExtent.x, 0.0F, -halfExtent.y}},  planeColor, {{1.0F, 0.0F}}}, ///< Back right floor corner.
				{{{halfExtent.x, 0.0F, halfExtent.y}},   planeColor, {{1.0F, 1.0F}}}, ///< Front right floor corner.
				{{{-halfExtent.x, 0.0F, halfExtent.y}},  planeColor, {{0.0F, 1.0F}}}, ///< Front left floor corner.
			},
			.indices{0U, 2U, 1U, 0U, 3U, 2U}, ///< Two triangles with the +Y top face front-facing.
		};
	}

	/**
		* @brief Creates a small CPU-only cube scene for early renderer integration.
		*
		* @return Scene containing several cube objects with distinct world transforms.
	*/
	Scene makeSampleScene() {
		const DirectionalLight sampleDirectional{
			.direction = Vec3{-0.55F, -0.78F, 0.30F},                 ///< Cool grazing light crosses all three cubes.
			.color = Vec3{0.65F, 0.82F, 1.0F},                       ///< Blue daylight tint separates it from the point light.
			.intensity = {.value = 0.75F},                           ///< Moderate strength makes directional shading visible.
			.ambient = 0.025F,                                       ///< Low ambient keeps directional shadows readable.
		};
		const SpotLight sampleSpot{
			.position = Vec3{1.45F, 4.8F, -1.45F},                     ///< Warm cone starts above the right side of the scene.
			.direction = Vec3{0.10F, -0.98F, -0.16F},                  ///< Cone aims down across the center cube cluster.
			.color = Vec3{1.0F, 0.58F, 0.38F},                        ///< Orange tint makes spot contribution distinct.
			.intensity = {.value = 2.2F},                             ///< Focused strength makes spot highlights visible.
			.range = {.value = 5.8F},                                 ///< Range covers the cube group without lighting everything.
			.innerConeAngle = {.radians = 0.28F},                     ///< Tight inner cone creates a clear bright core.
			.outerConeAngle = {.radians = 0.58F},                     ///< Wider outer cone gives a visible falloff band.
			.ambient = 0.02F,                                         ///< Small ambient contribution preserves shadow contrast.
		};
		return Scene{.objects{
			Object{.mesh = makeCube(), .model = identityMat4()},                                      ///< Center cube.
			Object{.mesh = makeCube(), .model = translate(identityMat4(), Vec3{-1.5F, 0.0F, 0.0F})}, ///< Left cube.
			Object{.mesh = makeCube(), .model = translate(identityMat4(), Vec3{1.5F, 0.0F, 0.0F})},  ///< Right cube.
		}, .pointLights{PointLight{}}, .directionalLights{sampleDirectional}, .spotLights{sampleSpot}};
	}

} // namespace vve::simple
