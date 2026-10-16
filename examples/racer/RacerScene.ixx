/**
 * @file
 * @brief Ring Racer visuals built through the public facade: circuit meshes, car rigs and a pool of smoke sprites.
 *
 * Functional objects:
 * - world, aboutY, aboutZ, at: conversions from the ground-plane logic to facade transforms.
 * - buildCircuit: normal-mapped grass with a large-scale colour variation layer, textured asphalt, edge lines, kerbs,
 *   centre line, start line, barrier walls and transparent checkpoint gates.
 * - addScenery: trees, bushes, grass tufts, flowers and rocks from the Kenney Nature Kit, scattered beside the track and
 *   kept out of the areas the countryside uses.
 * - CarModel / loadCarModel: a Kenney car (body and four wheel nodes), scaled to the physics wheel base, with its box size.
 * - CarRig / addCar: one instantiated car with brake lights hung on its instance root.
 * - placeCar, setBrakeLights, setCarOpacity: per-frame car updates through the scene tree.
 * - SmokePool: reused sprites that rise, grow and fade behind sliding cars.
 */
export module Racer.Scene;
import std;
import VVEngine;
import Racer.Rules;

export namespace racer {

	/// @brief World point above a ground-plane point.
	[[nodiscard]] inline vve::Vec3 world(Vec2f point, float height = 0.0F) { return vve::Vec3{point.x, height, point.y}; }
	/// @brief Rotation by an angle about +Y; the Quat constructor order is w, x, y, z.
	[[nodiscard]] inline vve::Quat aboutY(float angle) { return vve::Quat{std::cos(angle / 2.0F), 0.0F, std::sin(angle / 2.0F), 0.0F}; }
	/// @brief Rotation by an angle about +X, the wheel axle of the Kenney cars.
	[[nodiscard]] inline vve::Quat aboutX(float angle) { return vve::Quat{std::cos(angle / 2.0F), std::sin(angle / 2.0F), 0.0F, 0.0F}; }
	/// @brief Product of two rotations: first b, then a.
	[[nodiscard]] inline vve::Quat combine(vve::Quat a, vve::Quat b) {
		return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z, a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
			a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
	}
	/// @brief Transform from a translation, a rotation and a uniform scale.
	[[nodiscard]] inline vve::Transform at(vve::Vec3 translation, vve::Quat rotation = vve::Quat{1, 0, 0, 0}, float scale = 1.0F) {
		return {.translation = {.value = translation}, .rotation = {.value = rotation}, .scale = {.value = vve::Vec3{scale, scale, scale}}};
	}
	/// @brief World yaw of a Kenney car (+Z forward) for a ground-plane heading.
	[[nodiscard]] inline float yawOf(float heading) { return std::numbers::pi_v<float> / 2.0F - heading; }

	[[nodiscard]] auto buildCircuit(vve::RenderSystem &render, vve::AssetSystem &assets, const Track &track, const std::filesystem::path &root,
		bool startArch) -> std::expected<void, vve::Error>;
	[[nodiscard]] auto addScenery(vve::RenderSystem &render, vve::AssetSystem &assets, const Track &track, const std::filesystem::path &root,
		std::uint32_t seed, std::span<const Box2> reserved) -> std::expected<std::size_t, vve::Error>;

	/// @brief A loaded car model: its wheel nodes and the scale that gives it the physics wheel base.
	struct CarModel {
		vve::SceneHandle scene{};								///< Asset scene of one Kenney car.
		std::array<vve::NodeHandle, 4> wheels{};			///< Front left, front right, back left, back right.
		std::array<vve::Vec3, 4> wheelOffsets{};			///< Their imported positions (model units).
		float scale{1.0F};										///< Model units to metres.
		vve::Bounds box{};										///< Model box in model units (+Z forward).

		[[nodiscard]] float halfLength() const { return 0.5F * scale * (box.maximum.value.z - box.minimum.value.z); }	///< Collider half length.
		[[nodiscard]] float halfWidth() const { return 0.5F * scale * (box.maximum.value.x - box.minimum.value.x); }	///< Collider half width.
		[[nodiscard]] float front() const { return scale * box.maximum.value.z; }	///< Nose distance ahead of the car centre.
	};

