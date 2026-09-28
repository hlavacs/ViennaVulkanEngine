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
	* - RenderTexture retains a decode source and temporary RGBA8 pixels until its generation is resident on the GPU.
	* - ForwardPointLight, ForwardDirectionalLight, and ForwardSpotLight store forward-lighting parameters.
	* - Scene stores lights; geometry, materials, and textures stay in RenderScene.
	*/
export namespace vve::simple {

	inline constexpr std::size_t kMaxSceneTextures{VVE_MAX_SCENE_TEXTURES}; ///< Texture slots bound to the forward pass; defined for C++ and Slang in simple_shared.h.
	inline constexpr std::uint32_t kNoTexture{VVE_NO_TEXTURE}; ///< Material texture index meaning "untextured"; defined in simple_shared.h.
	inline constexpr std::size_t kMaxShadowedSpotLights{VVE_MAX_SHADOWED_SPOT_LIGHTS}; ///< Small fixed cap for the first spot-shadow data model.
	inline constexpr std::size_t kMaxShadowedPointLights{VVE_MAX_SHADOWED_POINT_LIGHTS}; ///< Small fixed cap for point-shadow CPU metadata.
	inline constexpr std::size_t kMaxDirectionalLights{VVE_MAX_DIRECTIONAL_LIGHTS};  ///< Directional-light cap; each cascade occupies one shadow-map layer.
	inline constexpr std::size_t kNumShadowCascades{VVE_NUM_SHADOW_CASCADES};      ///< Directional shadow cascades assigned to every packed light.
	inline constexpr std::size_t kShadowMatrixSpotBase{0U};   ///< First spot-light matrix inside the shared shadow matrix array (one per packed spot light).
	inline constexpr std::size_t kShadowMatrixPointBase{VVE_SHADOW_MATRIX_POINT_BASE}; ///< First point-face matrix (six per packed point light).
	inline constexpr std::size_t kShadowMatrixDirBase{VVE_SHADOW_MATRIX_DIR_BASE}; ///< First directional cascade matrix (kNumShadowCascades per packed light).
	inline constexpr std::size_t kShadowMatrixCount{VVE_SHADOW_MATRIX_COUNT}; ///< Size of the shared shadow matrix array; mirrors the shader.
	inline constexpr Scalar shadowDistance{60.0F};            ///< Maximum camera distance covered by directional shadows.
	inline constexpr Scalar zBackoff{40.0F};                  ///< Extra light-space depth behind each camera frustum slice.

	/// @brief One texture source with pixels released after upload; re-upload decodes again and returns io_error if the file is unavailable.
	struct RenderTexture {
		std::filesystem::path canonical_path{}; ///< Canonical file path or synthetic embedded key used for deduplication.
		std::shared_ptr<const EmbeddedImage> embedded{}; ///< Retained embedded source; otherwise re-decode uses canonical_path.
		std::vector<std::byte> rgba8{};         ///< Tight RGBA8 pixels; storage is freed after this generation reaches the GPU.
		PixelExtent extent{};                    ///< Decoded image dimensions in pixels.
		bool greyscale{false};                    ///< Cached decode classification for HEIGHT-map rejection.
		bool linear{false};                       ///< True for data maps uploaded without sRGB conversion.
		std::uint64_t generation{0};              ///< Unique identity of the decoded image; 0 marks a free texture-table slot.
	};

	/// @brief Point light parameters used by the simple forward pass.
	struct ForwardPointLight {
		Vec3 position{2.0F, 3.5F, -2.0F}; ///< World-space light position.
		Vec3 color{1.0F, 0.96F, 0.82F};   ///< RGB light tint applied to the direct component.
		Scalar intensity{3.0F};           ///< Multiplier for diffuse and specular lighting.
		Scalar range{7.0F};               ///< Distance where direct light fades to zero.
		Scalar ambient{0.18F};            ///< Scene-wide ambient term for unlit surfaces.
		bool enabled{true};               ///< True when this light participates in rendering.
		std::uint64_t owner{0};           ///< Scene instance that imported this light; 0 for lights set through the API.
	};

	/// @brief Directional light parameters used by the simple forward pass.
	struct ForwardDirectionalLight {
		Vec3 direction{-0.45F, -0.8F, 0.35F};    ///< World-space direction from the light toward the scene.
		Vec3 color{0.95F, 0.98F, 1.0F};          ///< RGB light tint applied to the direct component.
		LightIntensity intensity{.value = 1.4F}; ///< Multiplier for directional diffuse and specular lighting.
		Scalar ambient{0.06F};                   ///< Ambient term contributed by this light.
		bool enabled{true};                      ///< True when this light participates in rendering.
		std::uint64_t owner{0};           ///< Scene instance that imported this light; 0 for lights set through the API.
	};

	/// @brief Spot light parameters used by the simple forward pass.
	struct ForwardSpotLight {
		Vec3 position{0.0F, 4.0F, 3.0F};                 ///< World-space light position.
		Vec3 direction{0.0F, -0.85F, -0.45F};            ///< World-space direction from the light toward the scene.
		Vec3 color{1.0F, 0.9F, 0.72F};                   ///< RGB light tint applied to the direct component.
		LightIntensity intensity{.value = 4.0F};         ///< Multiplier for spotlight diffuse and specular lighting.
		LightRange range{.value = 8.0F};                 ///< Distance where direct light fades to zero.
		SpotConeAngle innerConeAngle{.radians = 0.35F};  ///< Angle where the spot light remains fully bright.
		SpotConeAngle outerConeAngle{.radians = 0.65F};  ///< Angle where the spot light fades to zero.
		Scalar ambient{0.04F};                           ///< Ambient term contributed by this light.
		bool enabled{true};                              ///< True when this light participates in rendering.
		std::uint64_t owner{0};           ///< Scene instance that imported this light; 0 for lights set through the API.
	};

	/// @brief Renderer-side lights; geometry, materials and textures stay in RenderScene.
	struct Scene {
		std::vector<ForwardPointLight> pointLights{};              ///< Point lights; at most kMaxShadowedPointLights are rendered.
		std::vector<ForwardDirectionalLight> directionalLights{};  ///< Directional lights; at most kMaxDirectionalLights are rendered.
		std::vector<ForwardSpotLight> spotLights{};                ///< Spot lights; at most kMaxShadowedSpotLights are rendered.
		Scalar ambient{0.18F};               ///< Scene-wide ambient term applied to every lit surface.
	};

} // namespace vve::simple
