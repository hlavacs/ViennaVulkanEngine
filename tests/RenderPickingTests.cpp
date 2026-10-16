/**
 * @file
 * @brief CPU checks for objectBounds and raycast: world boxes, nearest hits, misses, hidden objects and errors.
 */
import std;
import VVEngine;

namespace {

/// @brief Compares two vectors component-wise with a small tolerance.
[[nodiscard]] bool close(vve::Vec3 lhs, vve::Vec3 rhs) {
	return std::abs(lhs.x - rhs.x) < 1.0e-4F && std::abs(lhs.y - rhs.y) < 1.0e-4F && std::abs(lhs.z - rhs.z) < 1.0e-4F;
}

} // namespace

/// @brief Places unit cubes at known positions and casts rays along the x axis.
int main() {
	using namespace vve;
	auto engine = Engine<>{};
	auto world = engine.world();
	auto &render = world.get<RenderSystem>();
	const auto grey = LinearColor{.value = Vec3{0.5F, 0.5F, 0.5F}};
	const auto at = [](Vec3 position, Scalar size = 1.0F) {
		return Transform{.translation = Position{.value = position}, .scale = Scale{.value = Vec3{size, size, size}}};
	};
	const auto near_cube = render.addCuboid(Vec3{-0.5F, -0.5F, -0.5F}, Vec3{0.5F, 0.5F, 0.5F}, grey, at(Vec3{0, 0, 0}));
	const auto far_cube = render.addCuboid(Vec3{-0.5F, -0.5F, -0.5F}, Vec3{0.5F, 0.5F, 0.5F}, grey, at(Vec3{5, 0, 0}, 2.0F));
	if (!near_cube || !far_cube) { return 1; }
	// World boxes follow translation and scale.
	const auto near_box = render.objectBounds(*near_cube);
	const auto far_box = render.objectBounds(*far_cube);
	const auto missing_box = render.objectBounds(RenderObjectHandle{});
	if (!near_box || !far_box || !close(far_box->minimum.value, Vec3{4, -1, -1}) || !close(far_box->maximum.value, Vec3{6, 1, 1}) ||
		missing_box || missing_box.error() != Error::missing_object) { return 2; }
	std::println("[RenderPickingTests] far_box={},{},{} .. {},{},{}", far_box->minimum.value.x, far_box->minimum.value.y,
		far_box->minimum.value.z, far_box->maximum.value.x, far_box->maximum.value.y, far_box->maximum.value.z);
	const auto origin = Position{.value = Vec3{-10, 0, 0}};
	const auto along_x = Direction{.value = Vec3{2, 0, 0}}; // Not normalized: distances are still world units.
	// The nearest box wins; hiding it reveals the far one; a short ray or another direction misses.
	const auto first = render.raycast(origin, along_x, 100.0F);
	if (!first || first->object != *near_cube || std::abs(first->distance - 9.5F) > 1.0e-4F) { return 3; }
	if (!render.setObjectVisible(*near_cube, false)) { return 4; }
	const auto second = render.raycast(origin, along_x, 100.0F);
	const auto short_ray = render.raycast(origin, along_x, 10.0F);
	const auto upwards = render.raycast(origin, Direction{.value = Vec3{0, 1, 0}}, 100.0F);
	const auto no_direction = render.raycast(origin, Direction{.value = Vec3{0, 0, 0}}, 100.0F);
	const auto inside = render.raycast(Position{.value = Vec3{5, 0.5F, 0}}, Direction{.value = Vec3{0, 0, -1}}, 100.0F);
	std::println("[RenderPickingTests] first={} second={} short={} up={} inside={}", first->distance,
		second ? second->distance : -1.0F, short_ray.has_value(), upwards.has_value(), inside ? inside->distance : -1.0F);
	if (!second || second->object != *far_cube || std::abs(second->distance - 14.0F) > 1.0e-4F || short_ray || upwards || no_direction ||
		!inside || inside->object != *far_cube || inside->distance != 0.0F) { return 5; }
	// A sprite's box covers its quad in every orientation.
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	{
		std::ofstream file{directory / "white.ppm", std::ios::binary};
		file << "P6\n1 1\n255\n" << "\xff\xff\xff";
	}
	const auto sprite = render.addSprite(Vec2{2, 2}, directory / "white.ppm", grey, at(Vec3{0, 3, 0}));
	const auto sprite_box = sprite ? render.objectBounds(*sprite) : std::unexpected(Error::missing_object);
	const auto radius = std::sqrt(2.0F);
	if (!sprite_box || !close(sprite_box->minimum.value, Vec3{-radius, 3 - radius, -radius}) ||
		!close(sprite_box->maximum.value, Vec3{radius, 3 + radius, radius})) { return 6; }
	return 0;
}
