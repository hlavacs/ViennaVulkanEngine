// Prototype of the minimal scene tree: one world root node, one downward update, no dirty flags.
// Checked against the engine's public types and against the current keep_hierarchy code (4b54ba1a).
import std;
import VVEngine.Types;

namespace proto {

	using vve::Error;
	using vve::Mat4;
	using vve::Quat;
	using vve::RenderObjectHandle;
	using vve::Transform;
	using vve::Vec3;
	using vve::Vector;

	struct SceneNodeHandleTag {};
	using SceneNodeHandle = vve::TypedHandle<SceneNodeHandleTag>; ///< Node of the world tree.

	/// @brief Same TRS -> matrix rule as vve::simple::detail::modelMatrix.
	[[nodiscard]] Mat4 modelMatrix(const Transform &t) {
		const auto q = t.rotation.value;
		auto r = vve::identityMat4();
		r[0][0] = 1 - 2 * (q.y * q.y + q.z * q.z); r[0][1] = 2 * (q.x * q.y + q.w * q.z); r[0][2] = 2 * (q.x * q.z - q.w * q.y);
		r[1][0] = 2 * (q.x * q.y - q.w * q.z); r[1][1] = 1 - 2 * (q.x * q.x + q.z * q.z); r[1][2] = 2 * (q.y * q.z + q.w * q.x);
		r[2][0] = 2 * (q.x * q.z + q.w * q.y); r[2][1] = 2 * (q.y * q.z - q.w * q.x); r[2][2] = 1 - 2 * (q.x * q.x + q.y * q.y);
		return vve::math::scale(vve::math::multiply(vve::math::translate(vve::identityMat4(), t.translation.value), r), t.scale.value);
	}

	/// @brief The whole proposal: a tree with one world root. `write` stands in for setting RenderInstance::world_transform.
	class SceneTree {
	public:
		using Write = std::function<void(RenderObjectHandle, const Mat4 &)>;

		explicit SceneTree(Write write) : write_{std::move(write)} { nodes_.emplace(root_, Node{}); }

		[[nodiscard]] SceneNodeHandle root() const { return root_; }

		auto createNode(SceneNodeHandle parent, Transform local = {}) -> std::expected<SceneNodeHandle, Error> {
			if (!nodes_.contains(parent)) { return std::unexpected(Error::missing_object); }
			const auto node = vve::makeCounterHandle<SceneNodeHandle>();
			nodes_.emplace(node, Node{.parent = parent, .local = local});
			nodes_.at(parent).children.push_back(node);
			return node;
		}

		auto setNodeTransform(SceneNodeHandle node, Transform local) -> std::expected<void, Error> {
			if (node == root_) { return std::unexpected(Error::invalid_argument); } // The world never moves.
			if (!nodes_.contains(node)) { return std::unexpected(Error::missing_object); }
			nodes_.at(node).local = local;
			update(node);
			return {};
		}

		/// @brief Moves a node (with everything below it) under another parent; its local transform is kept.
		auto setNodeParent(SceneNodeHandle node, SceneNodeHandle parent) -> std::expected<void, Error> {
			if (node == root_) { return std::unexpected(Error::invalid_argument); }
			if (!nodes_.contains(node) || !nodes_.contains(parent)) { return std::unexpected(Error::missing_object); }
			for (auto up = parent; up.valid(); up = nodes_.at(up).parent) {
				if (up == node) { return std::unexpected(Error::invalid_argument); } // Would make a loop.
			}
			std::erase(nodes_.at(nodes_.at(node).parent).children, node);
			nodes_.at(parent).children.push_back(node);
			nodes_.at(node).parent = parent;
			update(node);
			return {};
		}

		/// @brief Hangs a render object on a node; offset is its transform relative to the node.
		auto setObjectParent(RenderObjectHandle object, SceneNodeHandle node, Transform offset = {}) -> std::expected<void, Error> {
			if (!nodes_.contains(node)) { return std::unexpected(Error::missing_object); }
			nodes_.at(node).objects.push_back({object, offset});
			update(node);
			return {};
		}

