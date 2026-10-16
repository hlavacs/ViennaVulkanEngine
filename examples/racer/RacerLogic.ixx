/**
 * @file
 * @brief Ring Racer physics without rendering: track, arcade vehicle model and box colliders.
 *
 * Everything works in the ground plane: a point p = (x, y) here is the world point (x, 0, y). A heading h points the
 * car along (cos h, sin h); its right side is (-sin h, cos h), so a positive steering angle turns right. The world yaw
 * of the car model (whose +X is forward) is -h.
 *
 * Functional objects:
 * - Vec2f: small 2D vector with the few operators the physics needs.
 * - Track: closed Catmull-Rom centre line, resampled at even spacing, with nearest-point and distance lookups.
 * - VehicleParams, Controls, Vehicle: bicycle-model car and its fixed-step integration (stepVehicle).
 * - Box2, Contact, corners, overlap: oriented rectangles, the collider of every body, and their separating-axis test.
 * - collideWithWalls, collideWithPosts, collideCars: a car box against the walls, the gate posts and another car.
 * - ringCircuit: control points of the game's circuit.
 * The race rules built on this physics live in Racer.Rules (RacerRules.ixx).
 */
export module Racer.Logic;
import std;

export namespace racer {

	/// @brief Two-component vector in the ground plane.
	struct Vec2f {
		float x{};	///< World x.
		float y{};	///< World z.
	};
	[[nodiscard]] constexpr Vec2f operator+(Vec2f a, Vec2f b) { return {a.x + b.x, a.y + b.y}; }
	[[nodiscard]] constexpr Vec2f operator-(Vec2f a, Vec2f b) { return {a.x - b.x, a.y - b.y}; }
	[[nodiscard]] constexpr Vec2f operator*(Vec2f a, float s) { return {a.x * s, a.y * s}; }
	[[nodiscard]] constexpr float dot(Vec2f a, Vec2f b) { return a.x * b.x + a.y * b.y; }
	[[nodiscard]] constexpr float cross(Vec2f a, Vec2f b) { return a.x * b.y - a.y * b.x; }	///< Positive when b lies to the right of a.
	[[nodiscard]] inline float length(Vec2f a) { return std::sqrt(dot(a, a)); }
	[[nodiscard]] inline Vec2f normalize(Vec2f a) { const float l = length(a); return l > 0.0F ? a * (1.0F / l) : Vec2f{}; }
	[[nodiscard]] constexpr Vec2f rightOf(Vec2f direction) { return {-direction.y, direction.x}; }	///< Right-hand side of a direction.
	[[nodiscard]] inline Vec2f headingVector(float heading) { return {std::cos(heading), std::sin(heading)}; }

	/// @brief Oriented rectangle in the ground plane: the collider of cars and gate posts.
	struct Box2 {
		Vec2f centre{};			///< Centre point.
		Vec2f axis{1.0F, 0.0F};	///< Unit direction of the long side.
		float halfLength{};		///< Half extent along axis.
		float halfWidth{};		///< Half extent across axis.
	};

	/// @brief How far two boxes overlap: push the second one along normal by depth to separate them.
	struct Contact {
		Vec2f normal{};			///< Unit direction from the first box towards the second.
		float depth{};				///< Overlap along normal.
	};

	[[nodiscard]] std::array<Vec2f, 4> corners(const Box2 &box);
	[[nodiscard]] std::optional<Contact> overlap(const Box2 &a, const Box2 &b);

	/// @brief One evenly spaced point of the track centre line.
	struct TrackSample {
		Vec2f position{};		///< Centre-line point.
		Vec2f tangent{};		///< Unit driving direction.
		float distance{};		///< Distance from the start line.
		float curvature{};	///< Signed curvature in 1/m; positive bends right.
	};

	/// @brief Where a point lies relative to the track.
	struct TrackLocation {
		float distance{};		///< Distance of the nearest centre-line point from the start line.
		float lateral{};		///< Signed offset from the centre line; positive is right of the driving direction.
		Vec2f centre{};		///< Nearest centre-line point.
		Vec2f tangent{};		///< Driving direction there.
	};

	/// @brief Closed track centre line with its width; distances run from 0 at the start line to length().
	class Track {
	public:
		static constexpr float halfWidth{6.0F};		///< Half the road width.
		static constexpr float kerbWidth{1.2F};		///< Kerb strip beside each road edge.
		static constexpr float wallOffset{10.5F};	///< Distance of the barrier walls from the centre line.
		static constexpr float postOffset{halfWidth + kerbWidth + 0.8F};	///< Distance of the gate posts from the centre line.
		static constexpr float postHalf{0.25F};	///< Half side of a square gate post.
		static constexpr int checkpoints{6};		///< Checkpoints per lap, each with a gate; number 0 is the start line.