	/// @brief One car in the render scene.
	struct CarRig {
		vve::RenderSceneInstanceHandle instance{};			///< Instance with its own copy of the node tree.
		vve::SceneNodeHandle root{};							///< Instance root: position, yaw and scale of the whole car.
		std::vector<vve::RenderObjectHandle> parts{};		///< Every object of the car, for picking and opacity.
		std::array<vve::RenderObjectHandle, 2> brakeLights{};	///< Unlit cuboids hung on the instance root.
		bool braking{};												///< Current brake-light state.
	};

	[[nodiscard]] auto loadCarModel(vve::AssetSystem &assets, const std::filesystem::path &path, float wheelBase)
		-> std::expected<CarModel, vve::Error>;
	[[nodiscard]] auto addCar(vve::RenderSystem &render, const CarModel &model) -> std::expected<CarRig, vve::Error>;
	void placeCar(vve::RenderSystem &render, const CarModel &model, const CarRig &rig, const GhostFrame &pose);
	void setBrakeLights(vve::RenderSystem &render, CarRig &rig, bool on);
	void setCarOpacity(vve::RenderSystem &render, const CarRig &rig, float opacity);

	/// @brief Smoke and dust puffs: a fixed set of sprites, reused in turn.
	class SmokePool {
	public:
		[[nodiscard]] auto create(vve::RenderSystem &render, const std::filesystem::path &texture, std::size_t count)
			-> std::expected<void, vve::Error>;
		void emit(vve::Vec3 position, bool dust);
		void update(vve::RenderSystem &render, float dt);
		[[nodiscard]] std::size_t active() const;

	private:
		/// @brief One puff.
		struct Puff {
			vve::RenderObjectHandle smoke{};	///< Grey sprite.
			vve::RenderObjectHandle dust{};	///< Brown sprite, used on grass.
			vve::Vec3 position{};				///< Current centre.
			float age{-1.0F};						///< Seconds since emission; negative when free.
			bool isDust{};							///< Which of the two sprites is shown.
		};
		std::vector<Puff> puffs_{};			///< All puffs.
		std::size_t next_{};					///< Next puff to reuse.
	};

} // namespace racer

namespace racer {

	/// @brief Appends quads between two point rows, quad i joining rows i and i+1 (closed), each oriented towards facing(i).
	void appendStrip(vve::MeshDescriptor &mesh, const std::vector<vve::Vec3> &a, const std::vector<vve::Vec3> &b,
		const std::function<vve::Vec3(std::size_t)> &facing, const std::function<bool(std::size_t)> &keep) {
		const auto sub = [](vve::Vec3 l, vve::Vec3 r) { return vve::math::subtract(l, r); };
		for (std::size_t i{}; i < a.size(); ++i) {
			if (!keep(i)) { continue; }
			const std::size_t j = (i + 1U) % a.size();
			// Counter-clockwise front faces: swap the rows when the quad would face away.
			auto normal = vve::math::cross(sub(b[i], a[i]), sub(a[j], a[i]));
			const bool flip = vve::math::dot(normal, facing(i)) < 0.0F;
			const auto &first = flip ? b : a;
			const auto &second = flip ? a : b;
			normal = vve::math::normalize(flip ? vve::math::scale(normal, -1.0F) : normal);
			const auto base = static_cast<std::uint32_t>(mesh.positions.size());
			for (const auto &point : {first[i], second[i], first[j], second[j]}) {
				mesh.positions.push_back(point);
				mesh.normals.push_back(normal);
			}
			for (const std::uint32_t index : {0U, 1U, 2U, 2U, 1U, 3U}) { mesh.indices.push_back(base + index); }
		}
	}

	/// @brief Row of points at a lateral offset and height along the whole track.
	std::vector<vve::Vec3> row(const Track &track, float lateral, float height) {
		std::vector<vve::Vec3> points{};
		for (const auto &sample : track.samples()) { points.push_back(world(sample.position + rightOf(sample.tangent) * lateral, height)); }
		return points;
	}

