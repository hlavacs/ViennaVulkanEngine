import std;
import VEEngine;

/**
 * @file
 * @brief Deterministic facade-renderer smoke scene for light/shadow example wiring.
 */
namespace {

/// @brief Reads the optional frame count used by automated example runs.
[[nodiscard]] int frameLimit(int argc, char **argv) {
	for (int index = 1; index + 1 < argc; ++index) {
		if (argv[index] == nullptr || argv[index + 1] == nullptr) {
			continue;
		}
		if (std::string_view{argv[index]} != "--frames") {
			continue;
		}
		int value{};
		const std::string_view text{argv[index + 1]};
		const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
		if (result.ec == std::errc{} && value >= 0) {
			return value;
		}
	}
	return 1;
}

/// @brief Returns the stable text path used by automatic render verification.
[[nodiscard]] std::filesystem::path outputPath(int argc, char **argv) {
	for (int index = 1; index + 1 < argc; ++index) {
		if (argv[index] != nullptr && argv[index + 1] != nullptr && std::string_view{argv[index]} == "--output") {
			return std::filesystem::path{argv[index + 1]};
		}
	}
	if (argc > 0 && argv[0] != nullptr) {
		const auto executableDirectory = std::filesystem::absolute(std::filesystem::path{argv[0]}).parent_path();
		return executableDirectory.parent_path() / "verify" / "light_shadow_debug.txt";
	}
	return std::filesystem::path{"verify"} / "light_shadow_debug.txt";
}

/// @brief Returns the stable image path next to the verification text file.
[[nodiscard]] std::filesystem::path pngOutputPath(const std::filesystem::path &textPath) {
	const auto baseDirectory = textPath.has_parent_path() ? textPath.parent_path() : std::filesystem::current_path();
	const auto verifyDirectory = baseDirectory.filename() == "verify" ? baseDirectory : baseDirectory / "verify";
	return verifyDirectory / "light_shadow_debug.png";
}

/// @brief Adds the debug floor, shadow caster, light marker, and lights through the facade.
[[nodiscard]] std::expected<void, vve::Error> loadShadowTestScene(vve::RenderSystem renderSystem) {
	const vve::Vec3 warmPointLightPosition{2.0F, 3.7F, -2.4F}; // Warm point light casts from the back-right side.
	const vve::Vec3 coolPointLightPosition{-2.25F, 2.9F, 1.65F}; // Cool point light casts from the front-left side.
	renderSystem.clearScene();
	if (auto result = renderSystem.addPlane(vve::Vec2{4.0F, 4.0F}, vve::LinearColor{.value = vve::Vec3{0.55F, 0.55F, 0.55F}});
		 !result) {
		return std::unexpected(result.error());
	}
	if (auto result = renderSystem.addCuboid(vve::Vec3{-0.4F, -1.0F, -0.4F}, vve::Vec3{0.4F, 1.0F, 0.4F},
														  vve::LinearColor{.value = vve::Vec3{0.55F, 0.55F, 0.55F}},
														  vve::Transform{.translation = vve::Position{.value = vve::Vec3{0.0F, 1.0F, -0.75F}}});
		 !result) {
		return std::unexpected(result.error());
	}
	if (auto result = renderSystem.addCuboid(vve::Vec3{-0.125F, -0.125F, -0.125F}, vve::Vec3{0.125F, 0.125F, 0.125F},
														  vve::LinearColor{.value = vve::Vec3{1.0F, 0.9F, 0.1F}},
														  vve::Transform{.translation = vve::Position{.value = warmPointLightPosition}});
		 !result) {
		return std::unexpected(result.error());
	}
	if (auto result = renderSystem.addCuboid(vve::Vec3{-0.125F, -0.125F, -0.125F}, vve::Vec3{0.125F, 0.125F, 0.125F},
														  vve::LinearColor{.value = vve::Vec3{0.18F, 0.55F, 1.0F}},
														  vve::Transform{.translation = vve::Position{.value = coolPointLightPosition}});
		 !result) {
		return std::unexpected(result.error());
	}
	renderSystem.addPointLight(vve::Position{.value = warmPointLightPosition},
										vve::LinearColor{.value = vve::Vec3{1.0F, 0.92F, 0.55F}},
										vve::LightIntensity{.value = 4.5F}, vve::LightRange{.value = 7.5F},
										vve::LinearColor{.value = vve::Vec3{0.08F, 0.08F, 0.08F}});
	renderSystem.addPointLight(vve::Position{.value = coolPointLightPosition},
										vve::LinearColor{.value = vve::Vec3{0.30F, 0.58F, 1.0F}},
										vve::LightIntensity{.value = 3.8F}, vve::LightRange{.value = 6.5F},
										vve::LinearColor{.value = vve::Vec3{0.025F, 0.035F, 0.055F}});
	renderSystem.setDirectionalLight(vve::Direction{.value = vve::Vec3{-0.55F, -0.78F, 0.30F}},
											  vve::LinearColor{.value = vve::Vec3{0.65F, 0.82F, 1.0F}},
											  vve::LightIntensity{.value = 0.75F},
											  vve::LinearColor{.value = vve::Vec3{0.025F, 0.025F, 0.025F}});
	renderSystem.setSpotLight(vve::Position{.value = vve::Vec3{1.45F, 4.8F, -1.45F}},
										vve::Direction{.value = vve::Vec3{0.10F, -0.98F, -0.16F}},
										vve::LinearColor{.value = vve::Vec3{1.0F, 0.58F, 0.38F}},
										vve::LightIntensity{.value = 2.2F}, vve::LightRange{.value = 5.8F},
										vve::SpotConeAngle{.radians = 0.58F},
										vve::LinearColor{.value = vve::Vec3{0.02F, 0.02F, 0.02F}});
	renderSystem.addSpotLight(vve::Position{.value = vve::Vec3{-1.55F, 4.6F, 1.25F}}, // Second cone starts on the opposite side.
										vve::Direction{.value = vve::Vec3{0.40F, -0.85F, -0.32F}}, // Aims across the plane for a separate shadow.
										vve::LinearColor{.value = vve::Vec3{0.38F, 0.62F, 1.0F}}, // Cool tint distinguishes the added light.
										vve::LightIntensity{.value = 2.0F}, vve::LightRange{.value = 6.0F},
										vve::SpotConeAngle{.radians = 0.58F},
										vve::LinearColor{.value = vve::Vec3{0.015F, 0.015F, 0.02F}});
	const auto lifetimeDemoObject = renderSystem.addCuboid(vve::Vec3{-0.05F, -0.05F, -0.05F}, vve::Vec3{0.05F, 0.05F, 0.05F},
																			 vve::LinearColor{.value = vve::Vec3{0.9F, 0.9F, 0.9F}},
																			 vve::Transform{.translation = vve::Position{.value = vve::Vec3{0.0F, 0.2F, 0.0F}}});
	if (!lifetimeDemoObject) {
		return std::unexpected(lifetimeDemoObject.error());
	}
	const auto lifetimeDemoHandle = *lifetimeDemoObject; // Temporary object demonstrates facade lifetime control.
	if (auto result = renderSystem.setObjectVisible(lifetimeDemoHandle, false); !result) {
		return std::unexpected(result.error());
	}
	if (auto result = renderSystem.setObjectVisible(lifetimeDemoHandle, true); !result) {
		return std::unexpected(result.error());
	}
	if (auto result = renderSystem.setObjectTransform(lifetimeDemoHandle,
																	  vve::Transform{.translation = vve::Position{.value = vve::Vec3{0.1F, 0.2F, 0.0F}}});
		 !result) {
		return std::unexpected(result.error());
	}
	if (auto result = renderSystem.removeObject(lifetimeDemoHandle); !result) {
		return std::unexpected(result.error());
	}
	return {};
}

} // namespace

