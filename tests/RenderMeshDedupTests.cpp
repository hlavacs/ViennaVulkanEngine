#include <vulkan/vulkan_core.h>

/**
 * @file
 * @brief Render-mesh CPU sharing, stable GPU cache, and dirty-upload coverage.
 *
 * Functional objects:
 * - main verifies imported instancing shares one CPU/GPU mesh and isolated edits upload once.
 */

import std;

import VEEngine;
import VEEngine.Simple;

/// @brief Verifies instances share mesh storage and mesh edits retain stable handle-keyed uploads.
int main() {
	auto engine = vve::simple::Engine{
		vve::ApplicationName{"render-mesh-dedup-tests"},
		vve::WindowSetups{vve::WindowSetup{}
			.id("main")
			.title("render-mesh-dedup-tests")
			.extent(vve::PixelExtent{.width = 64, .height = 64})
			.renderer(vve::RendererId{.value = "forward"})
			.visible(false)}};
	if (!engine.init()) { return 1; }

	auto &render = engine.renderSystem();
	const auto scene = engine.assets().loadScene(std::filesystem::path{VVE_TEST_MATERIAL_MODEL});
	if (!scene) { return 2; }
	render.clearScene();

	const auto first = render.instantiateScene(*scene);
	const auto first_vertex_count = render.sceneVertexCount();
	const auto second = render.instantiateScene(*scene);
	const auto third = render.instantiateScene(*scene);
	if (!first || !second || !third || render.sceneMeshCount() != 1U ||
		render.sceneInstanceCount() != 3U || render.sceneVertexCount() != first_vertex_count) {
		return 3;
	}
	if (!engine.renderFrame() || render.gpuMeshCount() != 1U || render.gpuMeshUploadCount() != 1U) {
		return 4;
	}
	std::cout << "triple sceneMeshCount=" << render.sceneMeshCount()
		<< " sceneInstanceCount=" << render.sceneInstanceCount()
		<< " sceneVertexCount=" << render.sceneVertexCount()
		<< " gpuMeshCount=" << render.gpuMeshCount()
		<< " gpuMeshUploadCount=" << render.gpuMeshUploadCount() << '\n';

	const auto uploads_before_removal = render.gpuMeshUploadCount();
	const auto material_uploads_before_removal = render.gpuMaterialUploadCount();
	const auto second_objects = render.sceneInstanceObjects(*second);
	if (!second_objects || second_objects->size() != 1U) { return 5; }
	if (const auto removed = render.removeObject(second_objects->front()); !removed) { return 5; }
	if (!engine.renderFrame() || render.gpuMeshCount() != 1U ||
		render.gpuMeshUploadCount() != uploads_before_removal || render.sceneInstanceCount() != 2U) {
		return 6;
	}
	if (render.gpuMaterialUploadCount() != material_uploads_before_removal) { return 10; }
	std::cout << "remove sceneInstanceCount=" << render.sceneInstanceCount()
		<< " gpuMeshCount=" << render.gpuMeshCount()
		<< " gpuMeshUploadCount=" << render.gpuMeshUploadCount()
		<< " gpuMaterialUploadCount=" << render.gpuMaterialUploadCount() << '\n';

	const auto material_uploads_before_addition = render.gpuMaterialUploadCount();
	const auto cuboid = render.addCuboid(vve::Vec3{-0.5F, -0.5F, -0.5F},
		vve::Vec3{0.5F, 0.5F, 0.5F}, vve::LinearColor{.value = vve::Vec3{0.25F, 0.5F, 0.75F}});
	if (!cuboid) { return 7; }
	auto positions = vve::Vector<vve::Vec3>{};
	positions.reserve(24U);
	for (const auto index : std::views::iota(0U, 24U)) {
		const auto offset = static_cast<float>(index) * 0.001F;
		positions.push_back(vve::Vec3{-0.5F + offset, -0.5F, -0.5F});
	}
	if (const auto updated = render.setObjectMeshPositions(*cuboid, std::move(positions)); !updated) {
		return 8;
	}
	const auto vertex_count_before_upload = render.sceneVertexCount();
	if (!engine.renderFrame() || render.gpuMeshCount() != 2U ||
		render.gpuMeshUploadCount() != uploads_before_removal + 1U ||
		render.sceneVertexCount() != vertex_count_before_upload) {
		return 9;
	}
	if (render.gpuMaterialUploadCount() != material_uploads_before_addition + 1U) { return 11; }
	std::cout << "update sceneMeshCount=" << render.sceneMeshCount()
		<< " sceneInstanceCount=" << render.sceneInstanceCount()
		<< " sceneVertexCount=" << render.sceneVertexCount()
		<< " gpuMeshCount=" << render.gpuMeshCount()
		<< " gpuMeshUploadCount=" << render.gpuMeshUploadCount()
		<< " gpuMaterialUploadCount=" << render.gpuMaterialUploadCount() << '\n';
	return 0;
}