	/// @brief Adds one flat band between two lateral offsets as a single mesh.
	auto addBand(vve::RenderSystem &render, const Track &track, float from, float to, float height, vve::Vec3 color,
		const std::function<bool(std::size_t)> &keep) -> std::expected<vve::RenderObjectHandle, vve::Error> {
		vve::MeshDescriptor mesh{.color = {.value = color}};
		appendStrip(mesh, row(track, from, height), row(track, to, height), [](std::size_t) { return vve::Vec3{0, 1, 0}; }, keep);
		return render.addMesh(std::move(mesh));
	}

	/// @brief The asphalt as one mesh; its texture repeats every 3 m across and along the road.
	auto addRoad(vve::RenderSystem &render, const Track &track, const std::filesystem::path &texture) -> std::expected<vve::RenderObjectHandle, vve::Error> {
		constexpr float tile{3.0F}, hw{Track::halfWidth};
		const auto left = row(track, -hw, 0.0F), right = row(track, hw, 0.0F);
		const float step = track.length() / static_cast<float>(track.samples().size());
		vve::MeshDescriptor mesh{.base_color_texture = texture};
		for (std::size_t i{}; i < left.size(); ++i) {
			const std::size_t j = (i + 1U) % left.size();
			const float v = static_cast<float>(i) * step / tile, next = v + step / tile;
			const auto base = static_cast<std::uint32_t>(mesh.positions.size());
			for (const auto &[point, uv] : {std::pair{left[i], vve::Vec2{-hw / tile, v}}, std::pair{right[i], vve::Vec2{hw / tile, v}},
					 std::pair{left[j], vve::Vec2{-hw / tile, next}}, std::pair{right[j], vve::Vec2{hw / tile, next}}}) {
				mesh.positions.push_back(point);
				mesh.normals.push_back(vve::Vec3{0, 1, 0});
				mesh.uvs.push_back(uv);
			}
			// Counter-clockwise seen from above, whichever way the road turns.
			const auto normal = vve::math::cross(vve::math::subtract(right[i], left[i]), vve::math::subtract(left[j], left[i]));
			for (const std::uint32_t index : normal.y > 0.0F ? std::array{0U, 1U, 2U, 2U, 1U, 3U} : std::array{0U, 2U, 1U, 2U, 3U, 1U}) {
				mesh.indices.push_back(base + index);
			}
		}
		return render.addMesh(std::move(mesh));
	}

