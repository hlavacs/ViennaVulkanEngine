module;
#include <imgui.h>

/**
 * @file
 * @brief Ring Racer ViewSystem: car rigs, ghost, cameras, lights and sky, smoke, HUD with minimap and logo, and music.
 */
export module Racer.View;
import std;
import VVEngine;
import Racer.Race;
import Racer.Scene;

export namespace racer {

/// @brief Everything seen and heard: car rigs, ghost, camera, lights, smoke, HUD and music.
struct ViewSystem {
	std::filesystem::path root{};				///< Asset root.
	std::vector<racer::CarModel> models{};	///< One loaded car model per car slot; the ghost uses the player's.
	std::map<vve::Entity, racer::CarRig> rigs{};	///< One rig per car entity of the current race.
	racer::CarRig ghostRig{};					///< See-through car replaying the best lap.
	racer::SmokePool smoke{};					///< Smoke and dust sprites.
	vve::GuiImageHandle logo{};				///< Menu logo.
	vve::WindowHandle window{};				///< The game window.
	int builtGeneration{-1};					///< Race generation the rigs belong to.
	View view{View::chase};					///< Current camera view.
	bool night{};								///< Night mode with headlights.
	bool skyShown{};							///< Whether the day skybox is set.
	std::size_t sceneryCount{};				///< Trees, bushes, plants and rocks placed beside the track.
	bool fullscreen{};							///< Window state.
	bool photo{};								///< Photo mode: free orbit camera, simulation paused.
	float orbitYaw{}, orbitPitch{0.4F}, orbitDistance{9.0F};	///< Photo camera around the player car.
	std::string picked{};						///< Description of the car clicked in photo mode.
	vve::Vec3 eye{0, 20, 40};					///< Smoothed camera position.
	std::vector<racer::RaceCar> cars{};		///< Copy of the simulation state for this frame.
	vve::AudioSystem *audio{};				///< Music player, null without an audio device.
	vve::GuiSystem *gui{};					///< Engine-owned GUI, for the logo id.
	vve::WindowSystem *windows{};			///< Engine-owned windows, for mouse capture and fullscreen.
	bool mouseCaptured{};						///< Current capture state.
	std::array<vve::SoundHandle, musicFiles.size()> songs{};	///< Loaded tracks.
	vve::AudioPlaybackHandle music{};		///< Playing track.
	int track{};									///< Selected track.
	float volume{0.35F};						///< Master volume.

	/// @brief Builds the circuit, loads the car, smoke, logo and music, and installs the HUD.
	template <typename TWorld> std::expected<void, vve::Error> init(TWorld &world) {
		auto &race = world.template get<RaceSystem>();
		auto &render = world.template get<vve::RenderSystem>();
		auto &assets = world.template get<vve::AssetSystem>();
		night = race.options.night;
		view = static_cast<View>(race.options.camera);
		gui = &world.template get<vve::GuiSystem>();
		windows = &world.template get<vve::WindowSystem>();
		window = windows->findWindow("main").value().handle();
		if (const auto result = racer::buildCircuit(render, assets, race.track, root); !result) { return result; }
		const auto scenery = racer::addScenery(render, assets, race.track, root, 11U);
		if (!scenery) { return std::unexpected(scenery.error()); }
		sceneryCount = *scenery;
		for (const auto *file : carFiles) {
			const auto loaded = racer::loadCarModel(assets, root / "assets/racer/cars" / file, race.params.wheelBase);
			if (!loaded) { return std::unexpected(loaded.error()); }
			models.push_back(*loaded);
			race.carSizes.emplace_back(loaded->halfLength(), loaded->halfWidth());
		}
		if (const auto result = smoke.create(render, root / "assets/racer/smoke.tga", 40U); !result) { return result; }
		const auto image = gui->loadImage(root / "assets/racer/logo.tga");
		if (!image) { return std::unexpected(image.error()); }
		logo = *image;
		auto &sound = world.template get<vve::AudioSystem>();
		if (sound.init()) {
			audio = &sound;
			for (std::size_t i{}; i < musicFiles.size(); ++i) {
				if (const auto song = sound.loadSound(root / "assets/sounds" / musicFiles[i], vve::AudioLoadMode::on_demand)) { songs[i] = *song; }
			}
			(void)sound.setMasterVolume(vve::AudioVolume{.value = volume});
			playMusic();
		}
		gui->draw([this, &race] { drawHud(race); });
		return {};
	}

