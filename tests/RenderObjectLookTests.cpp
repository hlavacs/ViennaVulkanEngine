/**
 * @file
 * @brief CPU checks for setObjectColor and setObjectTexture: material sharing, release, kept texture or colour, errors.
 */
import std;
import VVEngine;
import VVE.TestSupport;

/// @brief Recolours and retextures two cubes and one imported triangle, checking resource counts after every call.
int main() {
	using namespace vve;
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	std::filesystem::create_directories(directory);
	const auto texture = directory / "white.ppm";
	{
		std::ofstream file{texture, std::ios::binary};
		file << "P6\n1 1\n255\n" << "\xff\xff\xff";
		if (!file) { return 1; }
	}
	auto engine = Engine<>{};
	auto world = engine.world();
	auto &render = world.get<RenderSystem>();
	auto &assets = world.get<AssetSystem>();
	const auto red = LinearColor{.value = Vec3{1, 0, 0}};
	const auto blue = LinearColor{.value = Vec3{0, 0, 1}};
	const auto a = render.addCuboid(Vec3{-1, -1, -1}, Vec3{1, 1, 1}, red);
	const auto b = render.addCuboid(Vec3{-1, -1, -1}, Vec3{1, 1, 1}, red);
	if (!a || !b || render.sceneMaterialCount() != 1U) { return 2; }
	const auto counts = [&](std::string_view stage) {
		std::println("[RenderObjectLookTests] {} materials={} textures={}", stage, render.sceneMaterialCount(), render.sceneTextureCount());
		return std::pair{render.sceneMaterialCount(), render.sceneTextureCount()};
	};
	// Recolouring one cube splits the shared material; recolouring the other one releases the red material.
	if (!render.setObjectColor(*a, blue) || counts("a_blue") != std::pair{std::size_t{2}, std::size_t{0}}) { return 3; }
	if (!render.setObjectColor(*b, blue) || counts("b_blue") != std::pair{std::size_t{1}, std::size_t{0}}) { return 4; }
	// A texture keeps the colour: blue+texture is new, and the plain blue material stays for b.
	if (!render.setObjectTexture(*a, texture) || counts("a_textured") != std::pair{std::size_t{2}, std::size_t{1}}) { return 5; }
	if (!render.setObjectTexture(*b, texture) || counts("b_textured") != std::pair{std::size_t{1}, std::size_t{1}}) { return 6; }
	// A colour keeps the texture, and the same look again changes nothing.
	if (!render.setObjectColor(*a, red) || !render.setObjectColor(*a, red) ||
		counts("a_red_textured") != std::pair{std::size_t{2}, std::size_t{1}}) { return 7; }
	// Errors leave everything unchanged.
	const auto missing_color = render.setObjectColor(RenderObjectHandle{}, red);
	const auto missing_object = render.setObjectTexture(RenderObjectHandle{}, texture);
	const auto missing_file = render.setObjectTexture(*a, directory / "missing.png");
	if (missing_color || missing_color.error() != Error::missing_object || missing_object || missing_object.error() != Error::missing_object ||
		missing_file || missing_file.error() != Error::io_error || counts("errors") != std::pair{std::size_t{2}, std::size_t{1}}) { return 8; }
	// Removing both cubes releases every material and the texture.
	if (!render.removeObject(*a) || !render.removeObject(*b) || counts("removed") != std::pair{std::size_t{0}, std::size_t{0}}) { return 9; }

	// An imported object becomes a plain coloured object; the imported material stays cached for later instances.
	const auto model = test::writeFixture(directory, "triangle.obj", test::triangleObj);
	const auto scene = assets.loadScene(model);
	if (!scene) { return 10; }
	const auto instance = render.instantiateScene(*scene);
	const auto objects = instance ? render.sceneInstanceObjects(*instance) : std::unexpected(Error::missing_object);
	if (!objects || objects->size() != 1U || counts("imported") != std::pair{std::size_t{1}, std::size_t{0}}) { return 11; }
	if (!render.setObjectColor(objects->front(), blue) || counts("imported_blue") != std::pair{std::size_t{2}, std::size_t{0}}) { return 12; }
	return 0;
}
