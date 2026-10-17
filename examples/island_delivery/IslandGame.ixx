module;
#include <imgui.h>
#include <VVPPL.h>

/** @file @brief ECS flight, cameras, cockpit instruments and delivery HUD using only VVE's public facade. */
export module Island.Game;
import std;
import VVEngine;
export import Island.Logic;
import Island.Scene;
import Island.Sound;
import Island.Map;

export namespace island {

/** @brief One user system binds the aircraft component to VVE rendering, input, GUI and positional sound. */
struct GameSystem {
	std::filesystem::path assetRoot{}; ///< CMake's downloaded asset directory.
	int frameLimit{}; ///< Zero selects interactive, real-time play.
	int stepsPerFrame{2}; ///< Bounded runs advance this many 120 Hz physics steps per rendered frame.
	unsigned deliveryGoal{1}; ///< Number of completed jobs required by a bounded demonstration.
	bool automatic{}; ///< Fly through the same physics using the built-in pilot.
	bool muted{}; ///< Skip opening an audio device when explicitly requested.
	bool antialias{true}; ///< Four-sample coverage at the default Full HD resolution.
	int cameraMode{}; ///< Chase, cockpit or aircraft showcase.
	bool paused{}, quit{}, fullscreen{}; ///< Interactive session state.
	bool mapOpen{}, openMapOnStart{}; ///< Route map pauses flight while the player reviews airport cargo jobs.
	std::size_t selectedTarget{1}; ///< Airport selected on the map before accepting its cargo assignment.
	std::optional<std::size_t> pendingCargo{}; ///< GUI requests are applied to the authoritative ECS component next frame.
	float mapZoom{1.0F}; ///< Retained map magnification.
	vve::Vec2 mapPan{}; ///< Retained map displacement in world metres.
	float commandedThrottle{}, accumulator{}; ///< Persistent engine lever and unsimulated real time.
	vve::Entity aircraft{}; ///< The ECS entity carrying the authoritative Flight component.
	vve::SceneNodeHandle aircraftNode{}; ///< Scene root of the aircraft's exterior parts.
	vve::WindowHandle window{}; ///< The example's only window.
	vve::ECS *ecs{}; ///< Engine-owned component container.
	vve::WindowSystem *windows{}; ///< Public keyboard and fullscreen controls.
	vve::RenderSystem *render{}; ///< Public Vulkan-backed renderer.
	Soundscape sound{}; ///< Layered recordings and per-voice mixing state.
	Flight displayed{}; ///< Current component copy shared with the GUI callback.
	vve::Camera camera{}; ///< Render camera, also used as the sound listener.
	vve::Vec3 eye{0, 10, 265}; ///< Smoothed chase-camera position.
	std::map<std::size_t, vve::Vector<vve::RenderObjectHandle>> residentIslands{}; ///< Only nearby islands own render resources.
	vve::Vector<vve::RenderObjectHandle> water{}; ///< Fixed pool of ocean tiles following the aircraft.
	std::size_t peakResidentIslands{}; ///< High-water mark for automated streaming verification.

