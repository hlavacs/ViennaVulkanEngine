module;
#include "shaders/simple_shared.h"

export module VEEngine.Simple.Scene;
import std;
import VEEngine.Simple.Types;

/**
	* @file
	* @brief CPU lighting and texture views for the simple forward renderer.
	*
	* Functional objects:
	* - RenderTexture stores one canonical decoded RGBA8 image shared by CPU materials and GPU uploads.
	* - PointLight, DirectionalLight, and SpotLight store forward-lighting parameters.
	* - Scene stores lights and a non-owning view of the RenderScene texture table; geometry stays in RenderScene.
	*/
export namespace vve::simple {

	inline constexpr std::size_t kMaxSceneTextures{VVE_MAX_SCENE_TEXTURES}; ///< Texture slots bound to the forward pass; defined for C++ and Slang in simple_shared.h.
	inline constexpr std::uint32_t kNoTexture{VVE_NO_TEXTURE}; ///< Material texture index meaning "untextured"; defined in simple_shared.h.
	inline constexpr std::size_t kMaxShadowedSpotLights{VVE_MAX_SHADOWED_SPOT_LIGHTS}; ///< Small fixed cap for the first spot-shadow data model.
	inline constexpr std::size_t kMaxShadowedPointLights{VVE_MAX_SHADOWED_POINT_LIGHTS}; ///< Small fixed cap for point-shadow CPU metadata.
	inline constexpr std::size_t kMaxDirectionalLights{VVE_MAX_DIRECTIONAL_LIGHTS};  ///< Directional-light cap; each cascade occupies one shadow-map layer.
	inline constexpr std::size_t kNumShadowCascades{VVE_NUM_SHADOW_CASCADES};      ///< Directional shadow cascades assigned to every packed light.
	inline constexpr std::size_t kShadowMatrixSpotBase{0U};   ///< First spot-light matrix inside the shared shadow matrix array (one per packed spot light).
	inline constexpr std::size_t kShadowMatrixPointBase{kMaxShadowedSpotLights}; ///< First point-face matrix (six per packed point light).
	inline constexpr std::size_t kShadowMatrixDirBase{kShadowMatrixPointBase + kMaxShadowedPointLights * 6U}; ///< First directional cascade matrix (kNumShadowCascades per packed light).
	inline constexpr std::size_t kShadowMatrixCount{kShadowMatrixDirBase + kMaxDirectionalLights * kNumShadowCascades}; ///< Size of the shared shadow matrix array; mirrors the shader.
	inline constexpr Scalar shadowDistance{60.0F};            ///< Maximum camera distance covered by directional shadows.
	inline constexpr Scalar zBackoff{40.0F};                  ///< Extra light-space depth behind each camera frustum slice.

	/// @brief One semantic-agnostic decoded texture retained by the CPU render scene.
	struct RenderTexture {
		std::filesystem::path canonical_path{}; ///< Canonical absolute source path used as the deduplication key.
		std::vector<std::byte> rgba8{};         ///< Tight four-channel 8-bit pixels decoded once with stb_image.
		PixelExtent extent{};                    ///< Decoded image dimensions in pixels.
		bool linear{false};                       ///< True for data maps uploaded without sRGB conversion.
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

	/// @brief Directional light parameters used by the simple forward pass.
	struct DirectionalLight {
		Vec3 direction{-0.45F, -0.8F, 0.35F};    ///< World-space direction from the light toward the scene.
		Vec3 color{0.95F, 0.98F, 1.0F};          ///< RGB light tint applied to the direct component.
		LightIntensity intensity{.value = 1.4F}; ///< Multiplier for directional diffuse and specular lighting.
		Scalar ambient{0.06F};                   ///< Ambient term contributed by this light.
		bool enabled{true};                      ///< True when this light participates in rendering.
	};

	/// @brief Spot light parameters used by the simple forward pass.
	struct SpotLight {
		Vec3 position{0.0F, 4.0F, 3.0F};                 ///< World-space light position.
		Vec3 direction{0.0F, -0.85F, -0.45F};            ///< World-space direction from the light toward the scene.
		Vec3 color{1.0F, 0.9F, 0.72F};                   ///< RGB light tint applied to the direct component.
		LightIntensity intensity{.value = 4.0F};         ///< Multiplier for spotlight diffuse and specular lighting.
		LightRange range{.value = 8.0F};                 ///< Distance where direct light fades to zero.
		SpotConeAngle innerConeAngle{.radians = 0.35F};  ///< Angle where the spot light remains fully bright.
		SpotConeAngle outerConeAngle{.radians = 0.65F};  ///< Angle where the spot light fades to zero.
		Scalar ambient{0.04F};                           ///< Ambient term contributed by this light.
		bool enabled{true};                              ///< True when this light participates in rendering.
	};

	/// @brief Renderer-side lights and the capped non-owning texture upload view.
	struct Scene {
		std::vector<const RenderTexture *> textures{};      ///< RenderScene texture entries in shader-table order.
		std::vector<PointLight> pointLights{};              ///< Point lights; at most kMaxShadowedPointLights are rendered.
		std::vector<DirectionalLight> directionalLights{};  ///< Directional lights; at most kMaxDirectionalLights are rendered.
		std::vector<SpotLight> spotLights{};                ///< Spot lights; at most kMaxShadowedSpotLights are rendered.
		Scalar ambient{PointLight{}.ambient};               ///< Scene-wide ambient term applied to every lit surface.
	};

} // namespace vve::simple