	/// @brief Builds the static circuit; every part is one mesh, so the whole track costs a dozen draw calls. With the
	/// Kenney finish arch standing over the start line, the yellow start gate is left out (its posts still collide).
	auto buildCircuit(vve::RenderSystem &render, vve::AssetSystem &assets, const Track &track, const std::filesystem::path &root, bool startArch)
		-> std::expected<void, vve::Error> {
		const auto all = [](std::size_t) { return true; };
		const auto fail = [](const auto &result) { return !result.has_value(); };
		// The ground (assets/racer/ground.obj) is normal-mapped grass 15 cm below the road, tiled every 6 m.
		const auto ground = assets.loadScene(root / "assets/racer/ground.obj");
		if (!ground) { return std::unexpected(ground.error()); }
		if (const auto placed = render.instantiateScene(*ground); fail(placed)) { return std::unexpected(placed.error()); }
		// A thin see-through layer of large noise patches hides the repetition of the grass tiles. The renderer sorts
		// see-through objects by origin, so the origin lies 500 m below: the layer is always blended first, under gates and smoke.
		constexpr float edge{1200.0F}, lift{500.0F - 0.08F}, repeat{9.0F};
		vve::MeshDescriptor layer{.positions = {vve::Vec3{-edge, lift, -edge}, vve::Vec3{-edge, lift, edge}, vve::Vec3{edge, lift, edge}, vve::Vec3{edge, lift, -edge}},
			.indices = {0, 1, 2, 0, 2, 3}, .normals = vve::Vector<vve::Vec3>(4, vve::Vec3{0, 1, 0}),
			.uvs = {vve::Vec2{0, 0}, vve::Vec2{0, repeat}, vve::Vec2{repeat, repeat}, vve::Vec2{repeat, 0}},
			.base_color_texture = root / "assets/racer/variation.tga", .transform = at(vve::Vec3{0, -500.0F, 0})};
		const auto variation = render.addMesh(std::move(layer));
		if (fail(variation)) { return std::unexpected(variation.error()); }
		if (const auto result = render.setObjectOpacity(*variation, 0.4F); !result) { return result; }
		constexpr float hw = Track::halfWidth, kerb = Track::kerbWidth;
		const vve::Vec3 red{0.75F, 0.08F, 0.06F}, white{0.9F, 0.9F, 0.9F};
		const std::array bands{
			addRoad(render, track, root / "assets/racer/asphalt.tga"),
			addBand(render, track, -hw + 0.25F, -hw + 0.45F, 0.02F, white, all),
			addBand(render, track, hw - 0.45F, hw - 0.25F, 0.02F, white, all),
			addBand(render, track, -0.15F, 0.15F, 0.02F, white, [](std::size_t i) { return i % 4U < 2U; }),
			addBand(render, track, -hw, hw, 0.025F, white, [](std::size_t i) { return i == 0U; }),
		};
		for (const auto &band : bands) { if (fail(band)) { return std::unexpected(band.error()); } }
		// Kerbs alternate red and white every two samples on both sides.
		for (const float side : {-1.0F, 1.0F}) {
			for (const bool even : {true, false}) {
				const auto kerbBand = addBand(render, track, side * hw, side * (hw + kerb), 0.015F, even ? red : white,
					[even](std::size_t i) { return (i / 2U % 2U == 0U) == even; });
				if (fail(kerbBand)) { return std::unexpected(kerbBand.error()); }
			}
		}
		// Each wall is one mesh: an inner face, an outer face and a top.
		for (const float side : {-1.0F, 1.0F}) {
			const float inner = side * Track::wallOffset, outer = side * (Track::wallOffset + 0.4F);
			vve::MeshDescriptor wall{.color = {.value = vve::Vec3{0.55F, 0.6F, 0.68F}}};
			// The inner face looks at the road, the outer face away from it.
			const auto roadward = [&track, side](std::size_t i) { return world(rightOf(track.samples()[i].tangent) * -side); };
			const auto away = [&roadward](std::size_t i) { return vve::math::scale(roadward(i), -1.0F); };
			appendStrip(wall, row(track, inner, -0.15F), row(track, inner, 0.9F), roadward, all);
			appendStrip(wall, row(track, outer, -0.15F), row(track, outer, 0.9F), away, all);
			appendStrip(wall, row(track, inner, 0.9F), row(track, outer, 0.9F), [](std::size_t) { return vve::Vec3{0, 1, 0}; }, all);
			if (const auto added = render.addMesh(std::move(wall)); fail(added)) { return std::unexpected(added.error()); }
		}
		// Checkpoint gates: the track's two post colliders drawn as tall cuboids, joined by a bar; see-through, the start gate yellow.
		for (int checkpoint{startArch ? 1 : 0}; checkpoint < RaceProgress::checkpoints; ++checkpoint) {
			const auto &left = track.posts()[2U * static_cast<std::size_t>(checkpoint)];
			const auto &right = track.posts()[2U * static_cast<std::size_t>(checkpoint) + 1U];
			const Vec2f across = rightOf(left.axis);
			const auto rotation = aboutY(std::atan2(-across.y, across.x));	// Cuboid +X across the road.
			const float half = Track::postHalf, reach = Track::postOffset;
			const vve::LinearColor color{.value = checkpoint == 0 ? vve::Vec3{1.0F, 0.85F, 0.1F} : vve::Vec3{0.2F, 0.8F, 1.0F}};
			std::vector<std::expected<vve::RenderObjectHandle, vve::Error>> parts{};
			for (const auto *post : {&left, &right}) {
				parts.push_back(render.addCuboid(vve::Vec3{-half, -0.1F, -half}, vve::Vec3{half, 5.5F, half}, color, at(world(post->centre), rotation)));
			}
			parts.push_back(render.addCuboid(vve::Vec3{-reach - half, -0.3F, -half}, vve::Vec3{reach + half, 0.3F, half}, color,
				at(world((left.centre + right.centre) * 0.5F, 5.5F), rotation)));
			for (const auto &part : parts) {
				if (fail(part)) { return std::unexpected(part.error()); }
				if (const auto result = render.setObjectOpacity(*part, 0.35F); !result) { return result; }
			}
		}
		return {};
	}