		[[nodiscard]] static Track build(std::span<const Vec2f> control, float spacing = 2.0F);
		[[nodiscard]] float length() const { return length_; }
		[[nodiscard]] const std::vector<TrackSample> &samples() const { return samples_; }
		[[nodiscard]] TrackLocation locate(Vec2f point) const;
		[[nodiscard]] TrackSample at(float distance) const;
		[[nodiscard]] float wrap(float distance) const;
		[[nodiscard]] float delta(float from, float to) const;
		[[nodiscard]] const std::vector<Box2> &posts() const { return posts_; }

	private:
		std::vector<TrackSample> samples_{};	///< Even samples; the last one is followed by the first.
		std::vector<Box2> posts_{};				///< Two gate posts at every checkpoint, left and right of the road.
		float length_{};							///< Closed length in metres.
		float spacing_{};							///< Distance between samples.
	};

	/// @brief Tunable numbers of the arcade car; the defaults give about 180 km/h top speed.
	struct VehicleParams {
		float wheelBase{2.6F};			///< Axle distance of the car model.
		float wheelRadius{0.4F};		///< Tyre radius for the wheel spin.
		float maxSteer{0.55F};			///< Steering angle limit at standstill (radians).
		float steerFalloff{18.0F};		///< Speed at which the steering limit has halved.
		float steerRate{2.5F};			///< Steering speed (radians per second).
		float engine{9.0F};				///< Full-throttle acceleration (m/s^2).
		float brake{14.0F};				///< Full-brake deceleration (m/s^2).
		float reverse{4.0F};				///< Reverse acceleration (m/s^2).
		float maxReverse{8.0F};			///< Reverse speed limit (m/s).
		float rolling{0.3F};				///< Rolling resistance (m/s^2).
		float drag{0.0035F};				///< Air drag factor (1/m): deceleration = drag * speed^2.
		float grip{9.0F};					///< Largest sideways deceleration the tyres give (m/s^2).
		float handbrakeGrip{0.3F};		///< Grip factor while the handbrake is pulled; the car slides.
		float grassGrip{0.6F};			///< Grip factor on grass.
		float grassRolling{8.0F};		///< Rolling factor on grass.
		float grassDrag{4.0F};			///< Drag factor on grass.
	};

	/// @brief Driver inputs for one step; analogue values lie in [0, 1], steer in [-1, 1] (positive turns right).
	struct Controls {
		float throttle{};		///< Accelerator.
		float brake{};			///< Brake, or reverse once stopped.
		float steer{};			///< Steering wheel.
		bool handbrake{};		///< Locks the rear wheels.
	};

	/// @brief State of one car.
	struct Vehicle {
		Vec2f position{};		///< Centre in the ground plane.
		Vec2f velocity{};		///< Velocity in the ground plane.
		float heading{};		///< Driving direction angle.
		float steer{};			///< Current front-wheel angle.
		float wheelAngle{};	///< Accumulated wheel rotation for the visuals.
		float slip{};			///< Sideways speed after the tyres acted; large while drifting.
		float impact{};		///< Speed into a wall or car in the last step, zero without contact.
		bool offTrack{};		///< Beyond the kerbs, on the grass.
		bool braking{};		///< Brake pressed while moving forward (brake lights).
		float halfLength{2.1F};	///< Collider half length, from the car model.
		float halfWidth{0.95F};	///< Collider half width, from the car model.

		[[nodiscard]] float forwardSpeed() const { return dot(velocity, headingVector(heading)); }
		[[nodiscard]] Box2 box() const { return {.centre = position, .axis = headingVector(heading), .halfLength = halfLength, .halfWidth = halfWidth}; }
	};

	void stepVehicle(Vehicle &car, const Controls &controls, const VehicleParams &params, float dt);
	void collideWithWalls(Vehicle &car, const Track &track);
	void collideWithPosts(Vehicle &car, const Track &track);
	void collideCars(Vehicle &a, Vehicle &b);
	[[nodiscard]] float wallClearance(const Vehicle &car, const Track &track);

	[[nodiscard]] std::vector<Vec2f> ringCircuit();

} // namespace racer