	/** @brief Builds nearby scenery and the aircraft, creates its ECS component and starts the downloaded recordings. */
	template <typename TWorld> std::expected<void, vve::Error> init(TWorld &world) {
		ecs = &world.template get<vve::ECS>();
		windows = &world.template get<vve::WindowSystem>();
		render = &world.template get<vve::RenderSystem>();
		const auto found = windows->findWindow("main");
		if (!found) { return std::unexpected(vve::Error::invalid_handle); }
		window = found->handle();
		if (antialias) {
			if (auto result = render->setAntialiasing(vve::Antialiasing::msaa4); !result) { return result; }
		}
		if (auto result = buildEnvironment(*render, world.template get<vve::AssetSystem>(), assetRoot); !result) {
			std::cerr << "[island] scenery initialization failed\n"; return result;
		}
		displayed.position = vve::math::add(islandCenter(displayed.originIsland), vve::Vec3{0, aircraftGroundHeight, 240});
		eye = vve::math::add(displayed.position, vve::Vec3{0, 5, 25});
		selectedTarget = displayed.targetIsland;
		mapOpen = openMapOnStart || (frameLimit == 0 && !automatic);
		if (auto result = updateEnvironment(*render, assetRoot, displayed.position, residentIslands, water); !result) { return result; }
		peakResidentIslands = residentIslands.size();
		const auto node = render->createNode(render->sceneRoot());
		if (!node) { return std::unexpected(node.error()); }
		aircraftNode = *node;
		if (auto result = buildAircraft(*render, world.template get<vve::AssetSystem>(), aircraftNode, assetRoot); !result) {
			std::cerr << "[island] aircraft import failed\n"; return result;
		}
		aircraft = ecs->create();
		if (auto result = ecs->add(aircraft, displayed); !result) { return result; }
		if (auto result = sound.init(world.template get<vve::AudioSystem>(), assetRoot, muted); !result) { return result; }
		if (sound.deviceUnavailable) { std::cerr << "[island] audio output unavailable: " << world.template get<vve::AudioSystem>().lastError() << '\n'; }
		// A restrained exposure curve preserves photographed surface detail and highlights.
		render->setPostProcessSetup([](vvppl::PostProcessing &pp) {
			pp.addTonemap().exposure = 0.85F;
			auto &vignette = pp.addVignette();
			vignette.intensity = 0.16F;
			vignette.radius = 0.8F;
		});
		world.template get<vve::GuiSystem>().draw([this] { drawHud(); });
		setCamera(0.0F);
		return updateAircraft(*render, aircraftNode, displayed);
	}

	/** @brief Reads held controls; the throttle lever remains at its setting after the key is released. */
	Controls keyboard(const vve::InputState &input, float dt) {
		const auto down = [&input](vve::Key key) { return input.isKeyDown(key) ? 1.0F : 0.0F; };
		commandedThrottle = std::clamp(commandedThrottle + dt * 0.4F * (down(vve::Key::w) - down(vve::Key::s)), 0.0F, 1.0F);
		return {.throttle = commandedThrottle, .pitch = down(vve::Key::down) - down(vve::Key::up),
			.roll = down(vve::Key::right) - down(vve::Key::left), .yaw = down(vve::Key::d) - down(vve::Key::a),
			.brake = down(vve::Key::space)};
	}

	/** @brief Advances the ECS aircraft at 120 Hz, then updates its scene hierarchy, camera and audio listener. */
	template <typename TWorld> std::expected<void, vve::Error> update(TWorld &, const vve::FrameContext &frame) {
		const auto input = windows->input();
		if (input.wasKeyPressed(vve::Key::escape)) { if (mapOpen) { mapOpen = false; } else { paused = !paused; } }
		if (input.wasKeyPressed(vve::key('m'))) { mapOpen = !mapOpen; }
		if (input.wasKeyPressed(vve::key('c'))) { cameraMode = (cameraMode + 1) % 3; }
		if (input.wasKeyPressed(vve::key('p')) && !mapOpen) { automatic = !automatic; commandedThrottle = displayed.throttle; }
		if (input.wasKeyPressed(vve::Key::f11)) {
			if (auto result = windows->setFullscreen(window, !fullscreen); !result) { return result; }
			fullscreen = !fullscreen;
		}
		auto component = ecs->get<Flight>(aircraft);
		if (!component) { return std::unexpected(component.error()); }
		if (input.wasKeyPressed(vve::key('r'))) {
			const auto origin = component->originIsland, target = component->targetIsland;
			const auto completed = component->deliveries;
			*component = Flight{};
			component->originIsland = origin; component->targetIsland = target; component->deliveries = completed;
			component->position = vve::math::add(islandCenter(origin), vve::Vec3{0, aircraftGroundHeight, 240});
			commandedThrottle = accumulator = 0.0F;
			paused = mapOpen = false;
			eye = vve::math::add(component->position, vve::Vec3{0, 5, 25});
		}
		// A new cargo assignment keeps the same aircraft at the airport where the previous job finished.
		if (pendingCargo) {
			if (acceptCargo(*component, *pendingCargo)) { commandedThrottle = 0.0F; accumulator = 0.0F; }
			pendingCargo.reset();
		}
		if (automatic && !mapOpen && !paused && component->phase == Phase::delivered
			&& (frameLimit == 0 || component->deliveries < deliveryGoal)) {
			(void)acceptCargo(*component, suggestedTarget(*component));
			selectedTarget = component->targetIsland;
		}
		const float dt = frameLimit > 0 ? static_cast<float>(stepsPerFrame) / 120.0F
			: std::clamp(static_cast<float>(frame.delta_time.seconds), 0.0F, 0.1F);
		const auto controls = keyboard(input, paused || mapOpen ? 0.0F : dt);
		// Rendering frequency never changes the integrator's fixed simulation step.
		if (!paused && !mapOpen) {
			accumulator += dt;
			while (accumulator + 0.000001F >= 1.0F / 120.0F) {
				step(*component, automatic ? autopilot(*component) : controls, 1.0F / 120.0F);
				accumulator -= 1.0F / 120.0F;
			}
		}
		if (auto result = ecs->put(aircraft, *component); !result) { return result; }
		if (!automatic && component->deliveries > displayed.deliveries) {
			selectedTarget = suggestedTarget(*component);
			mapOpen = true;
		}
		displayed = *component;
		if (auto result = updateEnvironment(*render, assetRoot, displayed.position, residentIslands, water); !result) { return result; }
		peakResidentIslands = std::max(peakResidentIslands, residentIslands.size());
		if (auto result = updateAircraft(*render, aircraftNode, displayed); !result) { return result; }
		setCamera(dt);
		return sound.update(displayed, camera, cameraMode == 1, paused);
	}

