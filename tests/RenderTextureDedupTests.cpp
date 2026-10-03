#include <vulkan/vulkan_core.h>
#include <stb_image.h>

/**
 * @file
 * @brief RenderScene and Vulkan texture-table deduplication coverage.
 *
 * Functional objects:
 * - main verifies deduplication, slot lifetime, pixel release, re-upload and cached decode errors.
 * - checkReleasedSources verifies embedded ownership, generation-gated release and cached HEIGHT classification.
 */

import std;

import VVEngine;
import VVE.TestSupport;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.RenderResources;
import VVEngine.Simple.Scene;

/// @brief Checks pixel capacity release, retained embedded sources and HEIGHT cache hits without a GPU.
static int checkReleasedSources() {
	vve::simple::RenderScene scene;
	// Both accepted and rejected HEIGHT cache hits must work without consulting released pixels.
	for (const bool grey : {false, true}) {
		auto embedded = std::make_shared<vve::simple::EmbeddedImage>();
		embedded->extent = {1, 1};
		embedded->bytes = {std::byte{0}, grey ? std::byte{0} : std::byte{255}, std::byte{0}, std::byte{255}};
		const auto path = std::filesystem::path{grey ? "embedded-grey#*0" : "embedded-colour#*0"};
		const auto slot = scene.acquireTexture(path, vve::simple::MaterialTextureSemantic::normal, embedded);
		if (!slot) { return 42; }
		const auto *texture = scene.findTexture(*slot);
		const auto original = texture->rgba8;
		const auto generation = texture->generation;
		const auto decodes = scene.textureDecodeCount();
		const auto source = std::weak_ptr<const vve::simple::EmbeddedImage>{embedded};
		embedded.reset(); // The render slot must keep the source alive independently of the importer.
		scene.releaseUploadedPixels([](std::size_t) { return 0U; });
		if (texture->rgba8 != original || source.expired()) { return 42; }
		scene.releaseUploadedPixels([&](std::size_t index) { return index == *slot ? generation : 0U; });
		if (!texture->rgba8.empty() || texture->rgba8.capacity() != 0U) { return 42; }
		const auto height = scene.acquireTexture(path, vve::simple::MaterialTextureSemantic::height);
		if ((grey ? height.has_value() || height.error() != vve::Error::invalid_argument : !height || *height != *slot) ||
			scene.textureDecodeCount() != decodes || !texture->rgba8.empty()) { return 43; }
		const auto restored = scene.textureForUpload(*slot);
		if (!restored || (*restored)->rgba8 != original || (*restored)->generation != generation ||
			scene.textureDecodeCount() != decodes + 1U) { return 44; }
		std::println("D4a embedded grey={} released_capacity=0 reload_decodes=1", grey);
	}
	return 0;
}