namespace racer {

	/// @brief Point on a uniform Catmull-Rom segment between p1 and p2.
	[[nodiscard]] Vec2f catmullRom(Vec2f p0, Vec2f p1, Vec2f p2, Vec2f p3, float t) {
		const float t2 = t * t, t3 = t2 * t;
		return (p1 * 2.0F + (p2 - p0) * t + (p0 * 2.0F - p1 * 5.0F + p2 * 4.0F - p3) * t2 + (p1 * 3.0F - p0 - p2 * 3.0F + p3) * t3) * 0.5F;
	}

	/// @brief Builds the closed curve through the control points and resamples it every `spacing` metres.
	Track Track::build(std::span<const Vec2f> control, float spacing) {
		Track track{};
		const std::size_t count = control.size();
		if (count < 4U || !(spacing > 0.0F)) { return track; }
		// A dense polyline first; its arc length then places the even samples.
		constexpr int steps{64};
		std::vector<Vec2f> dense{};
		for (std::size_t segment{}; segment < count; ++segment) {
			const auto point = [&](std::size_t offset) { return control[(segment + count + offset - 1U) % count]; };
			for (int step{}; step < steps; ++step) {
				dense.push_back(catmullRom(point(0), point(1), point(2), point(3), static_cast<float>(step) / steps));
			}
		}
		std::vector<float> arc(dense.size() + 1U, 0.0F);
		for (std::size_t i{}; i < dense.size(); ++i) { arc[i + 1U] = arc[i] + racer::length(dense[(i + 1U) % dense.size()] - dense[i]); }
		track.length_ = arc.back();
		const auto sampleCount = std::max<std::size_t>(8U, static_cast<std::size_t>(std::lround(track.length_ / spacing)));
		track.spacing_ = track.length_ / static_cast<float>(sampleCount);
		std::size_t cursor{};
		for (std::size_t k{}; k < sampleCount; ++k) {
			const float s = static_cast<float>(k) * track.spacing_;
			while (arc[cursor + 1U] < s) { ++cursor; }
			const float t = (s - arc[cursor]) / std::max(arc[cursor + 1U] - arc[cursor], 1.0e-6F);
			const Vec2f a = dense[cursor], b = dense[(cursor + 1U) % dense.size()];
			track.samples_.push_back({.position = a + (b - a) * t, .distance = s});
		}
		// Tangents from the neighbours, curvature from the turn between consecutive tangents.
		for (std::size_t k{}; k < sampleCount; ++k) {
			const auto &previous = track.samples_[(k + sampleCount - 1U) % sampleCount];
			const auto &next = track.samples_[(k + 1U) % sampleCount];
			track.samples_[k].tangent = normalize(next.position - previous.position);
		}
		for (std::size_t k{}; k < sampleCount; ++k) {
			const auto &next = track.samples_[(k + 1U) % sampleCount];
			track.samples_[k].curvature = cross(track.samples_[k].tangent, next.tangent) / track.spacing_;
		}
		// Every checkpoint gets a gate: two square posts beside the kerbs, turned with the road.
		for (int checkpoint{}; checkpoint < checkpoints; ++checkpoint) {
			const auto sample = track.at(track.length_ * static_cast<float>(checkpoint) / checkpoints);
			for (const float side : {-1.0F, 1.0F}) {
				track.posts_.push_back({.centre = sample.position + rightOf(sample.tangent) * (side * postOffset), .axis = sample.tangent,
					.halfLength = postHalf, .halfWidth = postHalf});
			}
		}
		return track;
	}

	/// @brief Wraps a distance into [0, length).
	float Track::wrap(float distance) const {
		const float wrapped = std::fmod(distance, length_);
		return wrapped < 0.0F ? wrapped + length_ : wrapped;
	}

	/// @brief Shortest signed distance along the track from one position to another.
	float Track::delta(float from, float to) const {
		float difference = wrap(to - from);
		return difference > length_ * 0.5F ? difference - length_ : difference;
	}

	/// @brief Centre-line point and direction at a distance, interpolated between samples.
	TrackSample Track::at(float distance) const {
		const float s = wrap(distance);
		const auto index = std::min(static_cast<std::size_t>(s / spacing_), samples_.size() - 1U);
		const auto &a = samples_[index];
		const auto &b = samples_[(index + 1U) % samples_.size()];
		const float t = (s - a.distance) / spacing_;
		return {.position = a.position + (b.position - a.position) * t, .tangent = normalize(a.tangent + (b.tangent - a.tangent) * t),
			.distance = s, .curvature = a.curvature + (b.curvature - a.curvature) * t};
	}