	/** @brief Places the chase, pilot-eye or orbit camera with a common kilometre-scale clipping range. */
	void setCamera(float dt) {
		const auto ahead = forward(displayed);
		const vve::Vec3 right{std::cos(displayed.heading), 0, std::sin(displayed.heading)};
		auto target = vve::math::add(displayed.position, vve::math::scale(ahead, 35.0F));
		if (cameraMode == 1) {
			eye = vve::math::add(displayed.position, vve::Vec3{0, 1.15F, 0});
			eye = vve::math::add(eye, vve::math::scale(ahead, 0.25F));
		} else {
			const auto offset = cameraMode == 2 ? vve::math::add(vve::math::scale(right, 20.0F), vve::math::scale(ahead, -12.0F))
				: vve::math::scale(ahead, -24.0F);
			auto desired = vve::math::add(vve::math::add(displayed.position, offset), vve::Vec3{0, 7, 0});
			desired.y = std::max(desired.y, terrainHeight(desired.x, desired.z) + 3.0F);
			const float blend = dt <= 0 ? 1.0F : 1.0F - std::exp(-6.0F * dt);
			eye = vve::math::add(eye, vve::math::scale(vve::math::subtract(desired, eye), blend));
			if (cameraMode == 2) { target = displayed.position; }
		}
		camera = vve::Camera::lookAt(vve::Position{eye}, vve::Position{target}, {}, vve::FovY{0.93F},
			vve::ClipPlanes{.near_plane = 0.1F, .far_plane = 10000.0F});
		render->setCamera(camera);
	}

