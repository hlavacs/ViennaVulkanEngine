#include <imgui.h>

import std;
import VEEngine;

/**
 * @file
 * @brief Crate-collector mini game built entirely on the public VVE facade.
 *
 * The player steers the standard camera controller across a grassy plane while
 * crates rain from the sky. Driving the camera onto a crate collects it for a
 * point. The score is shown in an ImGui overlay in the top-left corner.
 */
namespace {

constexpr auto grassTextureRelativePath = "assets/game/plane/grass.jpg"; ///< Tiled ground texture and asset-root sentinel.

constexpr float groundHalfExtent = 20.0F;      ///< Half side length of the square play field in metres.
constexpr float groundTileSize = 4.0F;         ///< Side length of one tiled grass quad in metres.
constexpr float eyeHeight = 1.0F;              ///< Fixed camera height above the plane in metres.
constexpr float collectRadius = 1.2F;          ///< Horizontal distance at which the camera collects a crate.
constexpr float crateHalfSize = 0.5F;          ///< Half side length of a falling crate cube.
constexpr float crateRestY = 0.5F;             ///< Crate centre height once landed; the bottom rests on y=0.
constexpr float crateSpawnY = 24.0F;           ///< Height crates spawn at before falling.
constexpr float gravity = 12.0F;               ///< Downward acceleration applied to falling crates (m/s^2).
constexpr float spawnInterval = 1.3F;          ///< Seconds between crate spawns while below the cap.
constexpr std::size_t maxActiveCrates = 12U;   ///< Maximum simultaneously present crates.
constexpr std::uint32_t windowWidth = 960U;    ///< Window and camera target width in pixels.
constexpr std::uint32_t windowHeight = 540U;   ///< Window and camera target height in pixels.
constexpr vve::Vec3 sunLightDirection{0.35F, -1.0F, 0.26F}; ///< Light-to-scene direction of the sun's light (normalised at use).
constexpr float sunDistance = 80.0F;           ///< Camera-relative distance to the sun; must stay inside the renderer's 100 m far plane.
constexpr float sunRadius = 1.6F;              ///< Sun sphere radius; with sunDistance this spans roughly 2.3 degrees.

/// @brief One falling or landed crate tracked by the game loop.
struct Crate {
	vve::RenderObjectHandle handle{}; ///< Live render-object handle used to move or remove the crate.
	vve::Vec3 position{};             ///< Current world position of the crate centre.
	float velocityY{};                ///< Vertical velocity while falling.
	bool landed{};                    ///< True once the crate has reached the ground.
};

/// @brief Finds the repository-style asset root from either the cwd or executable location.
[[nodiscard]] std::filesystem::path assetRoot(char *argv0) {
	auto containsAssets = [](const std::filesystem::path &candidate) {
		return std::filesystem::exists(candidate / grassTextureRelativePath);
	};
	if (const auto cwd = std::filesystem::current_path(); containsAssets(cwd)) {
		return cwd;
	}
	if (argv0 == nullptr) {
		return {};
	}
	auto executable = std::filesystem::absolute(std::filesystem::path{argv0});
	if (std::filesystem::exists(executable)) {
		executable = std::filesystem::weakly_canonical(executable);
	}
	for (auto candidate = executable.parent_path(); !candidate.empty(); candidate = candidate.parent_path()) {
		if (containsAssets(candidate)) {
			return candidate;
		}
		if (candidate == candidate.root_path()) {
			break;
		}
	}
	return {};
}

/// @brief Reads the optional frame count used by automated example runs.
[[nodiscard]] std::optional<int> frameLimit(int argc, char **argv) {
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
	return std::nullopt;
}

/// @brief Builds a UV-sphere triangle mesh in object space for the sun.
void makeSphere(float radius, std::uint32_t stacks, std::uint32_t slices,
					 vve::Vector<vve::Vec3> &positions, vve::Vector<std::uint32_t> &indices) {
	constexpr float pi = std::numbers::pi_v<float>;
	// Generate one ring of vertices per stack from the north to the south pole.
	for (std::uint32_t stack = 0; stack <= stacks; ++stack) {
		const float phi = pi * static_cast<float>(stack) / static_cast<float>(stacks);
		for (std::uint32_t slice = 0; slice <= slices; ++slice) {
			const float theta = 2.0F * pi * static_cast<float>(slice) / static_cast<float>(slices);
			positions.push_back(vve::Vec3{radius * std::sin(phi) * std::cos(theta), radius * std::cos(phi),
													radius * std::sin(phi) * std::sin(theta)});
		}
	}
	// Connect adjacent rings into two triangles per quad cell.
	const std::uint32_t ringStride = slices + 1U;
	for (std::uint32_t stack = 0; stack < stacks; ++stack) {
		for (std::uint32_t slice = 0; slice < slices; ++slice) {
			const std::uint32_t topLeft = stack * ringStride + slice;
			const std::uint32_t bottomLeft = topLeft + ringStride;
			for (const std::uint32_t index : std::array{topLeft, bottomLeft, topLeft + 1U, topLeft + 1U, bottomLeft, bottomLeft + 1U}) {
				indices.push_back(index);
			}
		}
	}
}

/// @brief Builds the grassy ground, the sun, and the sun's directional light plus ambient.
/// @return The sun's render-object handle so the frame loop can keep it anchored to the camera.
[[nodiscard]] std::expected<vve::RenderObjectHandle, vve::Error> loadScene(vve::RenderSystem render,
																											const std::filesystem::path &root) {
	render.clearScene();

	// Tile the field with grass quads so the single scene texture repeats across the plane.
	const auto grassTexture = root / grassTextureRelativePath;
	const auto tilesPerSide = static_cast<int>(std::lround((groundHalfExtent * 2.0F) / groundTileSize));
	const float halfTile = groundTileSize * 0.5F;
	for (int ix = 0; ix < tilesPerSide; ++ix) {
		for (int iz = 0; iz < tilesPerSide; ++iz) {
			const float centerX = -groundHalfExtent + halfTile + static_cast<float>(ix) * groundTileSize;
			const float centerZ = -groundHalfExtent + halfTile + static_cast<float>(iz) * groundTileSize;
			const vve::Vec3 minimum{centerX - halfTile, -0.1F, centerZ - halfTile};
			const vve::Vec3 maximum{centerX + halfTile, 0.0F, centerZ + halfTile};
			if (auto result = render.addTexturedCuboid(minimum, maximum, grassTexture); !result) {
				return std::unexpected(result.error());
			}
		}
	}

	// The sun is a bright sphere drawn in a constant color; the frame loop pins it far away relative to the camera.
	vve::Vector<vve::Vec3> sunPositions{};
	vve::Vector<std::uint32_t> sunIndices{};
	makeSphere(sunRadius, 24U, 32U, sunPositions, sunIndices);
	const auto sun = render.addTriangleMesh(std::move(sunPositions), std::move(sunIndices),
															vve::LinearColor{.value = vve::Vec3{1.0F, 0.92F, 0.35F}});
	if (!sun) {
		return std::unexpected(sun.error());
	}
	// The sun is the light source itself: flat emissive shading and no shadow thrown onto the field.
	if (auto result = render.setObjectUnlit(*sun, true); !result) {
		return std::unexpected(result.error());
	}
	if (auto result = render.setObjectCastsShadow(*sun, false); !result) {
		return std::unexpected(result.error());
	}

	// The sun casts a warm directional light straight down onto the field, with a touch of sky ambient.
	render.setDirectionalLight(vve::Direction{.value = sunLightDirection},
										vve::LinearColor{.value = vve::Vec3{1.0F, 0.96F, 0.85F}}, vve::LightIntensity{.value = 1.35F},
										vve::LinearColor{.value = vve::Vec3{0.32F, 0.34F, 0.40F}});
	return *sun;
}

} // namespace