	/// @brief Projects a point onto the nearest centre-line segment.
	TrackLocation Track::locate(Vec2f point) const {
		std::size_t nearest{};
		float best = std::numeric_limits<float>::max();
		for (std::size_t k{}; k < samples_.size(); ++k) {
			const Vec2f offset = point - samples_[k].position;
			if (const float d = dot(offset, offset); d < best) { best = d; nearest = k; }
		}
		// The point lies on the segment before or after the nearest sample; take the closer projection.
		TrackLocation result{};
		best = std::numeric_limits<float>::max();
		for (const std::size_t first : {(nearest + samples_.size() - 1U) % samples_.size(), nearest}) {
			const auto &a = samples_[first];
			const auto &b = samples_[(first + 1U) % samples_.size()];
			const Vec2f along = b.position - a.position;
			const float t = std::clamp(dot(point - a.position, along) / std::max(dot(along, along), 1.0e-6F), 0.0F, 1.0F);
			const Vec2f centre = a.position + along * t;
			const Vec2f offset = point - centre;
			if (const float d = dot(offset, offset); d < best) {
				best = d;
				const Vec2f tangent = normalize(along);
				result = {.distance = wrap(a.distance + t * spacing_), .lateral = cross(tangent, offset), .centre = centre, .tangent = tangent};
			}
		}
		return result;
	}

	/// @brief Integrates one fixed step: steering, engine and brakes, kinematic turning, then limited tyre grip.
	void stepVehicle(Vehicle &car, const Controls &controls, const VehicleParams &params, float dt) {
		const float grass = car.offTrack ? 1.0F : 0.0F;
		const Vec2f forward = headingVector(car.heading);
		const Vec2f right = rightOf(forward);
		float speed = dot(car.velocity, forward);
		const float sideways = dot(car.velocity, right);
		// Steering: rate-limited towards a target that shrinks with speed.
		const float limit = params.maxSteer / (1.0F + std::abs(speed) / params.steerFalloff);
		const float target = std::clamp(controls.steer, -1.0F, 1.0F) * limit;
		car.steer += std::clamp(target - car.steer, -params.steerRate * dt, params.steerRate * dt);
		// Engine and brakes; the brake pedal reverses once the car has stopped.
		float push{};
		const float throttle = std::clamp(controls.throttle, 0.0F, 1.0F), brake = std::clamp(controls.brake, 0.0F, 1.0F);
		push += speed < -0.5F ? throttle * params.brake : throttle * params.engine;
		if (speed > 0.5F) { push -= brake * params.brake; }
		else if (speed > -params.maxReverse) { push -= brake * params.reverse; }
		car.braking = brake > 0.0F && speed > 0.5F;
		if (controls.handbrake) { push -= (speed > 0.0F ? 1.0F : -1.0F) * 6.0F; }
		speed += push * dt;
		// Resistance never reverses the motion on its own.
		const float rolling = params.rolling * (1.0F + grass * (params.grassRolling - 1.0F));
		const float drag = params.drag * (1.0F + grass * (params.grassDrag - 1.0F));
		const float resistance = (rolling + drag * speed * speed) * dt;
		speed = std::abs(speed) <= resistance ? 0.0F : speed - std::copysign(resistance, speed);
		// Turning follows the front wheels; the old sideways speed remains and the tyres remove what their grip allows.
		car.heading += speed / params.wheelBase * std::tan(car.steer) * dt;
		const Vec2f velocity = forward * speed + right * sideways;
		const Vec2f newForward = headingVector(car.heading);
		const Vec2f newRight = rightOf(newForward);
		const float grip = params.grip * (controls.handbrake ? params.handbrakeGrip : 1.0F) * (1.0F + grass * (params.grassGrip - 1.0F));
		float slide = dot(velocity, newRight);
		slide -= std::clamp(slide, -grip * dt, grip * dt);
		const float newSpeed = dot(velocity, newForward);
		car.velocity = newForward * newSpeed + newRight * slide;
		car.position = car.position + car.velocity * dt;
		car.slip = std::abs(slide);
		car.wheelAngle += newSpeed * dt / params.wheelRadius;
		car.impact = 0.0F;
	}

