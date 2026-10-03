/**
 * @file
 * @brief Checks hidden windows, procedural UVs and embedded glTF textures with four coloured quadrants.
 * writeFixtures builds a PNG and GLB; checkCapture inspects their unlit rendering in a 64x64 window.
 */
#include <SDL3/SDL_video.h>
#include <stb_image.h>
#include <vulkan/vulkan_core.h>

import std;
import VVE.TestSupport;
import VVEngine.Simple;

namespace {

/// @brief Writes the same 2x2 PNG separately and inside a GLB with a front-facing textured quad.
[[nodiscard]] bool writeFixtures(const std::filesystem::path &directory) {
	// PNG rows are top-left red, top-right green, bottom-left blue, bottom-right white.
	constexpr std::uint8_t png[]{
		0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
		0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x02, 0x00, 0x00, 0x00, 0xfd, 0xd4, 0x9a,
		0x73, 0x00, 0x00, 0x00, 0x19, 0x49, 0x44, 0x41, 0x54, 0x78, 0x01, 0x01, 0x0e, 0x00, 0xf1, 0xff,
		0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x1f, 0xee,
		0x05, 0xfb, 0xde, 0xdd, 0xec, 0x2b, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
		0x60, 0x82};
	std::filesystem::create_directories(directory);
	auto image = std::ofstream{directory / "quadrants.png", std::ios::binary};
	image.write(reinterpret_cast<const char *>(png), sizeof(png));
	image.close();
	// GLB integers and floats are little-endian, independent of the test host's byte order.
	std::vector<std::uint8_t> binary{};
	const auto appendWord = [](auto &bytes, std::uint32_t word) {
		for (const auto shift : {0U, 8U, 16U, 24U}) { bytes.push_back(static_cast<std::uint8_t>(word >> shift)); }
	};
	// Positions followed by glTF UVs (v=0 at the top), then two counterclockwise triangles.
	for (const float value : std::array{-1.F, -1.F, 0.F, 1.F, -1.F, 0.F, 1.F, 1.F, 0.F, -1.F, 1.F, 0.F,
		0.F, 1.F, 1.F, 1.F, 1.F, 0.F, 0.F, 0.F}) { appendWord(binary, std::bit_cast<std::uint32_t>(value)); }
	for (const auto index : {0U, 1U, 2U, 0U, 2U, 3U}) { appendWord(binary, index); }
	binary.insert(binary.end(), std::begin(png), std::end(png));
	std::string json = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
		"nodes":[{"mesh":0}],"buffers":[{"byteLength":186}],
		"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},
		{"buffer":0,"byteOffset":48,"byteLength":32},{"buffer":0,"byteOffset":80,"byteLength":24},
		{"buffer":0,"byteOffset":104,"byteLength":82}],
		"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[-1,-1,0],"max":[1,1,0]},
		{"bufferView":1,"componentType":5126,"count":4,"type":"VEC2"},
		{"bufferView":2,"componentType":5125,"count":6,"type":"SCALAR"}],
		"images":[{"bufferView":3,"mimeType":"image/png"}],"textures":[{"source":0}],
		"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0},"metallicFactor":0}}],
		"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}]})";
	// Chunk padding is excluded from the buffer and image byte lengths declared above.
	while (json.size() % 4U != 0U) { json.push_back(' '); }
	while (binary.size() % 4U != 0U) { binary.push_back(0U); }
	std::vector<std::uint8_t> glb{};
	for (const auto word : {0x46546c67U, 2U, static_cast<std::uint32_t>(28U + json.size() + binary.size()),
		static_cast<std::uint32_t>(json.size()), 0x4e4f534aU}) { appendWord(glb, word); }
	glb.insert(glb.end(), json.begin(), json.end());
	appendWord(glb, static_cast<std::uint32_t>(binary.size()));
	appendWord(glb, 0x004e4942U);
	glb.insert(glb.end(), binary.begin(), binary.end());
	auto model = std::ofstream{directory / "quad.glb", std::ios::binary};
	model.write(reinterpret_cast<const char *>(glb.data()), static_cast<std::streamsize>(glb.size()));
	model.close();
	return image.good() && model.good();
}