		/// @brief Removes a node and everything below it; returns the objects the render system must delete.
		auto removeNode(SceneNodeHandle node) -> std::expected<Vector<RenderObjectHandle>, Error> {
			if (node == root_) { return std::unexpected(Error::invalid_argument); }
			if (!nodes_.contains(node)) { return std::unexpected(Error::missing_object); }
			std::erase(nodes_.at(nodes_.at(node).parent).children, node);
			Vector<RenderObjectHandle> objects{};
			Vector<SceneNodeHandle> pending{node};
			while (!pending.empty()) {
				const auto current = pending.back();
				pending.pop_back();
				for (const auto &[object, offset] : nodes_.at(current).objects) { objects.push_back(object); }
				pending.append_range(nodes_.at(current).children);
				nodes_.erase(current);
			}
			return objects;
		}

		[[nodiscard]] auto children(SceneNodeHandle node) const -> const Vector<SceneNodeHandle> & { return nodes_.at(node).children; }

		/// @brief The one update rule: multiply the ancestors once, then walk down and place every object below.
		void update(SceneNodeHandle node) {
			auto parent_world = vve::identityMat4();
			for (auto up = nodes_.at(node).parent; up.valid(); up = nodes_.at(up).parent) {
				parent_world = vve::math::multiply(modelMatrix(nodes_.at(up).local), parent_world);
			}
			down(node, parent_world);
		}

	private:
		struct Node {
			SceneNodeHandle parent{};											///< Invalid only for the world root.
			Vector<SceneNodeHandle> children{};								///< In creation order.
			Transform local{};													///< Relative to the parent.
			Vector<std::pair<RenderObjectHandle, Transform>> objects{};	///< Objects and their offsets.
		};

		void down(SceneNodeHandle node, const Mat4 &parent_world) {
			const auto &item = nodes_.at(node);
			const auto world = vve::math::multiply(parent_world, modelMatrix(item.local));
			for (const auto &[object, offset] : item.objects) { write_(object, vve::math::multiply(world, modelMatrix(offset))); }
			for (const auto child : item.children) { down(child, world); }
		}

		std::unordered_map<SceneNodeHandle, Node, vve::HandleHash<SceneNodeHandle>> nodes_{};
		SceneNodeHandle root_{vve::makeCounterHandle<SceneNodeHandle>()};
		Write write_{};
	};

} // namespace proto

namespace compat {
	using namespace proto;

	/// @brief The current engine code (keep_hierarchy, 4b54ba1a), copied step by step for comparison.
	struct OldHierarchy {
		struct Node { std::string parent; Vector<std::string> children; Transform local; Vector<RenderObjectHandle> objects; };
		std::map<std::string, Node> nodes;
		std::map<RenderObjectHandle, Mat4> *render{};
		void update(const std::string &n, const Mat4 &parent_world) {               // RenderSystem::updateHierarchy
			const auto world = vve::math::multiply(parent_world, modelMatrix(nodes.at(n).local));
			for (const auto o : nodes.at(n).objects) { (*render)[o] = world; }
			for (const auto &c : nodes.at(n).children) { update(c, world); }
		}
		void set(const std::string &n, Transform t) {                                // setSceneInstanceNodeTransform
			nodes.at(n).local = t;
			auto parent_world = vve::identityMat4();
			for (auto p = nodes.at(n).parent; !p.empty(); p = nodes.at(p).parent) {
				parent_world = vve::math::multiply(modelMatrix(nodes.at(p).local), parent_world);
			}
			update(n, parent_world);
		}
	};

	Quat aboutY(float a) { return Quat{std::cos(a / 2), 0.0F, std::sin(a / 2), 0.0F}; }
	Quat aboutZ(float a) { return Quat{std::cos(a / 2), 0.0F, 0.0F, std::sin(a / 2)}; }
	Transform tr(Vec3 t, Quat r = vve::identityQuat()) { return Transform{.translation = {.value = t}, .rotation = {.value = r}}; }

