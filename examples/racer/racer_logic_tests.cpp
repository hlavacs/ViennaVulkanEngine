/**
 * @file
 * @brief CPU tests for Racer.Logic: track geometry, vehicle physics against closed-form values, collisions, laps, AI and ghost.
 *
 * Every check prints one line; the program returns the number of the first failing check, or 0.
 */
import std;
import Racer.Rules;

namespace {

/// @brief Prints one check and reports whether it passed.
bool check(std::string_view name, bool passed, std::string_view detail) {
	std::println("[RacerLogicTests] {} {} {}", passed ? "PASS" : "FAIL", name, detail);
	return passed;
}

/// @brief Relative closeness of two values.
bool near(float value, float expected, float tolerance) { return std::abs(value - expected) <= tolerance * std::abs(expected); }

/// @brief A track through 16 points of a circle of radius 50, driven anticlockwise in x/y (that is, bending right).
racer::Track circleTrack() {
	std::vector<racer::Vec2f> points{};
	for (int i{}; i < 16; ++i) {
		const float angle = 2.0F * std::numbers::pi_v<float> * static_cast<float>(i) / 16.0F;
		points.push_back({50.0F * std::cos(angle), 50.0F * std::sin(angle)});
	}
	return racer::Track::build(points);
}

/// @brief Counts of colliders found overlapping after a step, over a whole race.
struct Violations {
	int walls{};	///< Steps with a car corner inside a wall.
	int cars{};		///< Steps with two car boxes overlapping by more than 5 cm.
	int posts{};	///< Steps with a car box inside a gate post.
	bool operator==(const Violations &) const = default;
};

/// @brief Runs a whole race of computer drivers and returns the finish order and times.
std::vector<std::pair<int, float>> headlessRace(const racer::Track &track, Violations &violations) {
	const racer::VehicleParams params{};
	auto cars = racer::startGrid(track, 4, 7U);
	std::vector<racer::Controls> controls(cars.size());
	constexpr float dt = 1.0F / 120.0F;
	float time{};
	violations = {};
	while (time < 300.0F && !std::ranges::all_of(cars, [](const auto &car) { return car.progress.finished; })) {
		for (std::size_t i{}; i < cars.size(); ++i) { controls[i] = racer::driveAi(cars[i].vehicle, cars[i].ai, track, params); }
		racer::stepRace(cars, controls, track, params, time, 3, dt);
		time += dt;
		for (std::size_t i{}; i < cars.size(); ++i) {
			violations.walls += racer::wallClearance(cars[i].vehicle, track) < -0.01F ? 1 : 0;
			for (const auto &post : track.posts()) {
				const auto contact = racer::overlap(cars[i].vehicle.box(), post);
				violations.posts += contact && contact->depth > 0.01F ? 1 : 0;
			}
			for (std::size_t j{i + 1U}; j < cars.size(); ++j) {
				const auto contact = racer::overlap(cars[i].vehicle.box(), cars[j].vehicle.box());
				violations.cars += contact && contact->depth > 0.05F ? 1 : 0;
			}
		}
	}
	std::vector<std::pair<int, float>> order{};
	for (std::size_t i{}; i < cars.size(); ++i) {
		order.emplace_back(static_cast<int>(i), cars[i].progress.finished ? cars[i].progress.finishTime : 1.0e9F);
	}
	std::ranges::sort(order, {}, &std::pair<int, float>::second);
	return order;
}

} // namespace