/// @brief Checks extent and quadrant centres, allowing linear filtering around the four texel colours.
[[nodiscard]] int checkCapture(vve::simple::RenderSystem &render, const std::filesystem::path &path) {
	std::filesystem::remove(path);
	const auto captured = render.captureFrameToPng(path);
	std::println("[RenderTextureOrientationTests] capture={} success={}", path.filename().string(), captured.has_value());
	if (!captured) { return 1; }
	int width{}, height{}, channels{};
	auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>{
		stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free};
	std::println("[RenderTextureOrientationTests] extent={}x{} expected=64x64", width, height);
	if (!pixels || width != 64 || height != 64) { return 2; }
	// At distance 3 and a 60-degree field of view the quad spans about 37 pixels; these are texel centres.
	constexpr std::array expected{std::array{255, 0, 0}, std::array{0, 255, 0}, std::array{0, 0, 255}, std::array{255, 255, 255}};
	std::size_t quadrant{};
	bool matches = true;
	for (const auto y : {23, 41}) {
		for (const auto x : {23, 41}) {
			const auto *pixel = vve::test::pixelAt(pixels.get(), width, x, y);
			std::println("[RenderTextureOrientationTests] quadrant={} rgb={},{},{} expected={},{},{}", quadrant,
				pixel[0], pixel[1], pixel[2], expected[quadrant][0], expected[quadrant][1], expected[quadrant][2]);
			for (const auto channel : {0, 1, 2}) {
				matches = matches && std::abs(static_cast<int>(pixel[channel]) - expected[quadrant][channel]) <= 80;
			}
			++quadrant;
		}
	}
	return matches ? 0 : 3;
}

} // namespace

/// @brief Renders a procedural front face and an embedded glTF quad using the same known image.
int main() {
	const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	if (!writeFixtures(directory)) { return 1; }
	auto engine = vve::simple::Engine{vve::simple::Windows{.value = {
		vve::WindowDesc{.id = "main", .extent = {64, 64}, .visible = false}}}};
	if (!engine.init()) { return 2; }
	const auto *window = engine.windowSystem().findWindow("main");
	const bool hidden = window && (SDL_GetWindowFlags(window->native()) & SDL_WINDOW_HIDDEN) != 0U;
	std::println("[RenderTextureOrientationTests] hidden={} expected=true", hidden);
	if (!hidden) { return 3; }
	auto &render = engine.renderSystem();
	render.setCamera(vve::Camera::lookAt(vve::Position{{0.0F, 0.0F, 3.0F}}, vve::Position{}));
	const auto cuboid = render.addTexturedCuboid({-1.0F, -1.0F, -0.5F}, {1.0F, 1.0F, 0.0F}, directory / "quadrants.png");
	if (!cuboid || !render.setObjectUnlit(*cuboid, true) || !engine.renderFrame()) { return 4; }
	if (const auto check = checkCapture(render, directory / "cuboid.png"); check != 0) { return 4 + check; }
	render.clearScene();
	const auto scene = engine.assets().loadScene(directory / "quad.glb");
	std::println("[RenderTextureOrientationTests] embedded_scene_loaded={}", scene.has_value());
	if (!scene) { return 8; }
	const auto materials = engine.assets().sceneMaterials(*scene);
	if (!materials) { return 9; }
	bool embedded{}, embedded_path{};
	// The base-colour source must retain its embedded payload and synthetic image identity.
	for (const auto material : *materials) {
		const auto sources = engine.assets().materialTextureSources(material);
		if (!sources) { return 9; }
		for (const auto &source : *sources) {
			if (source.semantic != vve::simple::MaterialTextureSemantic::base_color) { continue; }
			embedded = source.embedded != nullptr;
			embedded_path = source.path.generic_string().ends_with("#*0");
			std::println("[RenderTextureOrientationTests] embedded={} path={}", embedded, source.path.generic_string());
		}
	}
	if (!embedded) { return 10; }
	if (!embedded_path) { return 11; }
	const auto instance = render.instantiateScene(*scene);
	if (!instance) { return 12; }
	const auto objects = render.sceneInstanceObjects(*instance);
	if (!objects || objects->size() != 1U || !render.setObjectUnlit(objects->front(), true)) { return 13; }
	if (const auto check = checkCapture(render, directory / "embedded.png"); check != 0) { return 13 + check; }
	// Re-upload the retained embedded payload after release, without relying on the original model file.
	if (!engine.renderFrame() || render.sceneTexturePixelBytes() != 0U) { return 18; }
	const auto decodes = render.textureDecodeCount();
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::filesystem::remove(directory / "quad.glb");
	// Reinitialization resets validation diagnostics; preserve coverage of the original captures and teardown.
	if (render.forward().validationErrorCount() != 0U) { return 19; }
	if (!render.initialize(engine.windowSystem()) || !render.renderFrame(engine.windowSystem()) ||
		render.sceneTexturePixelBytes() != 0U || render.gpuTextureCount() != 1U ||
		render.textureDecodeCount() != decodes + 1U) { return 19; }
	if (const auto check = checkCapture(render, directory / "embedded-reloaded.png"); check != 0) { return 19 + check; }
	// Observe validation through resource destruction, not just the captured frames.
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	std::println("[RenderTextureOrientationTests] validation_errors={}", render.forward().validationErrorCount());
	return render.forward().validationErrorCount() == 0U ? 0 : 17;
}