	/// @brief Runs the hierarchy_car motion on both versions; returns the largest difference of any matrix entry.
	float hierarchyCar(int frames) {
		const std::vector<std::tuple<std::string, std::string, Vec3, int>> layout{          // assets/hierarchy_car/car.gltf
			{"Car", "", {0, 0, 0}, 0},
			{"Body", "Car", {0, 0.75F, 0}, 1}, {"Cabin", "Car", {-0.3F, 1.45F, 0}, 1},
			{"MountFrontLeft", "Car", {1.3F, 0.4F, -0.95F}, 0}, {"MountFrontRight", "Car", {1.3F, 0.4F, 0.95F}, 0},
			{"MountRearLeft", "Car", {-1.3F, 0.4F, -0.95F}, 0}, {"MountRearRight", "Car", {-1.3F, 0.4F, 0.95F}, 0},
			{"WheelFrontLeft", "MountFrontLeft", {0, 0, 0}, 2}, {"WheelFrontRight", "MountFrontRight", {0, 0, 0}, 2},
			{"WheelRearLeft", "MountRearLeft", {0, 0, 0}, 2}, {"WheelRearRight", "MountRearRight", {0, 0, 0}, 2}};
		std::map<RenderObjectHandle, Mat4> old_render, new_render;
		OldHierarchy old{.render = &old_render};
		SceneTree tree{[&](RenderObjectHandle o, const Mat4 &m) { new_render[o] = m; }};
		const auto instance_root = *tree.createNode(tree.root()); // keep_hierarchy instance: identity node above the asset root
		std::map<std::string, SceneNodeHandle> nodes;
		for (const auto &[name, parent, offset, object_count] : layout) {
			old.nodes[name] = {.parent = parent, .local = tr(offset)};
			if (!parent.empty()) { old.nodes.at(parent).children.push_back(name); }
			nodes[name] = *tree.createNode(parent.empty() ? instance_root : nodes.at(parent), tr(offset));
			for (int i = 0; i < object_count; ++i) {
				const auto o = vve::makeCounterHandle<RenderObjectHandle>();
				old.nodes.at(name).objects.push_back(o);
				(void)tree.setObjectParent(o, nodes.at(name));
			}
		}
		old.set("Car", old.nodes.at("Car").local);

		float worst = 0;
		const float steer = -std::atan(2.6F / 6.0F);
		for (int frame = 0; frame < frames; ++frame) {
			const float time = static_cast<float>(frame) / 60.0F, angle = 3.0F * time / 6.0F;
			const Vec3 position{6.0F * std::cos(angle), 0.0F, 6.0F * std::sin(angle)};
			const float yaw = -(angle + std::numbers::pi_v<float> / 2.0F), spin = -3.0F * time / 0.4F;
			// Same call sequence as examples/hierarchy_car: root, two front mounts, four wheels.
			std::vector<std::pair<std::string, Transform>> calls{{"Car", tr(position, aboutY(yaw))}};
			for (const auto *m : {"MountFrontLeft", "MountFrontRight"}) { calls.push_back({m, tr(old.nodes.at(m).local.translation.value, aboutY(steer))}); }
			for (const auto *w : {"WheelFrontLeft", "WheelFrontRight", "WheelRearLeft", "WheelRearRight"}) { calls.push_back({w, tr({}, aboutZ(spin))}); }
			for (const auto &[n, t] : calls) {
				old.set(n, t);
				(void)tree.setNodeTransform(nodes.at(n), t);
			}
			for (const auto &[o, m] : old_render) {
				for (int c = 0; c < 4; ++c) {
					for (int r = 0; r < 4; ++r) { worst = std::max(worst, std::abs(m[c][r] - new_render.at(o)[c][r])); }
				}
			}
		}
		return worst;
	}
} // namespace compat

namespace {
	using namespace proto;
	int failures = 0;
	void check(bool ok, std::string_view what) {
		if (!ok) { ++failures; std::println("FAIL: {}", what); }
	}
	bool near(Vec3 a, Vec3 b) { return vve::math::length(vve::math::subtract(a, b)) < 1e-4F; }
	Vec3 pos(const Mat4 &m) { return Vec3{m[3][0], m[3][1], m[3][2]}; }
	Transform at(Vec3 t, Quat r = vve::identityQuat()) { return Transform{.translation = {.value = t}, .rotation = {.value = r}}; }
	const Quat quarter{0.7071068F, 0.0F, 0.7071068F, 0.0F};
}