	/// @brief One kind of scenery: a Nature Kit model, how many, how large, and how far from the centre line.
	struct SceneryKind {
		const char *file;			///< File in assets/racer/nature.
		int count;					///< Number of copies.
		float minScale, maxScale;	///< Random uniform scale range (the models are about 1/7 of real size).
		float nearest, farthest;	///< Distance range from the nearest track centre line.
		bool shadows;				///< Small plants cast no shadow, which keeps the shadow passes cheap.
	};

	/// @brief Natural colour for a Nature Kit material; the kit's own colours are bright, metallic and mint green.
	[[nodiscard]] vve::Vec3 natureColor(std::string_view material) {
		constexpr std::array<std::pair<std::string_view, vve::Vec3>, 10> palette{{
			{"grass", {0.10F, 0.26F, 0.05F}}, {"leafsGreen", {0.08F, 0.22F, 0.04F}}, {"leafsDark", {0.04F, 0.13F, 0.03F}},
			{"woodBark", {0.20F, 0.11F, 0.05F}}, {"woodBarkDark", {0.12F, 0.07F, 0.04F}}, {"dirt", {0.18F, 0.12F, 0.07F}},
			{"colorRed", {0.70F, 0.05F, 0.04F}}, {"colorYellow", {0.85F, 0.65F, 0.05F}}, {"colorPurple", {0.35F, 0.08F, 0.60F}},
			{"_defaultMat", {0.30F, 0.29F, 0.27F}},
		}};
		const auto found = std::ranges::find(palette, material, &std::pair<std::string_view, vve::Vec3>::first);
		return found == palette.end() ? vve::Vec3{0.3F, 0.3F, 0.3F} : found->second;
	}