	/** @brief Draws a readable flight panel, route, airport guidance and an optional pilot instrument panel. */
	void drawHud() {
		const auto &io = ImGui::GetIO();
		const ImVec2 screen = io.DisplaySize;
		const float uiScale = std::clamp(screen.y / 900.0F, 0.65F, 1.3F);
		const auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize
			| ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
		ImGui::SetNextWindowPos(ImVec2{20 * uiScale, 20 * uiScale});
		ImGui::SetNextWindowBgAlpha(0.85F);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.025F, 0.055F, 0.075F, 1});
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{16 * uiScale, 12 * uiScale});
		ImGui::Begin("Island Air", nullptr, flags);
		ImGui::SetWindowFontScale(uiScale);
		ImGui::TextColored(ImVec4{0.95F, 0.73F, 0.32F, 1}, "I S L A N D   A I R");
		ImGui::Text("%s -> %s", islandName(displayed.originIsland).c_str(), islandName(displayed.targetIsland).c_str());
		ImGui::Separator();
		const float speed = vve::math::length(displayed.velocity);
		ImGui::Text("AIRSPEED %3.0f kt    ALTITUDE %4.0f ft", speed * 1.94384F, std::max(0.0F, displayed.position.y) * 3.28084F);
		ImGui::Text("THROTTLE %3.0f %%    DESTINATION %.2f km", displayed.throttle * 100.0F, remainingDistance(displayed) / 1000.0F);
		ImGui::Text("VERTICAL %+.0f ft/min   %s", displayed.velocity.y * 196.8504F, automatic ? "AUTOPILOT" : "MANUAL");
		ImGui::Text("CARGO: %s   |   %s", cargoName(displayed).data(), phaseName(displayed.phase).data());
		ImGui::Text("DELIVERIES %u   |   M: route map   |   %zu islands", displayed.deliveries, islandCount);
		ImGui::End();
		// The destination marker identifies a real apron; flying through it never awards a delivery.
		ImGui::SetNextWindowPos(ImVec2{20 * uiScale, screen.y - 100 * uiScale});
		ImGui::SetNextWindowBgAlpha(0.8F);
		ImGui::Begin("Flight controls", nullptr, flags);
		ImGui::SetWindowFontScale(uiScale);
		ImGui::TextUnformatted("W/S throttle   |   Up/Down pitch   |   Left/Right bank   |   A/D rudder   |   Space brakes");
		ImGui::TextUnformatted("M route map   |   C camera   |   P autopilot   |   R retry job   |   Esc pause   |   F11 fullscreen");
		ImGui::TextUnformatted(displayed.phase == Phase::delivered ? "DELIVERY COMPLETE - open M to collect new cargo and choose your next destination."
			: displayed.phase == Phase::crashed ? "AIRCRAFT LOST - press R to restart."
			: displayed.grounded && displayed.takeoffs > 0 ? "Taxi into the marked unloading area and hold still for two seconds."
			: "Follow the route on M. Land at your assigned island, taxi to the yellow unloading area and stop.");
		ImGui::End();
		if (cameraMode == 1) {
			ImGui::SetNextWindowPos(ImVec2{screen.x * 0.5F, screen.y - 132 * uiScale}, ImGuiCond_Always, ImVec2{0.5F, 1});
			ImGui::SetNextWindowBgAlpha(0.95F);
			ImGui::Begin("Cockpit instruments", nullptr, flags);
			ImGui::SetWindowFontScale(uiScale);
			ImGui::Text("     IAS            ALT            VSI            HDG");
			ImGui::Text("    %03.0f kt       %04.0f ft       %+05.0f         %03.0f deg", speed * 1.94384F,
				displayed.position.y * 3.28084F, displayed.velocity.y * 196.8504F,
				std::fmod(displayed.heading * 180.0F / std::numbers::pi_v<float> + 360.0F, 360.0F));
			ImGui::End();
		}
		if (paused) {
			ImGui::SetNextWindowPos(ImVec2{screen.x * 0.5F, screen.y * 0.5F}, ImGuiCond_Always, ImVec2{0.5F, 0.5F});
			ImGui::Begin("Paused", nullptr, flags);
			ImGui::SetWindowFontScale(uiScale);
			ImGui::TextUnformatted("FLIGHT PAUSED");
			if (ImGui::Button("Resume flight")) { paused = false; }
			ImGui::SameLine();
			if (ImGui::Button("Quit")) { quit = true; }
			ImGui::SliderFloat("Sound", &sound.volume, 0.0F, 1.0F);
			ImGui::End();
		}
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		if (!mapOpen) { drawNavigation(displayed); }
		if (mapOpen) {
			if (auto target = drawMap(displayed, selectedTarget, mapOpen, mapZoom, mapPan)) { pendingCargo = *target; }
		}
	}
};

} // namespace island
