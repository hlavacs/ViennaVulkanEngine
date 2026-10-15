import std;
import VVEngine;
import VVE.ExampleSupport;

/**
 * @file
 * @brief A car built from parts drives in a circle; the engine moves every part with its parent node.
 *
 * The car asset (assets/hierarchy_car/car.gltf) is a node tree:
 *   Car (no mesh) -> Body, Cabin, and four wheel mounts (no mesh) -> one wheel each (tyre and rim meshes).
 * The scene is instantiated with SceneInstantiationOptions::keep_hierarchy. Each frame the example only sets
 * local transforms: the Car root drives, the front mounts steer, and the wheels spin about their axles.
 * The engine composes parent and local transforms, so the body, cabin and wheels follow without extra code.
 *
 * Options: --frames N (stop after N frames and capture), --output <png>, --verify <txt>.
 * Each frame checks the front-left wheel's world position and rotation (both of its meshes) against an independent
 * calculation; any mismatch makes the program fail. The verification file lists the checked values for automatic review.
 */
namespace {

constexpr float circleRadius = 6.0F;		///< Radius of the driven circle in metres.
constexpr float speed = 3.0F;				///< Driving speed in metres per second.
constexpr float wheelRadius = 0.4F;			///< Tyre radius; sets the spin rate for rolling without slipping.
constexpr float wheelBase = 2.6F;			///< Distance between front and rear axles.
constexpr float frameTime = 1.0F / 60.0F;	///< Fixed simulation step, so runs are repeatable.
constexpr float tolerance = 1.0e-3F;		///< Allowed difference between engine and independent positions.

/// @brief Rotation by an angle about +Y; the Quat constructor order is w, x, y, z.
[[nodiscard]] vve::Quat aboutY(float angle) { return vve::Quat{std::cos(angle / 2.0F), 0.0F, std::sin(angle / 2.0F), 0.0F}; }

/// @brief Rotation by an angle about +Z, the wheel axle.
[[nodiscard]] vve::Quat aboutZ(float angle) { return vve::Quat{std::cos(angle / 2.0F), 0.0F, 0.0F, std::sin(angle / 2.0F)}; }

/// @brief Builds a local transform with unit scale.
[[nodiscard]] vve::Transform local(vve::Vec3 translation, vve::Quat rotation) {
	return vve::Transform{.translation = {.value = translation}, .rotation = {.value = rotation}};
}

/// @brief Reads the value after an option such as --output, or returns a fallback.
[[nodiscard]] std::filesystem::path option(int argc, char **argv, std::string_view name, std::filesystem::path fallback) {
	for (int index = 1; index + 1 < argc; ++index) {
		if (std::string_view{argv[index]} == name) { return argv[index + 1]; }
	}
	return fallback;
}

} // namespace

