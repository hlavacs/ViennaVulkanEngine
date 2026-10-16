/**
 * @file
 * @brief Ring Racer race rules on top of Racer.Logic: laps and checkpoints, the AI driver, the ghost, the grid and one race step.
 *
 * Functional objects:
 * - RaceProgress / advanceProgress: checkpoints, laps, lap times and wrong-way detection.
 * - AiDriver / driveAi: pure-pursuit steering and a curvature speed limit.
 * - GhostFrame / Ghost: a recorded lap that can be replayed by time.
 * - RaceCar, startGrid, stepRace: one car's full state, the starting grid and one fixed step of the whole race.
 */
export module Racer.Rules;
import std;
export import Racer.Logic;

export namespace racer {

	/// @brief Lap and checkpoint state of one car; laps count only after every checkpoint was passed in order.
	struct RaceProgress {
		static constexpr int checkpoints{Track::checkpoints};	///< Checkpoints per lap; number 0 is the start line.
		int completedLaps{-1};						///< -1 before the car first crosses the start line from the grid.
		int nextCheckpoint{0};						///< Checkpoint the car must pass next.
		float distance{};								///< Current distance along the track.
		float lapStart{};								///< Race time when the current lap began.
		float lastLap{};								///< Time of the previous lap, zero before the first.
		float bestLap{};								///< Best lap time, zero before the first.
		float wrongWayTime{};						///< Seconds the car has been driving against the track direction.
		bool finished{};								///< Completed all race laps.
		float finishTime{};							///< Race time at the finish.

		[[nodiscard]] float total(float trackLength) const { return static_cast<float>(completedLaps) * trackLength + distance; }
		[[nodiscard]] bool wrongWay() const { return wrongWayTime > 1.0F; }
	};

	/// @brief Result of one progress update.
	struct ProgressEvent {
		bool lapCompleted{};		///< A full lap was just completed.
		bool newBest{};			///< That lap is the best so far.
	};

	[[nodiscard]] ProgressEvent advanceProgress(RaceProgress &progress, const Vehicle &car, const Track &track, float raceTime,
		int raceLaps, float dt);

	/// @brief Personality of one computer driver.
	struct AiDriver {
		float line{};			///< Preferred lateral offset from the centre line.
		float skill{1.0F};	///< Fraction of the physical cornering speed the driver dares to use.
	};

	[[nodiscard]] Controls driveAi(const Vehicle &car, const AiDriver &driver, const Track &track, const VehicleParams &params);

	/// @brief One recorded pose of a ghost lap.
	struct GhostFrame {
		Vec2f position{};		///< Car centre.
		float heading{};		///< Driving direction.
		float steer{};			///< Front-wheel angle.
		float wheelAngle{};	///< Wheel rotation.
	};

	/// @brief A lap recorded at a fixed step and replayed by lap time.
	struct Ghost {
		float step{1.0F / 120.0F};			///< Time between frames.
		std::vector<GhostFrame> frames{};	///< Poses from the start of the lap.

		[[nodiscard]] bool empty() const { return frames.empty(); }
		[[nodiscard]] GhostFrame at(float lapTime) const;
	};

	/// @brief Everything the simulation keeps about one car.
	struct RaceCar {
		Vehicle vehicle{};			///< Physics state.
		RaceProgress progress{};	///< Laps and checkpoints.
		AiDriver ai{};					///< Driving style when the computer drives.
		bool player{};					///< Driven by the keyboard (unless on autopilot).
	};


	[[nodiscard]] std::vector<RaceCar> startGrid(const Track &track, int count, std::uint32_t seed);
	void stepRace(std::span<RaceCar> cars, std::span<const Controls> controls, const Track &track, const VehicleParams &params,
		float raceTime, int raceLaps, float dt);

} // namespace racer

namespace racer {

	/// @brief Moves the progress to the car's new distance, counting checkpoints, laps and wrong-way driving.
	ProgressEvent advanceProgress(RaceProgress &progress, const Vehicle &car, const Track &track, float raceTime, int raceLaps, float dt) {
		ProgressEvent event{};
		const auto location = track.locate(car.position);
		const float moved = track.delta(progress.distance, location.distance);
		// Each checkpoint k sits at k/6 of the lap; it counts only in order and only when driven over forwards.
		if (moved > 0.0F && !progress.finished) {
			const float spacing = track.length() / RaceProgress::checkpoints;
			const float checkpoint = static_cast<float>(progress.nextCheckpoint) * spacing;
			if (const float ahead = track.wrap(checkpoint - progress.distance); ahead > 0.0F && ahead <= moved) {
				if (progress.nextCheckpoint == 0) {
					++progress.completedLaps;
					if (progress.completedLaps > 0) {
						progress.lastLap = raceTime - progress.lapStart;
						event = {.lapCompleted = true, .newBest = progress.bestLap == 0.0F || progress.lastLap < progress.bestLap};
						if (event.newBest) { progress.bestLap = progress.lastLap; }
					}
					progress.lapStart = raceTime;
					if (progress.completedLaps >= raceLaps) { progress.finished = true; progress.finishTime = raceTime; }
				}
				progress.nextCheckpoint = (progress.nextCheckpoint + 1) % RaceProgress::checkpoints;
			}
		}
		progress.distance = location.distance;
		const bool against = dot(car.velocity, location.tangent) < -2.0F;
		progress.wrongWayTime = against ? progress.wrongWayTime + dt : 0.0F;
		return event;
	}