	/// @brief Scatters trees, bushes, grass, flowers and rocks behind the walls, near the track and in a sparser belt
	/// further out, never inside a reserved area; every copy keeps its node tree so one root transform places, turns and
	/// scales it. Placement uses a fixed seed, so the scenery is the same every run.
	auto addScenery(vve::RenderSystem &render, vve::AssetSystem &assets, const Track &track, const std::filesystem::path &root,
		std::uint32_t seed, std::span<const Box2> reserved) -> std::expected<std::size_t, vve::Error> {
		constexpr std::array kinds{
			SceneryKind{"tree_oak.glb", 20, 6.0F, 9.0F, 16.0F, 90.0F, true}, SceneryKind{"tree_default.glb", 20, 6.0F, 9.0F, 16.0F, 90.0F, true},
			SceneryKind{"tree_detailed.glb", 16, 6.0F, 9.0F, 16.0F, 90.0F, true}, SceneryKind{"tree_fat.glb", 14, 6.0F, 8.0F, 16.0F, 90.0F, true},
			SceneryKind{"tree_pineTallA.glb", 20, 6.0F, 9.0F, 16.0F, 90.0F, true}, SceneryKind{"tree_pineRoundA.glb", 16, 6.0F, 8.0F, 16.0F, 90.0F, true},
			SceneryKind{"tree_oak.glb", 30, 7.0F, 10.0F, 90.0F, 230.0F, true}, SceneryKind{"tree_pineTallA.glb", 30, 7.0F, 10.0F, 90.0F, 230.0F, true},
			SceneryKind{"tree_default.glb", 25, 7.0F, 10.0F, 90.0F, 230.0F, true},
			SceneryKind{"plant_bushLarge.glb", 30, 4.0F, 6.0F, 13.0F, 60.0F, true}, SceneryKind{"plant_bushDetailed.glb", 30, 4.0F, 6.0F, 13.0F, 60.0F, true},
			SceneryKind{"grass_large.glb", 120, 3.0F, 4.5F, 11.3F, 40.0F, false}, SceneryKind{"grass_leafsLarge.glb", 90, 3.0F, 4.5F, 11.3F, 40.0F, false},
			SceneryKind{"flower_redA.glb", 35, 2.5F, 3.5F, 11.3F, 30.0F, false}, SceneryKind{"flower_yellowA.glb", 35, 2.5F, 3.5F, 11.3F, 30.0F, false},
			SceneryKind{"flower_purpleA.glb", 35, 2.5F, 3.5F, 11.3F, 30.0F, false},
			SceneryKind{"rock_largeA.glb", 15, 3.0F, 5.0F, 13.0F, 70.0F, true}, SceneryKind{"rock_largeB.glb", 15, 3.0F, 5.0F, 13.0F, 70.0F, true},
		};
		// Trees keep a few metres from a reserved area so that their crowns do not reach into it.
		const auto blocked = [reserved](Vec2f spot, float margin) {
			return std::ranges::any_of(reserved, [&](const Box2 &box) {
				const Vec2f offset = spot - box.centre;
				return std::abs(dot(offset, box.axis)) < box.halfLength + margin && std::abs(dot(offset, rightOf(box.axis))) < box.halfWidth + margin;
			});
		};
		std::mt19937 random{seed};
		std::uniform_real_distribution<float> unit{0.0F, 1.0F};
		std::size_t placed{};
		for (const auto &kind : kinds) {
			const auto scene = assets.loadScene(root / "assets/racer/nature" / kind.file);
			if (!scene) { return std::unexpected(scene.error()); }
			// Colour of every mesh, per node in mesh order; instance objects keep that order per source node.
			std::map<vve::NodeHandle, std::vector<vve::Vec3>> colors{};
			for (const auto node : assets.sceneNodes(*scene).value_or(vve::Vector<vve::NodeHandle>{})) {
				for (const auto mesh : assets.nodeMeshes(node).value_or(vve::Vector<vve::MeshHandle>{})) {
					const auto material = assets.meshMaterial(mesh);
					const auto name = material ? assets.materialName(*material) : std::unexpected(vve::Error::missing_object);
					colors[node].push_back(natureColor(name ? name->value : std::string{}));
				}
			}
			for (int copy{}; copy < kind.count; ++copy) {
				// Pick a spot beside a random point of the track; reject spots that come too close to another part of it.
				Vec2f spot{};
				bool found{};
				for (int attempt{}; attempt < 20 && !found; ++attempt) {
					const auto sample = track.at(unit(random) * track.length());
					const float lateral = kind.nearest + (kind.farthest - kind.nearest) * unit(random) * unit(random);	// Denser near the track.
					spot = sample.position + rightOf(sample.tangent) * (unit(random) < 0.5F ? -lateral : lateral);
					found = std::abs(track.locate(spot).lateral) >= kind.nearest && length(spot) < 380.0F && !blocked(spot, kind.shadows ? 3.0F : 0.5F);
				}
				if (!found) { continue; }
				const auto instance = render.instantiateScene(*scene, vve::SceneInstantiationOptions{.keep_hierarchy = true});
				if (!instance) { return std::unexpected(instance.error()); }
				const auto node = render.sceneInstanceRoot(*instance);
				if (!node) { return std::unexpected(node.error()); }
				const float scale = kind.minScale + (kind.maxScale - kind.minScale) * unit(random);
				if (const auto result = render.setNodeTransform(*node, at(world(spot, -0.15F), aboutY(6.2832F * unit(random)), scale)); !result) {
					return std::unexpected(result.error());
				}
				// Natural colours with one of five brightness steps per copy; the steps keep the number of materials small.
				const float tint = 0.8F + 0.1F * static_cast<float>(static_cast<int>(unit(random) * 4.999F));
				std::map<vve::NodeHandle, std::size_t> next{};
				for (const auto part : render.sceneInstanceObjects(*instance).value_or(vve::Vector<vve::RenderObjectHandle>{})) {
					const auto node = render.objectSourceNode(part).value_or(vve::NodeHandle{});
					const auto &list = colors[node];
					if (const std::size_t index = next[node]++; index < list.size()) {
						(void)render.setObjectColor(part, vve::LinearColor{.value = vve::math::scale(list[index], tint)});
					}
					if (!kind.shadows) { (void)render.setObjectCastsShadow(part, false); }
				}
				++placed;
			}
		}
		return placed;
	}

