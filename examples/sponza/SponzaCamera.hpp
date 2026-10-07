#pragma once

/** @file @brief Authored camera settings for the large Sea Keep scene shown by the Sponza example. */
namespace vve::example::sponza {

inline constexpr Position eye{.value = Vec3{15.0F, 350.0F, 800.0F}}; ///< Outside the island, facing its castle.
inline constexpr Position target{.value = Vec3{15.0F, 170.0F, 84.0F}}; ///< Center of the castle's imported world bounds.
inline constexpr ClipPlanes clip{.near_plane = 1.0F, .far_plane = 3000.0F}; ///< Covers the island, sea and sky at their authored scale.
inline constexpr Scalar move_speed{80.0F}; ///< World units per second for exploring this large model.

/// @brief Applies keyboard movement while retaining the example's clipping range on every frame.
[[nodiscard]] inline auto updateSponzaCamera(DefaultCameraController &controller, const InputState &input, DeltaTime dt) -> Camera {
	auto camera = controller.update(input, dt);
	camera.clip = clip; // The shared controller returns a fresh Camera with default clip planes.
	return camera;
}

} // namespace vve::example::sponza