	/// @brief Starts the selected track in a loop.
	void playMusic() {
		if (audio == nullptr || !songs[track].valid()) { return; }
		if (music.valid()) { (void)audio->stop(music); }
		music = audio->play(songs[track], vve::AudioPlaybackOptions{.loop = true}).value_or(vve::AudioPlaybackHandle{});
	}

	/// @brief Mirrors the simulation into the scene and handles the view keys.
	template <typename TWorld> std::expected<void, vve::Error> update(TWorld &world, const vve::FrameContext &frame) {
		auto &race = world.template get<RaceSystem>();
		auto &render = world.template get<vve::RenderSystem>();
		auto &ecs = world.template get<vve::ECS>();
		const auto input = windows->input();
		const float dt = race.options.frames > 0 ? 1.0F / 60.0F : std::clamp(static_cast<float>(frame.delta_time.seconds), 0.0F, 0.05F);
		// A new race: replace every car rig and the ghost.
		if (builtGeneration != race.generation) {
			for (const auto &[entity, rig] : rigs) { (void)render.removeSceneInstance(rig.instance); }
			rigs.clear();
			if (ghostRig.instance.valid()) { (void)render.removeSceneInstance(ghostRig.instance); ghostRig = {}; }
			std::size_t slot{};
			for (const auto entity : ecs.template view<racer::RaceCar>()) {
				auto rig = racer::addCar(render, models[slot++ % models.size()]);
				if (!rig) { return std::unexpected(rig.error()); }
				rigs.emplace(entity, std::move(*rig));
			}
			if (race.mode == Mode::timeTrial) {
				auto rig = racer::addCar(render, models.front());
				if (!rig) { return std::unexpected(rig.error()); }
				ghostRig = std::move(*rig);
				racer::setCarOpacity(render, ghostRig, 0.4F);
			}
			builtGeneration = race.generation;
		}
		cars.clear();
		for (const auto entity : ecs.template view<racer::RaceCar>()) { cars.push_back(ecs.template get<racer::RaceCar>(entity).value()); }
		// View keys work in every phase except the menu.
		if (race.phase != Phase::menu) {
			if (input.wasKeyPressed(vve::key('c'))) { view = static_cast<View>((static_cast<int>(view) + 1) % 3); }
			if (input.wasKeyPressed(vve::key('n'))) { night = !night; }
			if (input.wasKeyPressed(vve::key('p'))) { photo = !photo; race.paused = photo; picked.clear(); }
		}
		if (input.wasKeyPressed(vve::Key::f11) && windows->setFullscreen(window, !fullscreen)) { fullscreen = !fullscreen; }
		// Cars, brake lights and smoke.
		std::size_t index{};
		for (auto &[entity, rig] : rigs) {
			const auto &car = cars[index++].vehicle;
			racer::placeCar(render, modelOf(index - 1U), rig, {.position = car.position, .heading = car.heading, .steer = car.steer, .wheelAngle = car.wheelAngle});
			racer::setBrakeLights(render, rig, car.braking);
			const bool sliding = car.slip > 2.5F || (car.braking && car.forwardSpeed() > 15.0F) || (car.offTrack && car.forwardSpeed() > 5.0F);
			if (sliding && !race.paused && (frame.frame_index.value + index) % 3U == 0U) {
				const auto back = racer::world(car.position - racer::headingVector(car.heading) * 1.8F, 0.3F);
				smoke.emit(back, car.offTrack);
			}
		}
		smoke.update(render, race.paused ? 0.0F : dt);
		// The ghost replays the best lap in step with the player's current lap.
		if (ghostRig.instance.valid() && !cars.empty()) {
			const bool show = !race.ghost.empty() && race.phase == Phase::racing;
			racer::setCarOpacity(render, ghostRig, show ? 0.4F : 0.0F);
			if (show) { racer::placeCar(render, models.front(), ghostRig, race.ghost.at(race.raceTime - cars.front().progress.lapStart)); }
		}
		if (const auto lit = updateLights(render); !lit) { return lit; }
		updateCamera(render, input, dt);
		return {};
	}

	/// @brief The car model of a car slot.
	[[nodiscard]] const racer::CarModel &modelOf(std::size_t slot) const { return models[slot % models.size()]; }