	/// @brief Loads one car, finds its wheels by name, and measures its box from the mesh bounds of all nodes.
	auto loadCarModel(vve::AssetSystem &assets, const std::filesystem::path &path, float wheelBase) -> std::expected<CarModel, vve::Error> {
		const auto scene = assets.loadScene(path);
		if (!scene) { return std::unexpected(scene.error()); }
		CarModel model{.scene = *scene};
		constexpr std::array names{"wheel-front-left", "wheel-front-right", "wheel-back-left", "wheel-back-right"};
		for (const auto node : assets.sceneNodes(*scene).value_or(vve::Vector<vve::NodeHandle>{})) {
			const auto offset = assets.nodeTransform(node).value_or(vve::Transform{}).translation.value;
			if (const auto name = assets.nodeName(node)) {
				if (const auto wheel = std::ranges::find(names, name->value); wheel != names.end()) {
					model.wheels[static_cast<std::size_t>(wheel - names.begin())] = node;
					model.wheelOffsets[static_cast<std::size_t>(wheel - names.begin())] = offset;
				}
			}
			// The Kenney nodes only translate their meshes, so offset mesh boxes are enough.
			for (const auto mesh : assets.nodeMeshes(node).value_or(vve::Vector<vve::MeshHandle>{})) {
				const auto bounds = assets.meshBounds(mesh);
				if (!bounds || !bounds->valid) { continue; }
				const auto low = vve::math::add(bounds->minimum.value, offset), high = vve::math::add(bounds->maximum.value, offset);
				model.box.minimum.value = model.box.valid ? vve::math::min(model.box.minimum.value, low) : low;
				model.box.maximum.value = model.box.valid ? vve::math::max(model.box.maximum.value, high) : high;
				model.box.valid = true;
			}
		}
		const float axles = model.wheelOffsets[0].z - model.wheelOffsets[2].z;
		if (!std::ranges::all_of(model.wheels, &vve::NodeHandle::valid) || !model.box.valid || axles <= 0.0F) {
			return std::unexpected(vve::Error::missing_object);
		}
		model.scale = wheelBase / axles;
		return model;
	}

	/// @brief Instantiates one car with its own node tree and hangs two brake lights on its instance root.
	auto addCar(vve::RenderSystem &render, const CarModel &model) -> std::expected<CarRig, vve::Error> {
		const auto instance = render.instantiateScene(model.scene, vve::SceneInstantiationOptions{.keep_hierarchy = true});
		if (!instance) { return std::unexpected(instance.error()); }
		const auto root = render.sceneInstanceRoot(*instance);
		if (!root) { return std::unexpected(root.error()); }
		CarRig rig{.instance = *instance, .root = *root};
		const auto objects = render.sceneInstanceObjects(*instance).value_or(vve::Vector<vve::RenderObjectHandle>{});
		rig.parts.assign(objects.begin(), objects.end());
		// The root is still at the origin with unit scale, so the lights are placed in model units.
		const auto &box = model.box;
		const float height = box.minimum.value.y + 0.55F * (box.maximum.value.y - box.minimum.value.y);
		const float side = 0.32F * (box.maximum.value.x - box.minimum.value.x);
		for (std::size_t i{}; i < rig.brakeLights.size(); ++i) {
			const auto light = render.addCuboid(vve::Vec3{-0.07F, -0.03F, -0.01F}, vve::Vec3{0.07F, 0.03F, 0.01F},
				vve::LinearColor{.value = vve::Vec3{0.25F, 0.02F, 0.02F}}, at(vve::Vec3{i == 0 ? -side : side, height, box.minimum.value.z - 0.01F}));
			if (!light) { return std::unexpected(light.error()); }
			if (const auto result = render.setObjectUnlit(*light, true); !result) { return std::unexpected(result.error()); }
			if (const auto result = render.setObjectCastsShadow(*light, false); !result) { return std::unexpected(result.error()); }
			if (const auto result = render.setObjectParent(*light, *root); !result) { return std::unexpected(result.error()); }
			rig.brakeLights[i] = *light;
			rig.parts.push_back(*light);
		}
		return rig;
	}

