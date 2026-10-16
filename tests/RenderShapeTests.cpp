/**
 * @file
 * @brief CPU checks for addSphere and addMesh: vertex and index counts, outward sphere winding, sharing and errors.
 */
import std;
import VVEngine;
import VVEngine.Simple;

namespace {

/// @brief Counts sphere triangles whose counter-clockwise normal points inwards; poles add degenerate ones, which are skipped.
[[nodiscard]] std::size_t inwardSphereTriangles() {
	using namespace vve;
	auto scene = simple::RenderScene{};
	const auto *mesh = scene.findMesh(scene.addSphereMesh(2.0F));
	std::size_t inward{};
	for (std::size_t first{}; first + 2U < mesh->indices.size(); first += 3U) {
		const auto &a = (*mesh->vertices)[mesh->indices[first]].position;
		const auto &b = (*mesh->vertices)[mesh->indices[first + 1U]].position;
		const auto &c = (*mesh->vertices)[mesh->indices[first + 2U]].position;
		const auto normal = math::cross(math::subtract(b, a), math::subtract(c, a));
		const auto centre = math::add(math::add(a, b), c);
		if (math::length(normal) > 1.0e-6F && math::dot(normal, centre) < 0) { ++inward; }
	}
	return inward;
}

} // namespace

/// @brief Adds spheres and game-built meshes through the facade and checks the scene counts after each call.
int main() {
	using namespace vve;
	if (const auto inward = inwardSphereTriangles(); inward != 0U) { std::println("[RenderShapeTests] inward={}", inward); return 1; }
	auto engine = Engine<>{};
	auto world = engine.world();
	auto &render = world.get<RenderSystem>();
	const auto grey = LinearColor{.value = Vec3{0.5F, 0.5F, 0.5F}};
	// Spheres of one radius share their 33 x 17 vertices; another radius adds a mesh.
	const auto first = render.addSphere(1.0F, grey);
	const auto second = render.addSphere(1.0F, grey, Transform{.translation = Position{.value = Vec3{3, 0, 0}}});
	if (!first || !second || render.sceneMeshCount() != 1U || render.sceneVertexCount() != 561U || render.sceneIndexCount() != 3072U) { return 2; }
	const auto larger = render.addSphere(2.0F, grey);
	const auto zero_radius = render.addSphere(0.0F, grey);
	if (!larger || render.sceneMeshCount() != 2U || zero_radius || zero_radius.error() != Error::invalid_argument) { return 3; }
	std::println("[RenderShapeTests] spheres meshes={} materials={}", render.sceneMeshCount(), render.sceneMaterialCount());
	render.clearScene();

	// A quad with normals and UVs; the same quad again gets its own mesh but shares the material.
	auto quad = MeshDescriptor{.positions = {Vec3{0, 0, 0}, Vec3{1, 0, 0}, Vec3{1, 1, 0}, Vec3{0, 1, 0}},
		.indices = {0, 1, 2, 0, 2, 3}, .normals = Vector<Vec3>(4, Vec3{0, 0, 1}),
		.uvs = {Vec2{0, 0}, Vec2{1, 0}, Vec2{1, 1}, Vec2{0, 1}}, .color = grey};
	const auto mesh = render.addMesh(quad);
	const auto copy = render.addMesh(quad);
	if (!mesh || !copy || render.sceneMeshCount() != 2U || render.sceneMaterialCount() != 1U ||
		render.sceneVertexCount() != 8U || render.sceneIndexCount() != 12U) { return 4; }
	// Empty normals and UVs are allowed; sizes that do not match the positions are not.
	auto flat = quad;
	flat.normals.clear();
	flat.uvs.clear();
	auto bad_normals = quad;
	bad_normals.normals.pop_back();
	auto bad_uvs = quad;
	bad_uvs.uvs.push_back(Vec2{});
	auto bad_index = quad;
	bad_index.indices.back() = 4U;
	auto missing_texture = quad;
	missing_texture.base_color_texture = "missing-mesh-texture.png";
	const auto flat_mesh = render.addMesh(flat);
	const auto errors = std::array{render.addMesh(bad_normals), render.addMesh(bad_uvs), render.addMesh(bad_index), render.addMesh(MeshDescriptor{})};
	const auto texture_error = render.addMesh(missing_texture);
	const bool rejected = std::ranges::all_of(errors, [](const auto &result) { return !result && result.error() == Error::invalid_argument; });
	std::println("[RenderShapeTests] meshes={} flat={} rejected={} texture_error={}", render.sceneMeshCount(), flat_mesh.has_value(), rejected,
		texture_error ? "none" : errorName(texture_error.error()));
	if (!flat_mesh || !rejected || texture_error || texture_error.error() != Error::io_error || render.sceneMeshCount() != 3U) { return 5; }
	// Removing a game-built mesh releases its geometry.
	return render.removeObject(*mesh) && render.sceneMeshCount() == 2U ? 0 : 6;
}