/// @brief Runs every check in order.
int main() {
	using namespace racer;
	const VehicleParams params{};
	constexpr float dt = 1.0F / 120.0F;

	// Track: the circle's length and curvature, and the lateral sign of a point outside it.
	const auto circle = circleTrack();
	const float circumference = 2.0F * std::numbers::pi_v<float> * 50.0F;
	const auto outside = circle.locate({60.0F, 0.0F});
	if (!check("circle_length", near(circle.length(), circumference, 0.01F), std::format("length={:.2f} expected={:.2f}", circle.length(), circumference))) { return 1; }
	// The curvature of a closed loop adds up to one full turn; positive means it bends right everywhere.
	float turning{};
	for (const auto &sample : circle.samples()) { turning += sample.curvature * circle.length() / static_cast<float>(circle.samples().size()); }
	const bool rightBend = std::ranges::all_of(circle.samples(), [](const auto &sample) { return sample.curvature > 0.0F; });
	if (!check("circle_turning", near(turning, 2.0F * std::numbers::pi_v<float>, 0.01F) && rightBend, std::format("turning={:.4f}", turning))) { return 2; }
	if (!check("locate_outside", near(outside.lateral, -10.0F, 0.03F), std::format("lateral={:.3f}", outside.lateral))) { return 3; }
	const auto ring = Track::build(ringCircuit());
	const auto start = ring.at(0.0F);
	const bool closed = length(ring.samples().back().position - start.position) < 3.0F;
	if (!check("ring_closed", closed && ring.length() > 500.0F && ring.length() < 800.0F, std::format("length={:.1f}", ring.length()))) { return 4; }

	// Top speed: engine = rolling + drag * v^2.
	Vehicle car{};
	for (int i{}; i < 120 * 90; ++i) { stepVehicle(car, {.throttle = 1.0F}, params, dt); }
	const float top = std::sqrt((params.engine - params.rolling) / params.drag);
	if (!check("top_speed", near(car.forwardSpeed(), top, 0.01F), std::format("speed={:.2f} expected={:.2f}", car.forwardSpeed(), top))) { return 5; }

	// Braking from top speed to 0.5 m/s: s = ln((b + c v0^2) / (b + c ve^2)) / 2c with b = brake + rolling.
	const auto from = car.position;
	const float v0 = car.forwardSpeed();
	while (car.forwardSpeed() > 0.5F) { stepVehicle(car, {.brake = 1.0F}, params, dt); }
	const float b = params.brake + params.rolling, c = params.drag;
	const float braking = std::log((b + c * v0 * v0) / (b + c * 0.25F)) / (2.0F * c);
	if (!check("braking_distance", near(length(car.position - from), braking, 0.02F),
		std::format("distance={:.2f} expected={:.2f}", length(car.position - from), braking))) { return 6; }

	// Turning at a fixed wheel angle and a low speed follows a circle of radius wheelBase / tan(steer).
	Vehicle turner{.velocity = {8.0F, 0.0F}};
	for (int i{}; i < 240; ++i) { stepVehicle(turner, {.throttle = 0.04F, .steer = 0.5F}, params, dt); }
	const float headingBefore = turner.heading;
	float travelled{};
	for (int i{}; i < 240; ++i) {
		const auto before = turner.position;
		stepVehicle(turner, {.throttle = 0.04F, .steer = 0.5F}, params, dt);
		travelled += length(turner.position - before);
	}
	const float radius = travelled / (turner.heading - headingBefore);
	const float expectedRadius = params.wheelBase / std::tan(turner.steer);
	if (!check("turn_radius", near(radius, expectedRadius, 0.02F) && turner.slip < 0.05F,
		std::format("radius={:.2f} expected={:.2f} slip={:.3f}", radius, expectedRadius, turner.slip))) { return 7; }

	// A wall stops a car whose nose is in it and reflects part of its speed; every corner ends up outside.
	Vehicle crasher{.position = {59.0F, 0.0F}, .velocity = {20.0F, 0.0F}};
	collideWithWalls(crasher, circle);
	const float clearance = wallClearance(crasher, circle);
	if (!check("wall_pushback", clearance >= -1.0e-3F && crasher.velocity.x < 0.0F && crasher.impact > 19.0F,
		std::format("clearance={:.3f} vx={:.2f} impact={:.2f}", clearance, crasher.velocity.x, crasher.impact))) { return 8; }

	// Box overlap: rotated boxes that touch only at a corner, and boxes that are apart.
	const Box2 square{.halfLength = 1.0F, .halfWidth = 1.0F};
	const Box2 diamond{.centre = {2.3F, 0.0F}, .axis = normalize({1.0F, 1.0F}), .halfLength = 1.0F, .halfWidth = 1.0F};
	const auto corner = overlap(square, diamond);
	const bool apart = !overlap(square, Box2{.centre = {2.5F, 0.0F}, .axis = normalize({1.0F, 1.0F}), .halfLength = 1.0F, .halfWidth = 1.0F});
	if (!check("box_overlap", corner && near(corner->depth, 1.0F + std::sqrt(2.0F) - 2.3F, 1.0e-3F) && corner->normal.x > 0.99F && apart,
		std::format("depth={:.4f} apart={}", corner ? corner->depth : -1.0F, apart))) { return 9; }

	// A car running into the back of another: both end up just touching, the closing speed shared out.
	Vehicle chaser{.position = {0.0F, 0.0F}, .velocity = {5.0F, 0.0F}}, leader{.position = {3.0F, 0.0F}};
	collideCars(chaser, leader);
	const auto rest = overlap(chaser.box(), leader.box());
	if (!check("car_contact", (!rest || rest->depth < 1.0e-4F) && near(leader.position.x - chaser.position.x, 2.0F * chaser.halfLength, 1.0e-4F) &&
		leader.velocity.x > 0.0F && chaser.velocity.x < 5.0F,
		std::format("gap={:.3f} chaser_vx={:.2f} leader_vx={:.2f}", leader.position.x - chaser.position.x, chaser.velocity.x, leader.velocity.x))) { return 14; }

	// A car placed on a gate post is pushed out of it.
	const auto &post = circle.posts().front();
	Vehicle onPost{.position = post.centre + post.axis * 1.0F, .velocity = post.axis * -10.0F};
	collideWithPosts(onPost, circle);
	if (!check("post_pushback", !overlap(onPost.box(), post) && onPost.impact > 0.0F && circle.posts().size() == 2U * RaceProgress::checkpoints,
		std::format("posts={} impact={:.2f}", circle.posts().size(), onPost.impact))) { return 15; }

	// Laps: a car driven along the centre line passes all checkpoints; skipping one does not count the lap.
	RaceProgress lap{.distance = circle.wrap(-5.0F)};
	int laps{};
	float time{};
	for (float s = -5.0F; s < circle.length() * 2.0F + 5.0F; s += 1.0F, time += 0.05F) {
		const Vehicle onLine{.position = circle.at(s).position, .velocity = circle.at(s).tangent * 20.0F};
		laps += advanceProgress(lap, onLine, circle, time, 3, 0.05F).lapCompleted ? 1 : 0;
	}
	RaceProgress cutter{.distance = circle.wrap(-5.0F)};
	for (const float s : {-2.0F, 2.0F, 150.0F, 300.0F, circle.length() - 2.0F, circle.length() + 2.0F}) {
		(void)advanceProgress(cutter, {.position = circle.at(s).position}, circle, 0.0F, 3, 0.05F);
	}
	if (!check("laps", laps == 2 && lap.completedLaps == 2 && lap.bestLap > 0.0F && cutter.completedLaps == 0,
		std::format("laps={} completed={} best={:.2f} cutter={}", laps, lap.completedLaps, lap.bestLap, cutter.completedLaps))) { return 10; }

	// Wrong way: driving against the track for more than a second raises the warning.
	RaceProgress backwards{.distance = 100.0F};
	for (int i{}; i < 150; ++i) {
		(void)advanceProgress(backwards, {.position = circle.at(100.0F).position, .velocity = circle.at(100.0F).tangent * -10.0F}, circle, 0.0F, 3, dt);
	}
	if (!check("wrong_way", backwards.wrongWay(), std::format("seconds={:.2f}", backwards.wrongWayTime))) { return 11; }

	// Ghost replay returns the frame for a lap time and holds the last one.
	const Ghost ghost{.step = 0.5F, .frames = {{.position = {0, 0}}, {.position = {1, 0}}, {.position = {2, 0}}}};
	if (!check("ghost", ghost.at(0.6F).position.x == 1.0F && ghost.at(10.0F).position.x == 2.0F, "frames=3")) { return 12; }

	// A full race of four computer drivers on the game circuit: all finish, no body inside another, same result twice.
	Violations violations{};
	const auto first = headlessRace(ring, violations);
	Violations violationsAgain{};
	const auto second = headlessRace(ring, violationsAgain);
	const bool finished = std::ranges::all_of(first, [](const auto &entry) { return entry.second < 1.0e8F; });
	std::string order{};
	for (const auto &[car_index, finish] : first) { order += std::format("{}:{:.2f} ", car_index, finish); }
	if (!check("ai_race", finished && violations == Violations{} && violationsAgain == violations && first == second,
		std::format("order={}violations walls={} cars={} posts={}", order, violations.walls, violations.cars, violations.posts))) { return 13; }
	return 0;
}