	/// @brief Moves and scales the whole car through its instance root, steers the front wheels and spins all four.
	void placeCar(vve::RenderSystem &render, const CarModel &model, const CarRig &rig, const GhostFrame &pose) {
		(void)render.setNodeTransform(rig.root, at(world(pose.position), aboutY(yawOf(pose.heading)), model.scale));
		// Positive steering turns right, which is towards -X of the model; rolling forwards turns about +X.
		const auto spin = aboutX(pose.wheelAngle);
		const auto steer = combine(aboutY(-pose.steer), spin);
		for (std::size_t i{}; i < model.wheels.size(); ++i) {
			(void)render.setSceneInstanceNodeTransform(rig.instance, model.wheels[i], at(model.wheelOffsets[i], i < 2U ? steer : spin));
		}
	}

	/// @brief Switches both brake lights between bright and dark red, only when the state changes.
	void setBrakeLights(vve::RenderSystem &render, CarRig &rig, bool on) {
		if (rig.braking == on) { return; }
		rig.braking = on;
		for (const auto light : rig.brakeLights) {
			(void)render.setObjectColor(light, vve::LinearColor{.value = on ? vve::Vec3{1.0F, 0.05F, 0.03F} : vve::Vec3{0.25F, 0.02F, 0.02F}});
		}
	}

	/// @brief Makes every part of a car see-through (the ghost) or solid again.
	void setCarOpacity(vve::RenderSystem &render, const CarRig &rig, float opacity) {
		for (const auto part : rig.parts) { (void)render.setObjectOpacity(part, opacity); }
	}

	/// @brief Creates the hidden sprites; each puff has a grey and a brown one so colours never change at runtime.
	auto SmokePool::create(vve::RenderSystem &render, const std::filesystem::path &texture, std::size_t count) -> std::expected<void, vve::Error> {
		for (std::size_t i{}; i < count; ++i) {
			const auto smoke = render.addSprite(vve::Vec2{1, 1}, texture, vve::LinearColor{.value = vve::Vec3{0.85F, 0.85F, 0.88F}});
			const auto dust = render.addSprite(vve::Vec2{1, 1}, texture, vve::LinearColor{.value = vve::Vec3{0.55F, 0.42F, 0.28F}});
			if (!smoke) { return std::unexpected(smoke.error()); }
			if (!dust) { return std::unexpected(dust.error()); }
			(void)render.setObjectVisible(*smoke, false);
			(void)render.setObjectVisible(*dust, false);
			puffs_.push_back({.smoke = *smoke, .dust = *dust});
		}
		return {};
	}

	/// @brief Starts the oldest puff at a position.
	void SmokePool::emit(vve::Vec3 position, bool dust) {
		if (puffs_.empty()) { return; }
		auto &puff = puffs_[next_];
		next_ = (next_ + 1U) % puffs_.size();
		puff.position = position;
		puff.isDust = dust;
		puff.age = 0.0F;
	}

	/// @brief Rises, grows and fades every live puff over one second; finished ones are hidden.
	void SmokePool::update(vve::RenderSystem &render, float dt) {
		constexpr float lifetime{1.0F};
		for (auto &puff : puffs_) {
			const auto shown = puff.isDust ? puff.dust : puff.smoke;
			const auto hidden = puff.isDust ? puff.smoke : puff.dust;
			if (puff.age < 0.0F) { continue; }
			puff.age += dt;
			const bool alive = puff.age < lifetime;
			(void)render.setObjectVisible(shown, alive);
			(void)render.setObjectVisible(hidden, false);
			if (!alive) { puff.age = -1.0F; continue; }
			const float t = puff.age / lifetime;
			puff.position.y += 0.8F * dt;
			(void)render.setObjectTransform(shown, at(puff.position, vve::Quat{1, 0, 0, 0}, 1.0F + 2.5F * t));
			(void)render.setObjectOpacity(shown, 0.6F * (1.0F - t));
		}
	}

	/// @brief Counts the puffs currently shown.
	std::size_t SmokePool::active() const { return static_cast<std::size_t>(std::ranges::count_if(puffs_, [](const Puff &puff) { return puff.age >= 0.0F; })); }

} // namespace racer