int main() {
	std::map<RenderObjectHandle, Mat4> render; // Stands in for RenderInstance::world_transform.
	SceneTree tree{[&](RenderObjectHandle o, const Mat4 &m) { render[o] = m; }};
	auto object = [] { return vve::makeCounterHandle<RenderObjectHandle>(); };

	// The world root is fixed.
	check(!tree.setNodeTransform(tree.root(), at({1, 0, 0})).has_value(), "world root cannot move");
	check(!tree.removeNode(tree.root()).has_value(), "world root cannot be removed");

	// Rig of tests/SceneHierarchyTests.cpp under an instance root: Car -> Body(obj), Mount -> Wheel(obj).
	const auto instance = *tree.createNode(tree.root());
	const auto car = *tree.createNode(instance);
	const auto body = *tree.createNode(car, at({0, 1, 0}));
	const auto mount = *tree.createNode(car, at({2, 0, 0}));
	const auto wheel = *tree.createNode(mount, at({0, 0, 1}));
	const auto body_obj = object(), wheel_obj = object();
	(void)tree.setObjectParent(body_obj, body);
	(void)tree.setObjectParent(wheel_obj, wheel);
	check(near(pos(render[wheel_obj]), {2, 0, 1}), "initial wheel");

	(void)tree.setNodeTransform(car, at({10, 0, 0}));
	(void)tree.setNodeTransform(mount, at({2, 0, 0}, quarter));
	check(near(pos(render[wheel_obj]), {13, 0, 0}), "wheel swings around the turned mount");
	check(near(pos(render[body_obj]), {10, 1, 0}), "body follows the root");

	// A direct placement (setObjectTransform) survives a sibling move and is replaced when an ancestor is set.
	render[body_obj] = modelMatrix(at({50, 50, 50}));
	(void)tree.setNodeTransform(mount, at({3, 0, 0}));
	check(near(pos(render[body_obj]), {50, 50, 50}), "direct placement survives a sibling move");
	(void)tree.setNodeTransform(car, at({0, 0, 0}));
	check(near(pos(render[body_obj]), {0, 1, 0}), "direct placement replaced when an ancestor is set");

	// Car on a trailer: hang the instance root under the trailer node; the car moves with it.
	const auto trailer = *tree.createNode(tree.root(), at({100, 0, 0}));
	check(tree.setNodeParent(instance, trailer).has_value(), "attach car to trailer");
	check(near(pos(render[body_obj]), {100, 1, 0}), "car follows trailer");
	(void)tree.setNodeTransform(trailer, at({200, 0, 0}));
	check(near(pos(render[wheel_obj]), {203, 0, 1}), "wheel follows trailer");
	check(!tree.setNodeParent(trailer, wheel).has_value(), "loops are rejected");

	// A primitive hung on the mount with an offset (a "turret" cube one metre up).
	const auto cube = object();
	(void)tree.setObjectParent(cube, mount, at({0, 1, 0}));
	check(near(pos(render[cube]), {203, 1, 0}), "primitive placed by node plus offset");

	// Removing a subtree reports the objects to delete.
	const auto removed = tree.removeNode(mount);
	check(removed && removed->size() == 2, "remove reports wheel and cube");
	check(tree.children(car).size() == 1, "mount gone");

	// The hierarchy_car motion gives bit-identical object matrices to the current code.
	const float worst = compat::hierarchyCar(600);
	std::println("hierarchy_car, 600 frames: largest difference to the current algorithm = {}", worst);
	check(worst == 0.0F, "bit-identical to the current keep_hierarchy code");

	std::println("{}", failures == 0 ? "PROTOTYPE PASS" : "PROTOTYPE FAIL");
	return failures == 0 ? 0 : 1;
}