/// @brief Verifies canonical paths share one decoded CPU entry and one stable GPU image.
int main() {
	auto engine = vve::test::hiddenEngine("render-texture-dedup-tests", vve::PixelExtent{.width = 64, .height = 64});
	if (!engine.init()) { return 1; }

	auto &render = engine.renderSystem();
	render.clearScene();
	const auto texture_a = std::filesystem::path{VVE_TEST_TEXTURE_A};
	const auto texture_b = std::filesystem::path{VVE_TEST_TEXTURE_B};
	// Resolve relative spellings on the asset drive, even when the build tree is on another drive.
	const auto base = texture_a.parent_path().parent_path();
	std::filesystem::current_path(base);
	std::error_code error{};
	const auto relative_a = std::filesystem::relative(texture_a, base, error);
	if (error || relative_a.empty()) { return 2; }
	const auto alternate_a = relative_a.parent_path() / ".." / relative_a.parent_path().filename() /
		relative_a.filename();

	const auto minimum = vve::Vec3{-0.5F, -0.5F, -0.5F};
	const auto maximum = vve::Vec3{0.5F, 0.5F, 0.5F};
	auto first_object = vve::RenderObjectHandle{};
	for (const auto &path : std::array{relative_a, alternate_a, texture_a}) {
		const auto object = render.addTexturedCuboid(minimum, maximum, path);
		if (!object) { return 3; }
		if (!first_object.valid()) { first_object = *object; }
	}
	if (render.sceneTextureCount() != 1U || render.gpuTextureCount() != 0U) { return 4; }
	const auto pixel_bytes = render.sceneTexturePixelBytes();
	if (pixel_bytes == 0U) { return 39; }
	if (!engine.renderFrame() || render.sceneTextureCount() != 1U || render.gpuTextureCount() != 1U) { return 5; }
	std::println("D4a cpu_pixels_before={} after={} gpu_textures={}", pixel_bytes,
		render.sceneTexturePixelBytes(), render.gpuTextureCount());
	if (render.sceneTexturePixelBytes() != 0U || render.gpuTextureCount() != 1U) { return 40; }
	// Resident generations need neither CPU pixels nor another decode during an unchanged frame.
	const auto initial_decodes = render.textureDecodeCount();
	const auto resident = render.forward().uploadedTextureGeneration(0U);
	if (resident == 0U || render.forward().uploadedTextureGeneration(vve::simple::kMaxSceneTextures) != 0U ||
		!engine.renderFrame() || render.textureDecodeCount() != initial_decodes || render.sceneTexturePixelBytes() != 0U ||
		render.gpuTextureCount() != 1U || render.forward().uploadedTextureGeneration(0U) != resident) { return 41; }

	const auto first_image = render.forward().objectTextures[0].image;
	const auto shared_sampler = render.forward().materialSampler;
	const auto texture_b_object = render.addTexturedCuboid(minimum, maximum, texture_b);
	if (first_image == VK_NULL_HANDLE || !texture_b_object ||
		render.sceneTextureCount() != 2U || render.gpuTextureCount() != 1U) {
		return 6;
	}
	if (!engine.renderFrame() || render.gpuTextureCount() != 2U ||
		render.forward().objectTextures[0].image != first_image) {
		return 7;
	}
	const auto second_image = render.forward().objectTextures[1].image;
	const auto repeated_object = render.addTexturedCuboid(minimum, maximum, alternate_a);
	if (second_image == VK_NULL_HANDLE || !repeated_object ||
		render.sceneTextureCount() != 2U) {
		return 8;
	}
	if (!engine.renderFrame() || render.gpuTextureCount() != 2U ||
		render.forward().objectTextures[0].image != first_image ||
		render.forward().objectTextures[1].image != second_image) {
		return 9;
	}

	std::cout << "dedup sceneTextureCount=" << render.sceneTextureCount()
				 << " gpuTextureCount=" << render.gpuTextureCount() << '\n';
	if (!render.removeObject(first_object) || !engine.renderFrame() ||
		render.sceneTextureCount() != 2U || render.gpuTextureCount() != 2U ||
		render.forward().objectTextures[0].image == VK_NULL_HANDLE ||
		render.forward().objectTextures[1].image == VK_NULL_HANDLE ||
		!std::ranges::all_of(render.renderMaterials(), [](const auto &material) {
			return material.base_color_texture_index != vve::simple::kNoTexture;
		})) {
		return 10;
	}
	std::cout << "remove sceneTextureCount=" << render.sceneTextureCount()
				 << " gpuTextureCount=" << render.gpuTextureCount() << '\n';

	render.clearScene();
	const auto replacement = render.addTexturedCuboid(minimum, maximum, texture_b);
	if (!replacement || !engine.renderFrame() || render.sceneTextureCount() != 1U ||
		render.gpuTextureCount() != 1U || render.forward().objectTextures[0].image == VK_NULL_HANDLE ||
		render.sceneInstanceCount() != 1U ||
		render.renderMaterials().front().base_color_texture_index != 0U) {
		return 11;
	}
	std::cout << "clear sceneTextureCount=" << render.sceneTextureCount()
				 << " gpuTextureCount=" << render.gpuTextureCount() << '\n';

	// Two different textures added before one frame share exactly one upload submission.
	render.clearScene();
	const auto submits_before = render.forward().textureUploadSubmitCount();
	if (!render.addTexturedCuboid(minimum, maximum, texture_a) ||
		!render.addTexturedCuboid(minimum, maximum, texture_b) || !engine.renderFrame()) { return 19; }
	const auto submits_after = render.forward().textureUploadSubmitCount();
	std::println("R6 two_textures upload_submits={} expected=1", submits_after - submits_before);
	if (render.gpuTextureCount() != 2U || submits_after != submits_before + 1U ||
		shared_sampler == VK_NULL_HANDLE || render.forward().materialSampler != shared_sampler) { return 20; }
	// An unchanged frame must not submit another upload.
	if (!engine.renderFrame() || render.forward().textureUploadSubmitCount() != submits_after) { return 21; }

	render.clearScene();
	const auto cap_directory = std::filesystem::path{VVE_TEST_TMP_DIR};
	// Clear the previous run first, then retain this run's files for inspection.
	std::filesystem::remove_all(cap_directory, error);
	if (error) { return 15; }
	std::filesystem::create_directories(VVE_TEST_TMP_DIR, error);
	if (error) { return 12; }
	// Missing and corrupt files are decoded once per key until clearScene(), even if repaired on disk.
	const auto missing_path = cap_directory / "missing.ppm";
	const auto missing_before = render.textureDecodeCount();
	for (const auto attempt : {1, 2, 3}) {
		const auto object = render.addTexturedCuboid(minimum, maximum, missing_path);
		std::println("D14 missing attempt={} decodes={}", attempt, render.textureDecodeCount() - missing_before);
		if (object || object.error() != vve::Error::io_error) { return 31; }
	}
	if (render.textureDecodeCount() != missing_before + 1U) { return 32; }
	const auto corrupt_path = cap_directory / "corrupt.ppm";
	{
		auto output = std::ofstream{corrupt_path};
		output << "not an image";
		output.close();
		if (!output) { return 33; }
	}
	const auto corrupt_before = render.textureDecodeCount();
	for (const auto attempt : {1, 2, 3}) {
		const auto object = render.addTexturedCuboid(minimum, maximum, corrupt_path);
		std::println("D14 corrupt attempt={} decodes={}", attempt, render.textureDecodeCount() - corrupt_before);
		if (object || object.error() != vve::Error::io_error ||
			render.textureDecodeCount() != corrupt_before + 1U) { return 34; }
	}
	// Purging and repairing the file must not discard a cached decode failure.
	(void)render.purgeUnusedAssets();
	for (const auto &path : {missing_path, corrupt_path}) {
		std::filesystem::copy_file(texture_a, path, std::filesystem::copy_options::overwrite_existing, error);
		if (error) { return 33; }
		const auto object = render.addTexturedCuboid(minimum, maximum, path);
		if (object || object.error() != vve::Error::io_error ||
			render.textureDecodeCount() != corrupt_before + 1U) { return 35; }
	}
	render.clearScene();
	const auto retry_before = render.textureDecodeCount();
	if (!render.addTexturedCuboid(minimum, maximum, missing_path) ||
		!render.addTexturedCuboid(minimum, maximum, corrupt_path) ||
		render.textureDecodeCount() != retry_before + 2U || render.sceneTextureCount() != 2U) { return 36; }

	// Distinct imported materials sharing a coloured HEIGHT source reuse its decoded normal-map slot.
	render.clearScene();
	const auto height_path = cap_directory / "height.ppm";
	{
		auto image = std::ofstream{height_path, std::ios::binary};
		image << "P6\n1 1\n255\n" << "\x80\x40\xff";
		auto material = std::ofstream{cap_directory / "height.mtl"};
		material << "newmtl A\nKd 1 0 0\nmap_bump height.ppm\n"
			"newmtl B\nKd 0 1 0\nmap_bump height.ppm\n";
		auto object = std::ofstream{cap_directory / "height.obj"};
		object << "mtllib height.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nv 2 0 0\n"
			"o A\nusemtl A\nf 1 2 3\no B\nusemtl B\nf 2 4 3\n";
		image.close(); material.close(); object.close();
		if (!image || !material || !object) { return 33; }
	}
	const auto height_before = render.textureDecodeCount();
	const auto height_scene = engine.assets().loadScene(cap_directory / "height.obj");
	if (!height_scene || !render.instantiateScene(*height_scene) || render.renderMaterials().size() != 2U) { return 37; }
	const auto height_slot = render.renderMaterials().front().normal_texture_index;
	std::println("D14 shared_height decodes={} materials={} textures={}",
		render.textureDecodeCount() - height_before, render.renderMaterials().size(), render.sceneTextureCount());
	if (render.textureDecodeCount() != height_before + 1U || height_slot == vve::simple::kNoTexture ||
		render.renderMaterials().back().normal_texture_index != height_slot || render.sceneTextureCount() != 1U ||
		!render.sceneTextureIsLinear(height_slot).value_or(false)) { return 38; }
	render.clearScene();
	// Every texture slot can be filled; one more texture is reported as an error instead of rendering untextured.
	auto cap_objects = std::vector<vve::RenderObjectHandle>{};
	auto cap_paths = std::vector<std::filesystem::path>{};
	for (std::size_t index{}; index < vve::simple::kMaxSceneTextures + 1U; ++index) {
		const auto path = cap_directory / ("texture-" + std::to_string(index) + ".ppm");
		auto output = std::ofstream{path, std::ios::binary};
		const auto pixels = std::array<char, 12U>{
			static_cast<char>(index), static_cast<char>(index + 1U), static_cast<char>(index + 2U),
			static_cast<char>(index + 3U), static_cast<char>(index + 4U), static_cast<char>(index + 5U),
			static_cast<char>(index + 6U), static_cast<char>(index + 7U), static_cast<char>(index + 8U),
			static_cast<char>(index + 9U), static_cast<char>(index + 10U), static_cast<char>(index + 11U)};
		output << "P6\n2 2\n255\n";
		output.write(pixels.data(), static_cast<std::streamsize>(pixels.size()));
		output.close();
		cap_paths.push_back(path);
		const auto object = render.addTexturedCuboid(minimum, maximum, path);
		const bool expected = index < vve::simple::kMaxSceneTextures ? object.has_value() :
			(!object && object.error() == vve::Error::capacity_exceeded);
		if (!output || !expected) {
			return 13;
		}
		if (object) { cap_objects.push_back(*object); }
	}
	if (render.sceneTextureCount() != vve::simple::kMaxSceneTextures ||
		!engine.renderFrame() || render.gpuTextureCount() != vve::simple::kMaxSceneTextures) {
		return 14;
	}
	std::cout << "cap sceneTextureCount=" << render.sceneTextureCount()
				 << " gpuTextureCount=" << render.gpuTextureCount() << '\n';

	// Removing an object and purging frees its texture slot, which the next texture reuses.
	if (!render.removeObject(cap_objects.front())) {
		return 16;
	}
	(void)render.purgeUnusedAssets();
	const auto reused = render.addTexturedCuboid(minimum, maximum, cap_paths.back());
	if (!reused || render.sceneTextureCount() != vve::simple::kMaxSceneTextures ||
		render.renderMaterials().back().base_color_texture_index != 0U ||
		!engine.renderFrame() || render.gpuTextureCount() != vve::simple::kMaxSceneTextures) {
		return 17;
	}
	std::cout << "reuse sceneTextureCount=" << render.sceneTextureCount()
				 << " gpuTextureCount=" << render.gpuTextureCount() << '\n';
	if (render.forward().materialSampler != shared_sampler) { return 22; }

	// Reuse the same purged slot with a known colour, and hide the other overlapping cuboids.
	const auto green_path = cap_directory / "green.ppm";
	{
		auto output = std::ofstream{green_path, std::ios::binary};
		output << "P6\n2 2\n255\n";
		constexpr unsigned char green[]{0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0};
		output.write(reinterpret_cast<const char *>(green), sizeof(green));
		output.close();
		if (!output) { return 23; }
	}
	if (!render.removeObject(*reused)) { return 24; }
	(void)render.purgeUnusedAssets();
	for (const auto object : cap_objects | std::views::drop(1)) {
		if (!render.setObjectVisible(object, false)) { return 25; }
	}
	const auto green_object = render.addTexturedCuboid(minimum, maximum, green_path);
	if (!green_object || !render.setObjectUnlit(*green_object, true)) { return 26; }
	const auto green_slot = render.renderMaterials().back().base_color_texture_index;
	std::println("[RenderTextureDedupTests] green_reused_slot={} expected=0", green_slot);
	if (green_slot != 0U) { return 27; }
	render.setCamera(vve::Camera::lookAt(vve::Position{{0.0F, 0.0F, 3.0F}}, vve::Position{}));
	// Capture both frame slots, so a stale descriptor in either slot cannot hide behind a correct CPU index.
	for (const auto slot : {0, 1}) {
		const auto path = cap_directory / ("green-" + std::to_string(slot) + ".png");
		std::filesystem::remove(path);
		if (!render.captureFrameToPng(path)) { return 28; }
		int width{}, height{}, channels{};
		auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>{
			stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free};
		std::println("[RenderTextureDedupTests] green_capture={}x{} slot={}", width, height, slot);
		if (!pixels || width != 64 || height != 64) { return 29; }
		const auto *centre = vve::test::pixelAt(pixels.get(), width, width / 2, height / 2);
		std::println("[RenderTextureDedupTests] green_rgb={},{},{} expected=0,255,0", centre[0], centre[1], centre[2]);
		if (centre[0] > 2 || centre[1] < 253 || centre[2] > 2) { return 30; }
	}
	if (const int check = checkReleasedSources(); check != 0) { return check; }
	// Reinitialize with one file-backed slot whose pixels have already been released.
	render.clearScene();
	if (!render.addTexturedCuboid(minimum, maximum, green_path) || !engine.renderFrame() ||
		render.sceneTexturePixelBytes() != 0U) { return 45; }
	const auto reload_decodes = render.textureDecodeCount();
	const auto reload_generation = render.forward().uploadedTextureGeneration(0U);
	const auto reinitialize = [&]() -> std::expected<void, vve::Error> {
		render.waitIdle();
		engine.gui().shutdownVulkan();
		render.shutdown();
		// Check the previous instance, including teardown, before initialization resets its validation counter.
		if (render.forward().validationErrorCount() != 0U) { return std::unexpected(vve::Error::platform_error); }
		return render.initialize(engine.windowSystem());
	};
	if (!reinitialize() || !render.renderFrame(engine.windowSystem()) || render.sceneTexturePixelBytes() != 0U ||
		render.gpuTextureCount() != 1U || render.textureDecodeCount() != reload_decodes + 1U ||
		render.forward().uploadedTextureGeneration(0U) != reload_generation) { return 45; }
	// Losing the file cannot affect resident frames; it must produce io_error when another upload needs it.
	std::filesystem::remove(green_path, error);
	if (error || !render.renderFrame(engine.windowSystem()) || render.textureDecodeCount() != reload_decodes + 1U ||
		render.gpuTextureCount() != 1U || render.sceneTexturePixelBytes() != 0U) { return 46; }
	const auto missing_reload = reinitialize();
	if (missing_reload || missing_reload.error() != vve::Error::io_error || render.gpuTextureCount() != 0U ||
		render.forward().uploadedTextureGeneration(0U) != 0U || render.textureDecodeCount() != reload_decodes + 2U) { return 47; }
	const auto cached_reload = reinitialize();
	if (cached_reload || cached_reload.error() != vve::Error::io_error ||
		render.textureDecodeCount() != reload_decodes + 2U || render.forward().validationErrorCount() != 0U) { return 48; }
	std::println("D4a file_reload_decodes=1 resident_without_source=true missing_reload=io_error cached_failure=true");
#ifndef NDEBUG
	// Include uploads, removals, and slot reuse in the validation error check.
	const auto &renderer = render.forward();
	std::cout << "RenderTextureDedupTests validationActive=" << renderer.validationActive()
		<< " validationErrorCount=" << renderer.validationErrorCount() << '\n';
	if (renderer.validationActive() && renderer.validationErrorCount() != 0U) { return 18; }
#endif
	return 0;
}