/// @brief Loads the car, drives it with local node transforms only, and verifies the composed wheel position.
int main(int argc, char **argv) {
	std::cout << std::unitbuf;
	const auto root = vve::example::assetRoot(argc > 0 ? argv[0] : nullptr);
	const int max_frames = vve::example::frameLimit(argc, argv);
	const auto verify_dir = std::filesystem::path{argc > 0 ? argv[0] : "."}.parent_path().parent_path() / "verify";
	const auto capture_path = option(argc, argv, "--output", verify_dir / "hierarchy_car.png");
	const auto verify_path = option(argc, argv, "--verify", verify_dir / "hierarchy_car.txt");

	auto engine = vve::EngineBuilder<>{}
						  .applicationName("hierarchy_car")
						  .addWindow(vve::WindowSetup{}.id("main").title("VVE Scene Hierarchy - Car").extent(
							  vve::PixelExtent{.width = 960, .height = 540}))
						  .build();
	if (const auto result = engine.init(); !result) {
		std::cerr << "[hierarchy_car] failed: stage=engine init, error=" << vve::errorName(result.error()) << '\n';
		return 1;
	}
	auto world = engine.world();
	auto assets = world.get<vve::AssetSystem>();
	auto render = world.get<vve::RenderSystem>();

	// Load the car once and keep its node tree in the render instance.
	const auto scene = assets.loadScene(root / "assets/hierarchy_car/car.gltf");
	if (!scene) {
		std::cerr << "[hierarchy_car] failed: stage=load car, error=" << vve::errorName(scene.error()) << '\n';
		return 2;
	}
	const auto car = render.instantiateScene(*scene, vve::SceneInstantiationOptions{.keep_hierarchy = true});
	if (!car) {
		std::cerr << "[hierarchy_car] failed: stage=instantiate, error=" << vve::errorName(car.error()) << '\n';
		return 3;
	}

	// Look up the nodes that move by name; their imported local transforms give the mount positions.
	std::map<std::string, vve::NodeHandle> nodes{};
	for (const auto node : assets.sceneNodes(*scene).value_or(vve::Vector<vve::NodeHandle>{})) {
		if (const auto name = assets.nodeName(node)) { nodes[name->value] = node; }
	}
	const std::array mounts{"MountFrontLeft", "MountFrontRight"};
	const std::array wheels{"WheelFrontLeft", "WheelFrontRight", "WheelRearLeft", "WheelRearRight"};
	for (const auto *name : {"Car", mounts[0], mounts[1], wheels[0], wheels[1], wheels[2], wheels[3]}) {
		if (!nodes.contains(name)) {
			std::cerr << "[hierarchy_car] failed: stage=find node, node=" << name << '\n';
			return 4;
		}
	}
	const auto mount_offset = assets.nodeTransform(nodes.at("MountFrontLeft")).value_or(vve::Transform{}).translation.value;

	// The front-left wheel node has two meshes (tyre and rim); both objects must follow it, so check both.
	vve::Vector<vve::RenderObjectHandle> checked_parts{};
	for (const auto object : render.sceneInstanceObjects(*car).value_or(vve::Vector<vve::RenderObjectHandle>{})) {
		if (render.objectSourceNode(object) == nodes.at("WheelFrontLeft")) { checked_parts.push_back(object); }
	}
	if (checked_parts.size() != 2U) {
		std::cerr << "[hierarchy_car] failed: stage=find wheel parts, count=" << checked_parts.size() << '\n';
		return 4;
	}

	// Ground, light and a camera that sees the whole circle.
	if (!render.addPlane(vve::Vec2{12.0F, 12.0F}, vve::LinearColor{.value = vve::Vec3{0.45F, 0.47F, 0.42F}})) { return 5; }
	render.setDirectionalLight(vve::Direction{.value = vve::Vec3{-0.4F, -1.0F, -0.3F}}, vve::LinearColor{.value = vve::Vec3{1.0F, 0.97F, 0.9F}},
							   vve::LightIntensity{.value = 1.2F}, vve::LinearColor{.value = vve::Vec3{0.3F, 0.32F, 0.36F}});
	render.setCamera(vve::Camera::lookAt(vve::Position{.value = vve::Vec3{0.0F, 10.0F, 15.0F}},
										 vve::Position{.value = vve::Vec3{0.0F, 0.0F, 0.0F}}));

	std::ofstream verify{};
	if (max_frames > 0) {
		std::filesystem::create_directories(verify_path.parent_path());
		verify.open(verify_path);
		verify << "frame,car_x,car_z,yaw,wheel_x,wheel_y,wheel_z,expected_x,expected_y,expected_z,error\n";
	}
	const float steer = -std::atan(wheelBase / circleRadius); // The car turns towards negative yaw.
	float worst_error{};

	for (int frame{}; max_frames == 0 || frame < max_frames; ++frame) {
		const float time = static_cast<float>(frame) * frameTime;
		const float angle = speed * time / circleRadius;					// Position on the circle.
		const auto position = vve::Vec3{circleRadius * std::cos(angle), 0.0F, circleRadius * std::sin(angle)};
		const float yaw = -(angle + std::numbers::pi_v<float> / 2.0F);		// Car +X points along the circle.
		const float spin = -speed * time / wheelRadius;							// Rolling forward turns the wheel about -Z.

		// Only local transforms are set; the engine updates every object below each changed node.
		bool moved = render.setSceneInstanceNodeTransform(*car, nodes.at("Car"), local(position, aboutY(yaw))).has_value();
		for (const auto *mount : mounts) {
			const auto offset = assets.nodeTransform(nodes.at(mount)).value_or(vve::Transform{}).translation.value;
			moved = moved && render.setSceneInstanceNodeTransform(*car, nodes.at(mount), local(offset, aboutY(steer))).has_value();
		}
		for (const auto *wheel : wheels) {
			moved = moved && render.setSceneInstanceNodeTransform(*car, nodes.at(wheel), local({}, aboutZ(spin))).has_value();
		}
		if (!moved) {
			std::cerr << "[hierarchy_car] failed: stage=set node transforms, frame=" << frame << '\n';
			return 6;
		}

		// Independent check: wheel = car position + yaw-rotated mount offset (the wheel has no offset of its own),
		// and its rotation = yaw * steering (both about Y) * spin (about Z), multiplied out by hand.
		const auto expected = vve::Vec3{position.x + std::cos(yaw) * mount_offset.x + std::sin(yaw) * mount_offset.z,
										position.y + mount_offset.y,
										position.z - std::sin(yaw) * mount_offset.x + std::cos(yaw) * mount_offset.z};
		const float half_turn = (yaw + steer) / 2.0F, half_spin = spin / 2.0F;
		const auto expected_rotation = vve::Quat{std::cos(half_turn) * std::cos(half_spin), std::sin(half_turn) * std::sin(half_spin),
												 std::sin(half_turn) * std::cos(half_spin), std::cos(half_turn) * std::sin(half_spin)};
		float error{};
		auto wheel = vve::Vec3{};
		for (const auto part : checked_parts) {
			const auto actual = render.objectTransform(part);
			if (!actual) {
				std::cerr << "[hierarchy_car] failed: stage=read wheel, error=" << vve::errorName(actual.error()) << '\n';
				return 7;
			}
			wheel = actual->translation.value;
			const auto q = actual->rotation.value;
			const float alignment = std::abs(q.w * expected_rotation.w + q.x * expected_rotation.x + q.y * expected_rotation.y +
											 q.z * expected_rotation.z);
			error = std::max({error, std::abs(wheel.x - expected.x), std::abs(wheel.y - expected.y), std::abs(wheel.z - expected.z),
							  1.0F - alignment});
		}
		worst_error = std::max(worst_error, error);
		if (verify.is_open()) {
			verify << std::format("{},{:.4f},{:.4f},{:.4f},{:.4f},{:.4f},{:.4f},{:.4f},{:.4f},{:.4f},{:.6f}\n", frame, position.x,
								  position.z, yaw, wheel.x, wheel.y, wheel.z, expected.x, expected.y, expected.z, error);
		}

		const auto status = engine.step();
		if (!status) {
			std::cerr << "[hierarchy_car] failed: stage=frame, frame=" << frame << ", error=" << vve::errorName(status.error()) << '\n';
			return 8;
		}
		if (*status == vve::FrameStatus::stopped) { break; }
	}

	// Bounded runs keep a picture of the last frame for review.
	if (max_frames > 0) {
		if (const auto result = render.captureFrameToPng(capture_path); !result) {
			std::cerr << "[hierarchy_car] failed: stage=capture, error=" << vve::errorName(result.error()) << '\n';
			return 9;
		}
		verify << std::format("worst_error,{:.6f}\nresult,{}\n", worst_error, worst_error <= tolerance ? "PASS" : "FAIL");
	}
	std::println("[hierarchy_car] frames={} worst_wheel_error={:.6f} {}", max_frames, worst_error,
				 worst_error <= tolerance ? "PASS" : "FAIL");
	return worst_error <= tolerance ? 0 : 10;
}