	/// @brief Day: the sky photo and its sun with shadows. Night: no sky, a dim moon, and two headlights on every car.
	std::expected<void, vve::Error> updateLights(vve::RenderSystem &render) {
		if (skyShown == night) {
			if (night) { render.clearSkybox(); }
			else if (const auto result = render.setSkybox(root / "assets/racer/sky.jpg"); !result) { return result; }
			skyShown = !night;
		}
		render.clearLights();
		render.setBackgroundColor(vve::LinearColor{.value = night ? vve::Vec3{0.01F, 0.015F, 0.04F} : vve::Vec3{0.45F, 0.68F, 0.95F}});
		render.setDirectionalLight(vve::Direction{.value = sunDirection},
			vve::LinearColor{.value = night ? vve::Vec3{0.5F, 0.6F, 0.9F} : vve::Vec3{1.0F, 0.95F, 0.85F}},
			vve::LightIntensity{.value = night ? 0.08F : 1.3F},
			vve::LinearColor{.value = night ? vve::Vec3{0.02F, 0.02F, 0.03F} : vve::Vec3{0.3F, 0.32F, 0.38F}});
		if (!night) { return {}; }
		// Two spot lights per car: four cars use eight of the renderer's ten shadowed spot lights.
		for (std::size_t slot{}; slot < cars.size(); ++slot) {
			const auto &car = cars[slot].vehicle;
			const auto forward = racer::headingVector(car.heading);
			for (const float side : {-0.6F, 0.6F}) {
				const auto lamp = racer::world(car.position + forward * (modelOf(slot).front() + 0.1F) + racer::rightOf(forward) * side, 0.7F);
				render.addSpotLight(vve::Position{.value = lamp}, vve::Direction{.value = vve::Vec3{forward.x, -0.12F, forward.y}},
					vve::LinearColor{.value = vve::Vec3{1.0F, 0.95F, 0.8F}}, vve::LightIntensity{.value = 4.0F}, vve::LightRange{.value = 45.0F},
					vve::SpotConeAngle{.radians = 0.45F}, vve::LinearColor{.value = vve::Vec3{0.0F, 0.0F, 0.0F}});
			}
		}
		return {};
	}

	/// @brief Chase, hood or top camera behind the player car; in photo mode an orbit camera with mouse picking.
	void updateCamera(vve::RenderSystem &render, const vve::InputState &input, float dt) {
		if (cars.empty()) {
			render.setCamera(vve::Camera::lookAt(vve::Position{.value = vve::Vec3{0, 70, 150}}, vve::Position{}, {}, {},
				vve::ClipPlanes{.near_plane = 1.0F, .far_plane = 1500.0F}));
			return;
		}
		const auto &car = cars.front().vehicle;
		const auto forward = racer::headingVector(car.heading);
		const auto target = racer::world(car.position, 1.0F);
		const vve::ClipPlanes clip{.near_plane = view == View::top ? 1.0F : 0.2F, .far_plane = 400.0F};
		if (photo) {
			// Hold the right button to orbit; the wheel zooms; a left click picks a car under the mouse.
			const bool orbiting = input.isKeyDown(vve::Key::mouse_right);
			captureMouse(orbiting);
			const auto delta = input.mouseDelta(window);
			if (orbiting) { orbitYaw += delta.x * 4.0F; orbitPitch = std::clamp(orbitPitch + delta.y * 3.0F, 0.05F, 1.4F); }
			orbitDistance = std::clamp(orbitDistance - input.mouseWheelDelta(window).y, 4.0F, 40.0F);
			const auto offset = vve::Vec3{std::cos(orbitPitch) * std::cos(orbitYaw), std::sin(orbitPitch), std::cos(orbitPitch) * std::sin(orbitYaw)};
			eye = vve::math::add(target, vve::math::scale(offset, orbitDistance));
			render.setCamera(vve::Camera::lookAt(vve::Position{.value = eye}, vve::Position{.value = target}, {}, {}, clip));
			if (input.wasKeyPressed(vve::Key::mouse_left)) { picked = pick(render, input.mousePosition(window).value_or(vve::Vec2{0.5F, 0.5F})); }
			return;
		}
		captureMouse(false);
		vve::Vec3 wanted{}, look{};
		switch (view) {
		case View::chase:
			wanted = racer::world(car.position - forward * 9.0F, 4.2F);
			look = racer::world(car.position + forward * 5.0F, 1.0F);
			break;
		case View::hood:
			wanted = racer::world(car.position + forward * 0.6F, 1.45F);
			look = racer::world(car.position + forward * 10.0F, 1.2F);
			break;
		case View::top:
			wanted = racer::world(car.position - forward * 0.1F, 45.0F);
			look = racer::world(car.position, 0.0F);
			break;
		}
		// The chase camera follows with a spring; the others are fixed to the car.
		const float follow = view == View::chase ? 1.0F - std::exp(-6.0F * dt) : 1.0F;
		eye = vve::math::add(eye, vve::math::scale(vve::math::subtract(wanted, eye), follow));
		render.setCamera(vve::Camera::lookAt(vve::Position{.value = eye}, vve::Position{.value = look}, {}, {}, clip));
	}