/**
 * @brief Runs the crate-collector game using only the public VVE facade.
 */
int main(int argc, char **argv) {
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	std::cout << "[game] engine=" << vve::engineImplementationNamespaceName << '\n';

	const auto activeRenderer = vve::RendererId{.value = "forward"};
	auto engine = vve::EngineBuilder<>{}
						 .applicationName("game")
						 .addWindow(vve::WindowSetup{}
										 .id("main")
										 .title("VVE Crate Collector")
										 .extent(vve::PixelExtent{.width = windowWidth, .height = windowHeight})
										 .renderer(activeRenderer)
										 .resizable(true))
						 .build();

	if (const auto result = engine.init(); !result) {
		std::cerr << "[game] engine init failed: error=" << vve::errorName(result.error()) << '\n';
		return 1;
	}

	auto render = engine.world().get<vve::RenderSystem>();
	const auto sunHandle = loadScene(render, assetRoot(argc > 0 ? argv[0] : nullptr));
	if (!sunHandle) {
		std::cerr << "[game] scene load failed: error=" << vve::errorName(sunHandle.error()) << '\n';
		return 2;
	}
	// Direction from any viewpoint toward the sun: opposite to the light it sends down.
	const auto toSun = vve::math::normalize(vve::math::scale(sunLightDirection, -1.0F));

	// Start the camera on the plane, at eye height, looking toward the centre of the field.
	vve::DefaultCameraController cameraController{};
	cameraController.eye = vve::Position{.value = vve::Vec3{0.0F, eyeHeight, 14.0F}};
	const auto startupForward =
		vve::math::normalize(vve::math::subtract(vve::Vec3{0.0F, eyeHeight, 0.0F}, cameraController.eye.value));
	cameraController.yaw = std::atan2(startupForward.x, -startupForward.z);
	cameraController.pitch = std::asin(startupForward.y);

	std::vector<Crate> crates{};                       ///< Active crates currently in the world.
	int score{};                                       ///< Crates collected so far.
	float spawnTimer{spawnInterval};                   ///< Time accumulator that spawns the first crate immediately.
	std::mt19937 rng{std::random_device{}()};          ///< Random source for crate spawn positions.
	std::uniform_real_distribution<float> place{-groundHalfExtent + 2.0F, groundHalfExtent - 2.0F};

	// The score overlay is drawn every frame in the top-left corner.
	engine.world().get<vve::GuiSystem>().draw([&score, &crates] {
		ImGui::SetNextWindowPos(ImVec2(12.0F, 12.0F), ImGuiCond_Always);
		ImGui::Begin("Score", nullptr,
						 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text("Score: %d", score);
		ImGui::Text("Crates on field: %zu", crates.size());
		ImGui::Separator();
		ImGui::TextUnformatted("WASD move  -  Arrows look  -  Esc quit");
		ImGui::TextUnformatted("Drive into a crate to collect it!");
		ImGui::End();
	});

	const int maxFrames = frameLimit(argc, argv).value_or(0);
	int frame{};
	bool running = true;
	auto lastTime = std::chrono::steady_clock::now();

	while (running && (maxFrames == 0 || frame < maxFrames)) {
		const auto now = std::chrono::steady_clock::now();
		const float dt = std::clamp(std::chrono::duration<float>(now - lastTime).count(), 0.0F, 0.1F);
		lastTime = now;

		// Steer with the standard controller but pin the eye to a fixed height so it stays on the plane.
		const auto input = engine.world().get<vve::WindowSystem>().input();
		cameraController.eye.value.y = eyeHeight;
		const auto steered = cameraController.update(input);
		cameraController.eye.value.y = eyeHeight;
		const auto eyePosition = cameraController.eye;
		const auto camera = vve::Camera::lookAt(
			eyePosition, vve::Position{.value = vve::math::add(eyePosition.value, steered.forward.value)});
		render.setCamera(camera, vve::PixelExtent{.width = windowWidth, .height = windowHeight});
		// Keep the sun at a fixed far offset from the eye so it shows no parallax as the camera moves.
		(void)render.setObjectTransform(*sunHandle, vve::Transform{.translation = vve::Position{
			.value = vve::math::add(eyePosition.value, vve::math::scale(toSun, sunDistance))}});

		// Spawn a fresh crate high above a random spot while the field is not full.
		spawnTimer += dt;
		if (crates.size() < maxActiveCrates && spawnTimer >= spawnInterval) {
			spawnTimer = 0.0F;
			const vve::Vec3 spawnPosition{place(rng), crateSpawnY, place(rng)};
			const vve::Vec3 minimum{-crateHalfSize, -crateHalfSize, -crateHalfSize};
			const vve::Vec3 maximum{crateHalfSize, crateHalfSize, crateHalfSize};
			if (auto added = render.addCuboid(minimum, maximum, vve::LinearColor{.value = vve::Vec3{0.58F, 0.36F, 0.18F}},
														 vve::Transform{.translation = vve::Position{.value = spawnPosition}});
				 added) {
				crates.push_back(Crate{.handle = *added, .position = spawnPosition, .velocityY = 0.0F, .landed = false});
			}
		}

		// Integrate gravity for airborne crates and rest them on the ground when they land.
		for (auto &crate : crates) {
			if (crate.landed) {
				continue;
			}
			crate.velocityY -= gravity * dt;
			crate.position.y += crate.velocityY * dt;
			if (crate.position.y <= crateRestY) {
				crate.position.y = crateRestY;
				crate.velocityY = 0.0F;
				crate.landed = true;
			}
			(void)render.setObjectTransform(crate.handle,
													  vve::Transform{.translation = vve::Position{.value = crate.position}});
		}

		const auto status = engine.step();
		if (!status) {
			std::cerr << "[game] frame failed: error=" << vve::errorName(status.error()) << '\n';
			return 3;
		}
		++frame;
		if (*status == vve::FrameStatus::stopped) {
			break;
		}
		if (input.wasKeyPressed(vve::Key::escape)) {
			running = false;
		}

		// Collect any landed crate the camera has reached, removing it and scoring a point.
		const auto eye = cameraController.eye.value;
		std::erase_if(crates, [&](const Crate &crate) {
			if (!crate.landed) {
				return false;
			}
			const float dx = eye.x - crate.position.x;
			const float dz = eye.z - crate.position.z;
			if ((dx * dx + dz * dz) > (collectRadius * collectRadius)) {
				return false;
			}
			if (render.removeObject(crate.handle)) {
				++score;
				return true;
			}
			return false;
		});
	}

	std::cout << "[game] frames=" << frame << " score=" << score << '\n';
	return 0;
}