int main(int argc, char **argv) {
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	std::cout << "[light_shadow_debug] engine=" << vve::engineImplementationNamespaceName << '\n';

	auto engine = vve::makeEngine(
		vve::ApplicationName{"light_shadow_debug"},
		vve::WindowSetups{vve::WindowSetup{}
								.id("main")
								.title("VVE Simple Light Shadow Debug")
								.extent(vve::PixelExtent{.width = 960, .height = 540})
								.renderer(vve::RendererId{.value = "forward"})});

	if (const auto result = engine.init(); !result) {
		std::cerr << "[light_shadow_debug] engine init failed: error=" << vve::errorName(result.error()) << '\n';
		return 1;
	}

	auto renderSystem = engine.world().get<vve::RenderSystem>();
	if (const auto result = loadShadowTestScene(renderSystem); !result) {
		std::cerr << "[light_shadow_debug] scene load failed: error=" << vve::errorName(result.error()) << '\n';
		return 2;
	}

	const int maxFrames = frameLimit(argc, argv);
	const auto path = outputPath(argc, argv);
	const auto pngPath = pngOutputPath(path);
	bool pngWritten{};
	for (int frame{}; frame < maxFrames; ++frame) {
		const auto status = engine.step();
		if (!status) {
			std::cerr << "[light_shadow_debug] frame failed: error=" << vve::errorName(status.error()) << '\n';
			return 3;
		}
		if (!pngWritten) {
			const auto captureResult = renderSystem.captureFrameToPng(pngPath);
			if (!captureResult) {
				std::cerr << "[light_shadow_debug] capture failed: error=" << vve::errorName(captureResult.error())
							 << ", path=" << pngPath.string() << '\n';
				return 4;
			}
			pngWritten = true;
			std::cout << "[light_shadow_debug] png_written=" << pngWritten << " path=" << pngPath.string() << '\n';
		}
		if (*status == vve::FrameStatus::stopped) { break; }
	}

	if (auto parent = path.parent_path(); !parent.empty()) {
		std::filesystem::create_directories(parent);
	}
	std::ofstream output{path, std::ios::trunc};
	if (!output) {
		std::cerr << "[light_shadow_debug] could not write output: " << path.string() << '\n';
		return 5;
	}
	output << "engine=" << vve::engineImplementationNamespaceName << '\n';
	output << "frames=" << maxFrames << '\n';
	output << "renderer=facade-forward\n";
	output << "scene_instances=" << renderSystem.sceneInstanceCount() << '\n';
	output << "scene_meshes=" << renderSystem.sceneMeshCount() << '\n';
	output << "directional_light=" << renderSystem.hasSceneDirectionalLight() << '\n';
	output << "point_light=" << renderSystem.hasScenePointLight() << '\n';
	output << "spot_light=" << renderSystem.hasSceneSpotLight() << '\n';
	const auto spotShadowSampleCount = renderSystem.sceneSpotShadowDepthSampleCount(); // Facade-reported spot debug rows.
	output << "spot_shadow_sample_count=" << spotShadowSampleCount << '\n';
	std::vector<std::size_t> spotShadowSlots; // Present facade slots are checked after preserving per-sample rows.
	for (std::size_t spotIndex{}; spotIndex < spotShadowSampleCount; ++spotIndex) { // One row per shadowed spot light.
		const auto sample = renderSystem.sceneSpotShadowDepthSample(spotIndex); // Optional aggregate supplies light-space NDC only.
		const auto slot = renderSystem.sceneSpotShadowDepthSampleSlot(spotIndex); // Scalar getter proves the selected shadow slot.
		const auto expectedDepth = renderSystem.sceneSpotShadowDepthSampleExpectedDepth(spotIndex); // Scalar getter reports compared depth.
		const auto bias = renderSystem.sceneSpotShadowDepthSampleBias(spotIndex); // Scalar getter reports compare bias.
		const auto shadowFactor = renderSystem.sceneSpotShadowDepthSampleFactor(spotIndex); // Scalar getter reports final visibility.
		const auto gpuDepth = renderSystem.sceneSpotShadowDepthGpuDepth(spotIndex); // GPU readback depth, if available.
		const auto hasGpu = renderSystem.sceneSpotShadowDepthHasGpu(spotIndex); // Explicit GPU-readback availability flag.
		const auto depthError = renderSystem.sceneSpotShadowDepthError(spotIndex); // Difference between CPU and GPU depths.
		const bool hasGpuDepth = hasGpu.value_or(false); // Missing readback stays deterministic for headless runs.
		if (slot) { spotShadowSlots.push_back(*slot); } // Only reachable samples can prove slot ownership.
		output << "spot_shadow_sample index=" << spotIndex << " slot=" << (slot ? std::to_string(*slot) : "null");
		if (sample) {
			output << " light_ndc=(" << sample->light_ndc.x << ',' << sample->light_ndc.y << ',' << sample->light_ndc.z << ')';
		} else {
			output << " light_ndc=null";
		}
		output << " expected_depth=" << (expectedDepth ? std::to_string(*expectedDepth) : "null")
				 << " bias=" << (bias ? std::to_string(*bias) : "null")
				 << " shadow_factor=" << (shadowFactor ? std::to_string(*shadowFactor) : "null")
				 << " gpu_depth=" << (hasGpuDepth && gpuDepth ? std::to_string(*gpuDepth) : "none")
				 << " has_gpu=" << (hasGpuDepth ? '1' : '0')
				 << " error=" << (hasGpuDepth && depthError ? std::to_string(*depthError) : "none") << '\n';
	}
	std::ranges::sort(spotShadowSlots); // Stable aggregate verdict for automated parsing.
	const bool enoughSpotSlots = spotShadowSlots.size() >= 2;
	const bool distinctSpotSlots = enoughSpotSlots && std::ranges::adjacent_find(spotShadowSlots) == spotShadowSlots.end();
	output << "spot_shadow_slots_distinct=" << (enoughSpotSlots ? (distinctSpotSlots ? "1" : "0") : "none") << '\n';
	const auto pointShadowSampleCount = renderSystem.scenePointShadowDepthSampleCount(); // Facade-reported point debug rows.
	output << "point_shadow_sample_count=" << pointShadowSampleCount << '\n';
	for (std::size_t pointIndex{}; pointIndex < pointShadowSampleCount; ++pointIndex) { // One row per shadowed point light.
		const auto sample = renderSystem.scenePointShadowDepthSample(pointIndex); // Aggregate supplies slot, face, world, and depth terms.
		const auto gpuDepth = renderSystem.scenePointShadowDepthGpuDepth(pointIndex); // GPU readback depth, if available.
		const auto hasGpu = renderSystem.scenePointShadowDepthHasGpu(pointIndex); // Explicit GPU-readback availability flag.
		const auto depthError = renderSystem.scenePointShadowDepthError(pointIndex); // Difference between CPU and GPU depths.
		const bool hasGpuDepth = hasGpu.value_or(false); // Missing readback stays deterministic for headless runs.
		output << "point_shadow_sample index=" << pointIndex
				 << " slot=" << (sample ? std::to_string(sample->triangle_id) : "null")
				 << " face=" << (sample ? std::to_string(sample->face_index) : "null")
				 << " layer=" << (sample ? std::to_string(sample->pixel_x) : "null");
		if (sample) {
			output << " world=(" << sample->world.x << ',' << sample->world.y << ',' << sample->world.z << ')'
					 << " light_ndc=(" << sample->light_ndc.x << ',' << sample->light_ndc.y << ',' << sample->light_ndc.z << ')';
		} else {
			output << " world=null light_ndc=null";
		}
		output << " expected_depth=" << (sample ? std::to_string(sample->expected_depth) : "null")
				 << " bias=" << (sample ? std::to_string(sample->bias) : "null")
				 << " shadow_factor=" << (sample ? std::to_string(sample->shadow_factor) : "null")
				 << " gpu_depth=" << (hasGpuDepth && gpuDepth ? std::to_string(*gpuDepth) : "none")
				 << " has_gpu=" << (hasGpuDepth ? '1' : '0')
				 << " error=" << (hasGpuDepth && depthError ? std::to_string(*depthError) : "none") << '\n';
	}
	const auto directionalShadowSampleCount = renderSystem.sceneShadowDepthSampleCount(); // Facade-reported directional debug rows.
	const auto directionalSample = directionalShadowSampleCount > 0U ? renderSystem.sceneShadowDepthSample(0U) : std::nullopt;
	if (directionalSample) { // Present directional debug data mirrors the spot row format for parsers.
		const bool hasGpuDepth = directionalSample->has_gpu; // Missing readback stays deterministic for headless runs.
		output << "directional_shadow_sample index=0 slot=" << directionalSample->face_index
				 << " light_ndc=(" << directionalSample->light_ndc.x << ',' << directionalSample->light_ndc.y << ','
				 << directionalSample->light_ndc.z << ')'
				 << " expected_depth=" << directionalSample->expected_depth << " bias=" << directionalSample->bias
				 << " shadow_factor=" << directionalSample->shadow_factor
				 << " gpu_depth=" << (hasGpuDepth ? std::to_string(directionalSample->gpu_depth) : "none")
				 << " has_gpu=" << (hasGpuDepth ? '1' : '0')
				 << " error=" << (hasGpuDepth ? std::to_string(directionalSample->error) : "none") << '\n';
	} else { // Stable absent row keeps directional samples machine-parseable when no data exists.
		output << "directional_shadow_sample index=0 slot=null light_ndc=null expected_depth=null"
				 << " bias=null shadow_factor=null gpu_depth=none has_gpu=0 error=none\n";
	}
	output << "png_written=" << pngWritten << '\n';
	std::cout << "[light_shadow_debug] frames=" << maxFrames << '\n';
	return 0;
}