	/// @brief Mouse capture (hidden pointer, endless motion) follows the orbit button.
	void captureMouse(bool captured) {
		if (captured != mouseCaptured && windows->setMouseCaptured(window, captured)) { mouseCaptured = captured; }
	}

	/// @brief Names the car under a window point, using the camera ray against the cars' boxes.
	std::string pick(vve::RenderSystem &render, vve::Vec2 point) const {
		const auto ray = render.screenRay(window, point);
		const auto hit = ray ? render.raycast(ray->first, ray->second, 200.0F) : std::nullopt;
		if (!hit) { return "nothing"; }
		std::size_t index{};
		for (const auto &[entity, rig] : rigs) {
			if (std::ranges::contains(rig.parts, hit->object)) {
				const auto &progress = cars[index].progress;
				return std::format("{}: lap {}, best {}, {:.0f} km/h", carNames[index % carNames.size()], std::max(progress.completedLaps + 1, 1),
					lapText(progress.bestLap), cars[index].vehicle.forwardSpeed() * 3.6F);
			}
			++index;
		}
		return "scenery";
	}

	/// @brief Main menu, pause menu, race HUD, minimap and results.
	void drawHud(RaceSystem &race) {
		const auto *viewport = ImGui::GetMainViewport();
		const auto centre = ImVec2{viewport->Pos.x + viewport->Size.x * 0.5F, viewport->Pos.y + viewport->Size.y * 0.5F};
		constexpr auto fixed = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse;
		const auto menuWindow = [&](const char *title) {
			ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2{0.5F, 0.5F});
			return ImGui::Begin(title, nullptr, fixed);
		};
		if (race.phase == Phase::menu) {
			if (menuWindow("Ring Racer")) {
				if (const auto id = gui->imageId(logo); id != 0U) { ImGui::Image(static_cast<ImTextureID>(id), ImVec2{192, 64}); }
				if (ImGui::Button("Race", ImVec2{192, 0})) { race.start(Mode::race); }
				if (ImGui::Button("Time trial", ImVec2{192, 0})) { race.start(Mode::timeTrial); }
				ImGui::SliderInt("Laps", &race.laps, 1, 9);
				ImGui::SliderInt("Opponents", &race.opponents, 0, 3);
				drawOptions();
				if (ImGui::Button("Quit", ImVec2{192, 0})) { race.quit = true; }
			}
			ImGui::End();
			return;
		}
		if (race.paused && !photo) {
			if (menuWindow("Paused")) {
				if (ImGui::Button("Resume", ImVec2{160, 0})) { race.paused = false; }
				if (ImGui::Button("Restart", ImVec2{160, 0})) { race.start(race.mode); }
				if (ImGui::Button("Main menu", ImVec2{160, 0})) { race.phase = Phase::menu; race.paused = false; }
				drawOptions();
				if (ImGui::Button("Quit", ImVec2{160, 0})) { race.quit = true; }
			}
			ImGui::End();
		}
		if (cars.empty()) { return; }
		const auto order = race.standings(cars);
		const auto &player = cars.front();
		const int position = static_cast<int>(std::ranges::find(order, 0U) - order.begin()) + 1;
		ImGui::SetNextWindowPos(ImVec2{12, 12}, ImGuiCond_Always);
		if (ImGui::Begin("Race", nullptr, fixed | ImGuiWindowFlags_NoTitleBar)) {
			ImGui::Text("%.0f km/h", std::abs(player.vehicle.forwardSpeed()) * 3.6F);
			ImGui::Text("Lap %d / %d", std::clamp(player.progress.completedLaps + 1, 1, race.laps), race.laps);
			if (race.mode == Mode::race) { ImGui::Text("Position %d / %zu", position, cars.size()); }
			ImGui::Text("Lap  %s", lapText(race.phase == Phase::racing ? race.raceTime - player.progress.lapStart : 0.0F).c_str());
			ImGui::Text("Last %s", lapText(player.progress.lastLap).c_str());
			ImGui::Text("Best %s", lapText(player.progress.bestLap).c_str());
			if (photo) { ImGui::TextUnformatted("Photo mode: right mouse orbits, click a car"); ImGui::TextUnformatted(picked.c_str()); }
		}
		ImGui::End();
		drawMinimap(race, viewport);
		auto *draw = ImGui::GetForegroundDrawList();
		const auto banner = [&](const std::string &text, ImU32 color) {
			const float size = 64.0F;
			const auto extent = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0F, text.c_str());
			draw->AddText(ImGui::GetFont(), size, ImVec2{centre.x - extent.x * 0.5F, centre.y * 0.45F}, color, text.c_str());
		};
		if (race.phase == Phase::countdown) { banner(std::format("{}", static_cast<int>(std::ceil(race.countdown))), IM_COL32(255, 60, 40, 255)); }
		if (race.phase == Phase::racing && race.raceTime < 1.0F) { banner("GO!", IM_COL32(60, 255, 80, 255)); }
		if (player.progress.wrongWay()) { banner("WRONG WAY", IM_COL32(255, 200, 0, 255)); }
		if (race.phase == Phase::finished) {
			if (menuWindow("Results")) {
				for (std::size_t place{}; place < order.size(); ++place) {
					const auto &progress = cars[order[place]].progress;
					ImGui::Text("%zu. %-7s %s  best %s", place + 1, carNames[order[place] % carNames.size()],
						progress.finished ? lapText(progress.finishTime).c_str() : "racing", lapText(progress.bestLap).c_str());
				}
				if (ImGui::Button("Race again", ImVec2{160, 0})) { race.start(race.mode); }
				if (ImGui::Button("Main menu", ImVec2{160, 0})) { race.phase = Phase::menu; }
			}
			ImGui::End();
		}
	}

	/// @brief Night, music and fullscreen settings shared by both menus.
	void drawOptions() {
		ImGui::Checkbox("Night", &night);
		if (audio != nullptr) {
			if (ImGui::Combo("Music", &track, "Dance\0Ophelia\0Never get out\0")) { playMusic(); }
			if (ImGui::SliderFloat("Volume", &volume, 0.0F, 1.0F, "%.2f")) { (void)audio->setMasterVolume(vve::AudioVolume{.value = volume}); }
		}
	}

	/// @brief Track outline and car dots in the bottom-right corner.
	void drawMinimap(const RaceSystem &race, const ImGuiViewport *viewport) const {
		constexpr float size{180.0F}, scale{size / 240.0F};
		const ImVec2 origin{viewport->Pos.x + viewport->Size.x - size * 0.5F - 12.0F, viewport->Pos.y + viewport->Size.y - size * 0.5F - 12.0F};
		const auto map = [&](racer::Vec2f p) { return ImVec2{origin.x + p.x * scale, origin.y + p.y * scale}; };
		auto *draw = ImGui::GetBackgroundDrawList();
		draw->AddRectFilled(ImVec2{origin.x - size * 0.5F, origin.y - size * 0.5F}, ImVec2{origin.x + size * 0.5F, origin.y + size * 0.5F}, IM_COL32(0, 0, 0, 110), 8.0F);
		std::vector<ImVec2> outline{};
		for (const auto &sample : race.track.samples()) { outline.push_back(map(sample.position)); }
		draw->AddPolyline(outline.data(), static_cast<int>(outline.size()), IM_COL32(220, 220, 220, 255), ImDrawFlags_Closed, 3.0F);
		for (std::size_t i = cars.size(); i-- > 0U;) {
			const auto color = carColors[i % carColors.size()];
			draw->AddCircleFilled(map(cars[i].vehicle.position), i == 0U ? 6.0F : 4.5F,
				IM_COL32(static_cast<int>(color.x * 255), static_cast<int>(color.y * 255), static_cast<int>(color.z * 255), 255));
		}
	}
};

} // namespace racer
