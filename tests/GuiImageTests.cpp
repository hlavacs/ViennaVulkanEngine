/**
 * @file
 * @brief CPU checks for GuiSystem::loadImage: errors, ids before the GUI runs, and the image surviving resource release.
 */
import std;
import VVEngine;

/// @brief Loads one HUD image without a GPU and checks that removals, purges and clearScene keep its texture.
int main() {
	using namespace vve;
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	const auto image = directory / "hud.ppm";
	const auto other = directory / "crate.ppm";
	for (const auto &path : {image, other}) {
		std::ofstream file{path, std::ios::binary};
		file << "P6\n1 1\n255\n" << (path == image ? "\x80\x40\xc8" : "\xff\xff\xff");
		if (!file) { return 1; }
	}
	auto engine = Engine<>{};
	auto world = engine.world();
	auto &gui = world.get<GuiSystem>();
	auto &render = world.get<RenderSystem>();
	const auto missing = gui.loadImage(directory / "missing.png");
	const auto hud = gui.loadImage(image);
	if (missing || missing.error() != Error::io_error || !hud || gui.imageId(*hud) != 0U || gui.imageId(GuiImageHandle{}) != 0U) { return 2; }
	// A textured object comes and goes; the HUD image has no material but keeps its slot.
	const auto crate = render.addTexturedCuboid(Vec3{-1, -1, -1}, Vec3{1, 1, 1}, other);
	if (!crate || render.sceneTextureCount() != 2U || !render.removeObject(*crate)) { return 3; }
	const auto after_remove = render.sceneTextureCount();
	(void)render.purgeUnusedAssets();
	const auto after_purge = render.sceneTextureCount();
	render.clearScene();
	const auto after_clear = render.sceneTextureCount();
	std::println("[GuiImageTests] missing=io_error after_remove={} after_purge={} after_clear={}", after_remove, after_purge, after_clear);
	return after_remove == 1U && after_purge == 1U && after_clear == 1U ? 0 : 4;
}