	/// @brief Steers at a look-ahead point on the driver's line and keeps a speed every bend ahead allows.
	Controls driveAi(const Vehicle &car, const AiDriver &driver, const Track &track, const VehicleParams &params) {
		const auto location = track.locate(car.position);
		const float speed = car.forwardSpeed();
		const float lookAhead = 6.0F + 0.45F * std::max(speed, 0.0F);
		const auto target = track.at(location.distance + lookAhead);
		const Vec2f aim = target.position + rightOf(target.tangent) * driver.line - car.position;
		const Vec2f forward = headingVector(car.heading);
		// Pure pursuit: the front-wheel angle that drives a circle through the aim point.
		const float alpha = std::atan2(cross(forward, aim), dot(forward, aim));
		const float wheel = std::atan(2.0F * params.wheelBase * std::sin(alpha) / std::max(length(aim), 1.0F));
		const float limit = params.maxSteer / (1.0F + std::abs(speed) / params.steerFalloff);
		// The slowest speed any bend ahead allows, given the braking distance to reach it.
		const float cornering = params.grip * 0.85F * driver.skill;
		float allowed = std::sqrt(params.engine / params.drag) * driver.skill;
		for (float ahead = 0.0F; ahead < 90.0F; ahead += 3.0F) {
			const float bend = std::max(std::abs(track.at(location.distance + ahead).curvature), 1.0e-4F);
			allowed = std::min(allowed, std::sqrt(cornering / bend + 2.0F * params.brake * 0.55F * ahead));
		}
		return {.throttle = speed < allowed - 0.5F ? 1.0F : 0.0F, .brake = speed > allowed + 1.5F ? 1.0F : 0.0F,
			.steer = std::clamp(wheel / limit, -1.0F, 1.0F)};
	}

	/// @brief Places cars in two staggered columns behind the start line, facing the driving direction.
	std::vector<RaceCar> startGrid(const Track &track, int count, std::uint32_t seed) {
		std::mt19937 random{seed};
		std::uniform_real_distribution<float> skill{0.9F, 1.0F}, line{-1.5F, 1.5F};
		std::vector<RaceCar> cars{};
		for (int index{}; index < count; ++index) {
			const auto slot = track.at(-8.0F - 7.0F * static_cast<float>(index));
			RaceCar car{.ai = {.line = line(random), .skill = skill(random)}, .player = index == 0};
			car.vehicle.position = slot.position + rightOf(slot.tangent) * (index % 2 == 0 ? -2.5F : 2.5F);
			car.vehicle.heading = std::atan2(slot.tangent.y, slot.tangent.x);
			car.progress.distance = track.locate(car.vehicle.position).distance;
			cars.push_back(car);
		}
		return cars;
	}

	/// @brief One fixed step for all cars: drive, push apart, then keep clear of posts and walls, then count progress.
	///
	/// Walls come last, so no push from another car can leave a car inside a wall.
	void stepRace(std::span<RaceCar> cars, std::span<const Controls> controls, const Track &track, const VehicleParams &params,
		float raceTime, int raceLaps, float dt) {
		for (std::size_t i{}; i < cars.size(); ++i) { stepVehicle(cars[i].vehicle, i < controls.size() ? controls[i] : Controls{}, params, dt); }
		for (std::size_t i{}; i < cars.size(); ++i) {
			for (std::size_t j{i + 1U}; j < cars.size(); ++j) { collideCars(cars[i].vehicle, cars[j].vehicle); }
		}
		for (auto &car : cars) {
			collideWithPosts(car.vehicle, track);
			collideWithWalls(car.vehicle, track);
		}
		for (auto &car : cars) { (void)advanceProgress(car.progress, car.vehicle, track, raceTime, raceLaps, dt); }
	}

	/// @brief Pose at a lap time; times past the end hold the last frame.
	GhostFrame Ghost::at(float lapTime) const {
		if (frames.empty()) { return {}; }
		const auto index = static_cast<std::size_t>(std::max(lapTime, 0.0F) / step);
		return frames[std::min(index, frames.size() - 1U)];
	}

} // namespace racer
