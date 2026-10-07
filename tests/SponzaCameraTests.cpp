import std;
import VVEngine;
#include "../examples/sponza/SponzaCamera.hpp"

/// @file Checks the authored showcase camera against actual imported castle geometry without initializing Vulkan.
namespace {
/// @brief Composes one local TRS matrix with public facade values.
vve::Mat4 matrix(const vve::Transform &t) {
	const auto q = t.rotation.value;
	const auto x = q.x, y = q.y, z = q.z, w = q.w;
	auto r = vve::identityMat4();
	r[0][0] = 1-2*(y*y+z*z); r[0][1] = 2*(x*y+w*z); r[0][2] = 2*(x*z-w*y);
	r[1][0] = 2*(x*y-w*z); r[1][1] = 1-2*(x*x+z*z); r[1][2] = 2*(y*z+w*x);
	r[2][0] = 2*(x*z+w*y); r[2][1] = 2*(y*z-w*x); r[2][2] = 1-2*(x*x+y*y);
	return vve::math::multiply(vve::math::translate(vve::identityMat4(),t.translation.value),vve::math::scale(r,t.scale.value));
}
/// @brief Checks six perspective planes at the example's 1280x720 startup aspect ratio.
bool visible(vve::Vec3 point,const vve::Camera &camera) {
	const auto forward = vve::math::normalize(camera.forward.value);
	const auto right = vve::math::normalize(vve::math::cross(forward,vve::Vec3{0,1,0}));
	const auto up = vve::math::cross(right,forward);
	const auto offset = vve::math::subtract(point,camera.position.value);
	const auto depth = vve::math::dot(offset,forward), half = depth*std::tan(camera.fov_y.radians/2);
	return depth >= camera.clip.near_plane && depth <= camera.clip.far_plane &&
		std::abs(vve::math::dot(offset,right)) <= half*(16.0F/9.0F) && std::abs(vve::math::dot(offset,up)) <= half;
}
} // namespace

/// @brief Proves startup framing and persistent clipping using the same camera helper as sponza.exe.
int main() {
	namespace camera_config = vve::example::sponza;
	std::cout << std::fixed << std::setprecision(6);
	// Asset loading and input snapshots are available before engine initialization; no GPU or native window is needed.
	auto engine = vve::EngineBuilder<>{}.applicationName("sponza-camera-tests").build();
	auto assets = engine.world().get<vve::AssetSystem>();
	auto input = engine.world().get<vve::WindowSystem>().input();
	vve::DefaultCameraController controller{};
	controller.eye = camera_config::eye;
	controller.move_speed = camera_config::move_speed;
	const auto direction = vve::math::normalize(vve::math::subtract(camera_config::target.value,controller.eye.value));
	controller.yaw = std::atan2(direction.x,-direction.z); controller.pitch = std::asin(direction.y);
	const auto first = camera_config::updateSponzaCamera(controller,input,vve::DeltaTime{0});
	// A subsequent moving frame must retain the example's far plane rather than return controller defaults.
	input.holdKey(static_cast<std::int32_t>(vve::Key::w));
	const auto second = camera_config::updateSponzaCamera(controller,input,vve::DeltaTime{0.1});
	const auto expected_eye = vve::math::add(first.position.value,vve::math::scale(direction,camera_config::move_speed*0.1F));
	const auto movement_error = vve::math::length(vve::math::subtract(second.position.value,expected_eye));
	std::cout << "first_near=" << first.clip.near_plane << " first_far=" << first.clip.far_plane
		<< " second_near=" << second.clip.near_plane << " second_far=" << second.clip.far_plane
		<< " movement_error=" << movement_error << '\n';
	if (camera_config::clip.near_plane <= 0 || camera_config::clip.far_plane <= 100 ||
		first.clip.near_plane != camera_config::clip.near_plane || first.clip.far_plane != camera_config::clip.far_plane ||
		second.clip.near_plane != camera_config::clip.near_plane || second.clip.far_plane != camera_config::clip.far_plane ||
		movement_error > 0.001F || vve::math::dot(vve::math::normalize(first.forward.value),direction) < 0.9999F) { return 1; }
	const auto scene = assets.loadScene(std::filesystem::path{VVE_TEST_SPONZA_SCENE});
	if (!scene) { std::cerr << "scene_error=" << vve::errorName(scene.error()) << '\n'; return 2; }
	const auto root = assets.sceneRootNode(*scene);
	if (!root) { return 3; }
	const auto old = vve::Camera::lookAt(vve::Position{{0,6,9}},vve::Position{{0,1,0}});
	std::size_t castle_vertices{}, castle_visible{}, old_visible{};
	std::vector<std::pair<vve::NodeHandle,vve::Mat4>> pending{{*root,vve::identityMat4()}};
	// Parent*local composition checks the geometry actually placed by scene instantiation.
	while (!pending.empty()) {
		const auto [node,parent] = pending.back(); pending.pop_back();
		const auto transform = assets.nodeTransform(node);
		const auto children = assets.sceneNodeChildren(*scene,node);
		const auto meshes = assets.nodeMeshes(node);
		if (!transform || !children || !meshes) { return 4; }
		const auto world = vve::math::multiply(parent,matrix(*transform));
		for (const auto child : *children) { pending.emplace_back(child,world); }
		for (const auto mesh : *meshes) {
			const auto name = assets.meshName(mesh);
			if (!name) { return 5; }
			if (!name->value.starts_with("Fortress_Fortress_")) { continue; } // Sea, sky and sand do not define castle framing.
			const auto positions = assets.meshPositions(mesh);
			if (!positions) { return 6; }
			for (const auto local : *positions) {
				const auto transformed = vve::math::multiply(world,vve::Vec4{local.x,local.y,local.z,1});
				const auto point = vve::Vec3{transformed.x,transformed.y,transformed.z};
				++castle_vertices; castle_visible += visible(point,first); old_visible += visible(point,old);
			}
		}
	}
	const auto coverage = castle_vertices ? static_cast<double>(castle_visible)/static_cast<double>(castle_vertices) : 0.0;
	std::cout << "castle_vertices=" << castle_vertices << " castle_frustum_vertices=" << castle_visible
		<< " castle_frustum_fraction=" << coverage << " old_castle_frustum_vertices=" << old_visible
		<< " result=" << (coverage >= 0.95 ? "PASS" : "FAIL") << '\n';
	return coverage >= 0.95 ? 0 : 7;
}