	/// @brief The four corners of a box, going round.
	std::array<Vec2f, 4> corners(const Box2 &box) {
		const Vec2f along = box.axis * box.halfLength, across = rightOf(box.axis) * box.halfWidth;
		return {box.centre + along + across, box.centre + along - across, box.centre - along - across, box.centre - along + across};
	}

	/// @brief Separating-axis test of two oriented rectangles; returns the axis of least overlap, or nothing when apart.
	std::optional<Contact> overlap(const Box2 &a, const Box2 &b) {
		const auto extent = [](const Box2 &box, Vec2f axis) {
			return std::abs(dot(box.axis, axis)) * box.halfLength + std::abs(dot(rightOf(box.axis), axis)) * box.halfWidth;
		};
		Contact best{.depth = std::numeric_limits<float>::max()};
		const Vec2f between = b.centre - a.centre;
		for (const Vec2f axis : {a.axis, rightOf(a.axis), b.axis, rightOf(b.axis)}) {
			const float distance = dot(between, axis);
			const float depth = extent(a, axis) + extent(b, axis) - std::abs(distance);
			if (depth <= 0.0F) { return std::nullopt; }
			if (depth < best.depth) { best = {.normal = axis * (distance < 0.0F ? -1.0F : 1.0F), .depth = depth}; }
		}
		return best;
	}

	/// @brief Bounces a car off a fixed surface whose normal points away from the car; returns the speed it had into it.
	float bounce(Vehicle &car, Vec2f normal) {
		const float into = dot(car.velocity, normal);
		if (into <= 0.0F) { return 0.0F; }
		car.velocity = (car.velocity - normal * (into * 1.3F)) * 0.9F;	// Restitution 0.3, scraping friction.
		car.impact = std::max(car.impact, into);
		return into;
	}

	/// @brief Smallest distance from any corner of the car's box to a barrier wall; negative when a corner is inside.
	float wallClearance(const Vehicle &car, const Track &track) {
		float clearance = std::numeric_limits<float>::max();
		for (const Vec2f corner : corners(car.box())) {
			clearance = std::min(clearance, Track::wallOffset - std::abs(track.locate(corner).lateral));
		}
		return clearance;
	}

	/// @brief Keeps every corner of the car's box inside the barrier walls; the deepest corner decides the push.
	void collideWithWalls(Vehicle &car, const Track &track) {
		car.offTrack = std::abs(track.locate(car.position).lateral) > Track::halfWidth + Track::kerbWidth;
		float deepest{};
		Vec2f outwards{};
		for (const Vec2f corner : corners(car.box())) {
			const auto location = track.locate(corner);
			if (const float depth = std::abs(location.lateral) - Track::wallOffset; depth > deepest) {
				deepest = depth;
				outwards = rightOf(location.tangent) * (location.lateral > 0.0F ? 1.0F : -1.0F);
			}
		}
		if (deepest <= 0.0F) { return; }
		car.position = car.position - outwards * deepest;
		(void)bounce(car, outwards);
	}

	/// @brief Pushes the car out of every gate post it touches; posts do not move.
	void collideWithPosts(Vehicle &car, const Track &track) {
		for (const Box2 &post : track.posts()) {
			if (const auto contact = overlap(car.box(), post)) {
				car.position = car.position - contact->normal * contact->depth;
				(void)bounce(car, contact->normal);
			}
		}
	}

	/// @brief Pushes two overlapping car boxes apart and exchanges their closing speed like equal masses.
	void collideCars(Vehicle &a, Vehicle &b) {
		const auto contact = overlap(a.box(), b.box());
		if (!contact) { return; }
		a.position = a.position - contact->normal * (contact->depth * 0.5F);
		b.position = b.position + contact->normal * (contact->depth * 0.5F);
		if (const float closing = dot(a.velocity - b.velocity, contact->normal); closing > 0.0F) {
			const float exchange = closing * 0.65F;	// Restitution 0.3 for equal masses.
			a.velocity = a.velocity - contact->normal * exchange;
			b.velocity = b.velocity + contact->normal * exchange;
			a.impact = std::max(a.impact, closing);
			b.impact = std::max(b.impact, closing);
		}
	}

	/// @brief Control points of the game's circuit, about 650 m long and inside a 110 m radius.
	std::vector<Vec2f> ringCircuit() {
		return {{0, -80}, {55, -84}, {98, -58}, {104, -5}, {80, 38}, {42, 40}, {22, 76}, {-28, 94}, {-80, 72}, {-102, 22},
			{-72, -18}, {-62, -62}};
	}

} // namespace racer
