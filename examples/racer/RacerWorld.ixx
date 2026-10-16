/**
 * @file
 * @brief Ring Racer countryside: finish arch, grandstand with crowd, paddock, town, farm, fields, pond, fences, hay
 * bales and tyre stacks around the circuit.
 *
 * Kenney models (assets/racer/kenney, CC0, added by hand; see the README) are placed where their files exist; a missing
 * file only leaves that piece out. Everything else is built from boxes, gabled roofs and cylinders, which MeshBatch
 * merges into one mesh per colour, so the whole countryside costs a few dozen draw calls.
 *
 * Functional objects:
 * - Countryside / buildCountryside: builds everything and returns the areas random scenery must keep clear of.
 * - grandstandCentre: where the crowd sits, for the crowd sound.
 * - Frame, MeshBatch, Kit: placement beside the track, merged procedural meshes and the kit model cache.
 * - polygon, box, roof, cylinder, cone, patch, fence, building, hayBale: the procedural shapes.
 */
export module Racer.World;
import std;
import VVEngine;
import Racer.Rules;
import Racer.Scene;

export namespace racer {

	/// @brief What the countryside built, as far as the rest of the game needs to know.
	struct Countryside {
		std::vector<Box2> reserved{};	///< Areas kept free of trees and plants.
		bool startArch{};				///< The Kenney finish arch stands over the start line instead of the yellow gate.
		std::size_t pieces{};			///< Kit models and merged meshes placed.
		std::size_t missing{};			///< Kit files that could not be loaded.
	};

	[[nodiscard]] Vec2f grandstandCentre(const Track &track);
	[[nodiscard]] auto buildCountryside(vve::RenderSystem &render, vve::AssetSystem &assets, const Track &track,
		const std::filesystem::path &root) -> std::expected<Countryside, vve::Error>;

} // namespace racer

namespace racer {

	namespace vm = vve::math;
	constexpr float ground{-0.15F};	///< Height of the grass, 15 cm below the road.

	/// @brief A place beside the track: origin on the centre line, x along the driving direction, z to its right.
	struct Frame {
		Vec2f origin{};	///< Centre-line point.
		Vec2f along{};		///< Driving direction.
		Vec2f right{};		///< Right of the driving direction, towards the infield.

		/// @brief Ground-plane point at a distance along the frame and a lateral offset (positive is right).
		[[nodiscard]] Vec2f point(float distance, float lateral) const { return origin + along * distance + right * lateral; }
		/// @brief Yaw that turns a model's +X along the track and its +Z to the right.
		[[nodiscard]] float yaw() const { return std::atan2(-along.y, along.x); }
	};

	/// @brief The frame at a distance along the track.
	[[nodiscard]] Frame frameAt(const Track &track, float distance) {
		const auto sample = track.at(distance);
		return {.origin = sample.position, .along = sample.tangent, .right = rightOf(sample.tangent)};
	}

	/// @brief A ground-plane rectangle; its long axis is turned by `turn` from +Z towards +X.
	[[nodiscard]] Box2 area(Vec2f centre, float halfX, float halfZ, float turn = 0.0F) {
		return {.centre = centre, .axis = {std::sin(turn), std::cos(turn)}, .halfLength = halfZ, .halfWidth = halfX};
	}

	/// @brief A local point turned about +Y by a yaw (as racer::aboutY) and moved to a base point.
	[[nodiscard]] vve::Vec3 turn(vve::Vec3 base, float yaw, vve::Vec3 local) {
		const float c = std::cos(yaw), s = std::sin(yaw);
		return {base.x + local.x * c + local.z * s, base.y + local.y, base.z - local.x * s + local.z * c};
	}

	/// @brief Appends a flat convex polygon as a triangle fan whose front faces `outward`, with its face normal.
	void polygon(vve::MeshDescriptor &mesh, std::span<const vve::Vec3> corners, vve::Vec3 outward) {
		auto normal = vm::cross(vm::subtract(corners[1], corners[0]), vm::subtract(corners[2], corners[0]));
		const bool flip = vm::dot(normal, outward) < 0.0F;
		normal = vm::normalize(flip ? vm::scale(normal, -1.0F) : normal);
		const auto base = static_cast<std::uint32_t>(mesh.positions.size());
		for (const auto &corner : corners) {
			mesh.positions.push_back(corner);
			mesh.normals.push_back(normal);
		}
		for (std::uint32_t k{1U}; k + 1U < corners.size(); ++k) {
			mesh.indices.push_back(base);
			mesh.indices.push_back(base + (flip ? k + 1U : k));
			mesh.indices.push_back(base + (flip ? k : k + 1U));
		}
	}

	/// @brief Appends a box standing on a base point: half extents along its turned X and Z, and a height.
	void box(vve::MeshDescriptor &mesh, vve::Vec3 base, float yaw, float halfX, float halfZ, float height) {
		const auto p = [&](float x, float y, float z) { return turn(base, yaw, {x, y, z}); };
		const float x = halfX, z = halfZ, h = height;
		const auto centre = p(0.0F, 0.5F * h, 0.0F);
		const std::array<std::array<vve::Vec3, 4>, 6> faces{{
			{p(x, 0, -z), p(x, h, -z), p(x, h, z), p(x, 0, z)}, {p(-x, 0, -z), p(-x, 0, z), p(-x, h, z), p(-x, h, -z)},
			{p(-x, h, -z), p(-x, h, z), p(x, h, z), p(x, h, -z)}, {p(-x, 0, -z), p(x, 0, -z), p(x, 0, z), p(-x, 0, z)},
			{p(-x, 0, z), p(x, 0, z), p(x, h, z), p(-x, h, z)}, {p(-x, 0, -z), p(-x, h, -z), p(x, h, -z), p(x, 0, -z)},
		}};
		for (const auto &face : faces) {
			const auto middle = vm::scale(vm::add(vm::add(face[0], face[1]), vm::add(face[2], face[3])), 0.25F);
			polygon(mesh, face, vm::subtract(middle, centre));
		}
	}

	/// @brief Appends a gabled roof whose eaves lie at the base point: slopes and underside into one mesh, the two
	/// triangular gables into another (usually the wall colour). The ridge runs along the turned X.
	void roof(vve::MeshDescriptor &slopes, vve::MeshDescriptor &gables, vve::Vec3 base, float yaw, float halfX, float halfZ, float rise) {
		const auto p = [&](float x, float y, float z) { return turn(base, yaw, {x, y, z}); };
		const auto inside = p(0.0F, 0.3F * rise, 0.0F);
		const auto away = [&](vve::Vec3 point) { return vm::subtract(point, inside); };
		polygon(slopes, std::array{p(-halfX, 0, halfZ), p(halfX, 0, halfZ), p(halfX, rise, 0), p(-halfX, rise, 0)}, away(p(0, 0.5F * rise, halfZ)));
		polygon(slopes, std::array{p(-halfX, 0, -halfZ), p(-halfX, rise, 0), p(halfX, rise, 0), p(halfX, 0, -halfZ)}, away(p(0, 0.5F * rise, -halfZ)));
		polygon(slopes, std::array{p(-halfX, 0, -halfZ), p(halfX, 0, -halfZ), p(halfX, 0, halfZ), p(-halfX, 0, halfZ)}, vve::Vec3{0, -1, 0});
		polygon(gables, std::array{p(halfX, 0, -halfZ), p(halfX, rise, 0), p(halfX, 0, halfZ)}, away(p(halfX, 0, 0)));
		polygon(gables, std::array{p(-halfX, 0, -halfZ), p(-halfX, 0, halfZ), p(-halfX, rise, 0)}, away(p(-halfX, 0, 0)));
	}

	/// @brief Unit directions around an axis, for cylinders and cones.
	[[nodiscard]] std::vector<vve::Vec3> around(vve::Vec3 axis, int sides) {
		const auto helper = std::abs(axis.y) < 0.9F ? vve::Vec3{0, 1, 0} : vve::Vec3{1, 0, 0};
		const auto first = vm::normalize(vm::cross(axis, helper));
		const auto second = vm::cross(axis, first);
		std::vector<vve::Vec3> directions{};
		for (int k{}; k < sides; ++k) {
			const float angle = 2.0F * std::numbers::pi_v<float> * static_cast<float>(k) / static_cast<float>(sides);
			directions.push_back(vm::add(vm::scale(first, std::cos(angle)), vm::scale(second, std::sin(angle))));
		}
		return directions;
	}

	/// @brief Appends a closed cylinder between two points, with smooth sides.
	void cylinder(vve::MeshDescriptor &mesh, vve::Vec3 from, vve::Vec3 to, float radius, int sides = 12) {
		const auto axis = vm::normalize(vm::subtract(to, from));
		const auto directions = around(axis, sides);
		std::vector<vve::Vec3> bottom{}, top{};
		for (const auto &direction : directions) {
			bottom.push_back(vm::add(from, vm::scale(direction, radius)));
			top.push_back(vm::add(to, vm::scale(direction, radius)));
		}
		for (std::size_t k{}; k < directions.size(); ++k) {
			const std::size_t j = (k + 1U) % directions.size();
			const auto base = static_cast<std::uint32_t>(mesh.positions.size());
			for (const auto &[point, normal] : {std::pair{bottom[k], directions[k]}, std::pair{bottom[j], directions[j]},
					 std::pair{top[j], directions[j]}, std::pair{top[k], directions[k]}}) {
				mesh.positions.push_back(point);
				mesh.normals.push_back(normal);
			}
			for (const std::uint32_t index : {0U, 1U, 2U, 0U, 2U, 3U}) { mesh.indices.push_back(base + index); }
		}
		polygon(mesh, bottom, vm::scale(axis, -1.0F));
		polygon(mesh, top, axis);
	}

	/// @brief Appends an upright cone on a base point.
	void cone(vve::MeshDescriptor &mesh, vve::Vec3 base, float radius, float height, int sides = 12) {
		const auto apex = vm::add(base, vve::Vec3{0, height, 0});
		const auto directions = around(vve::Vec3{0, 1, 0}, sides);
		for (std::size_t k{}; k < directions.size(); ++k) {
			const auto &a = directions[k], &b = directions[(k + 1U) % directions.size()];
			polygon(mesh, std::array{vm::add(base, vm::scale(a, radius)), vm::add(base, vm::scale(b, radius)), apex}, vm::add(a, b));
		}
	}

	/// @brief Appends a flat rectangle at a height whose texture repeats every `tile` metres, rows along its long axis.
	void patch(vve::MeshDescriptor &mesh, const Box2 &where, float height, float tile) {
		const Vec2f across = rightOf(where.axis);
		const auto base = static_cast<std::uint32_t>(mesh.positions.size());
		for (const auto &[a, b] : std::array<std::pair<float, float>, 4>{{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}}) {
			mesh.positions.push_back(world(where.centre + where.axis * (a * where.halfLength) + across * (b * where.halfWidth), height));
			mesh.normals.push_back(vve::Vec3{0, 1, 0});
			mesh.uvs.push_back(vve::Vec2{(b + 1.0F) * where.halfWidth / tile, (a + 1.0F) * where.halfLength / tile});
		}
		// Counter-clockwise seen from above: the order of the corners depends on the handedness of axis and across.
		const bool upwards = cross(where.axis, across) < 0.0F;
		for (const std::uint32_t index : upwards ? std::array{0U, 1U, 2U, 0U, 2U, 3U} : std::array{0U, 2U, 1U, 0U, 3U, 2U}) {
			mesh.indices.push_back(base + index);
		}
	}

	/// @brief Procedural parts collected per colour; each colour becomes one render object.
	class MeshBatch {
	public:
		/// @brief The mesh for a colour, created on first use; flat things pass shadows = false.
		vve::MeshDescriptor &of(vve::Vec3 color, bool shadows = true) {
			for (auto &[mesh, casts] : meshes_) {
				if (mesh.color.value.x == color.x && mesh.color.value.y == color.y && mesh.color.value.z == color.z && casts == shadows) { return mesh; }
			}
			return meshes_.emplace_back(vve::MeshDescriptor{.color = {.value = color}}, shadows).first;
		}

		/// @brief Adds every non-empty mesh to the scene and counts it.
		auto addTo(vve::RenderSystem &render, std::size_t &pieces) -> std::expected<void, vve::Error> {
			for (auto &[mesh, casts] : meshes_) {
				if (mesh.indices.empty()) { continue; }
				const auto object = render.addMesh(std::move(mesh));
				if (!object) { return std::unexpected(object.error()); }
				if (!casts) { (void)render.setObjectCastsShadow(*object, false); }
				++pieces;
			}
			meshes_.clear();
			return {};
		}

	private:
		std::deque<std::pair<vve::MeshDescriptor, bool>> meshes_{};	///< Mesh and whether it casts shadows; a deque keeps references valid.
	};

	/// @brief Kenney models, each loaded once on first use; files that do not load are counted and skipped.
	struct Kit {
		vve::RenderSystem &render;										///< Scene the copies go into.
		vve::AssetSystem &assets;										///< Loader.
		std::filesystem::path folder;									///< assets/racer/kenney.
		std::map<std::string, std::optional<vve::SceneHandle>> scenes{};	///< Loaded models; empty for missing files.
		std::size_t placed{};											///< Copies placed.
		std::size_t missing{};											///< Files that did not load.
		std::optional<vve::Error> failure{};							///< First render error; later placements are skipped.

		/// @brief Places one copy with its own node tree; the root carries position, yaw and uniform scale.
		void place(const std::string &file, vve::Vec3 position, float yaw, float scale) {
			if (failure) { return; }
			auto found = scenes.find(file);
			if (found == scenes.end()) {
				const auto scene = assets.loadScene(folder / file);
				missing += scene ? 0U : 1U;
				found = scenes.emplace(file, scene ? std::optional{*scene} : std::nullopt).first;
			}
			if (!found->second) { return; }
			const auto instance = render.instantiateScene(*found->second, vve::SceneInstantiationOptions{.keep_hierarchy = true});
			if (!instance) { failure = instance.error(); return; }
			const auto root = render.sceneInstanceRoot(*instance);
			if (!root) { failure = root.error(); return; }
			if (const auto moved = render.setNodeTransform(*root, at(position, aboutY(yaw), scale)); !moved) { failure = moved.error(); return; }
			++placed;
		}
	};

	/// @brief Wooden post-and-rail fence from a to b, posts every 2.5 m; a gap of `gap` metres is left in its middle.
	void fence(MeshBatch &batch, Vec2f a, Vec2f b, float gap = 0.0F) {
		const vve::Vec3 wood{0.32F, 0.22F, 0.13F};
		const float span = length(b - a);
		const Vec2f direction = normalize(b - a);
		const float yaw = std::atan2(-direction.y, direction.x);
		const auto part = [&](float from, float to) {
			if (to - from < 0.5F) { return; }
			const int posts = std::max(1, static_cast<int>(std::ceil((to - from) / 2.5F)));
			for (int k{}; k <= posts; ++k) {
				const float along = from + (to - from) * static_cast<float>(k) / static_cast<float>(posts);
				box(batch.of(wood), world(a + direction * along, ground), yaw, 0.07F, 0.07F, 1.3F);
			}
			for (const float height : {0.45F, 0.95F}) {
				box(batch.of(wood), world(a + direction * (0.5F * (from + to)), ground + height), yaw, 0.5F * (to - from), 0.035F, 0.12F);
			}
		};
		part(0.0F, 0.5F * (span - gap));
		part(0.5F * (span + gap), span);
	}

	/// @brief Fence round a rectangle; side 0..3 gets a gate gap of 5 m (-1 for none).
	void fenceAround(MeshBatch &batch, const Box2 &where, int gateSide = -1) {
		const Vec2f across = rightOf(where.axis);
		const auto corner = [&](float a, float b) { return where.centre + where.axis * (a * where.halfLength) + across * (b * where.halfWidth); };
		const std::array corners{corner(-1, -1), corner(1, -1), corner(1, 1), corner(-1, 1)};
		for (int side{}; side < 4; ++side) {
			fence(batch, corners[static_cast<std::size_t>(side)], corners[static_cast<std::size_t>((side + 1) % 4)], side == gateSide ? 5.0F : 0.0F);
		}
	}

	/// @brief A round hay bale lying on the grass.
	void hayBale(MeshBatch &batch, Vec2f spot, float yaw) {
		const Vec2f axis{std::cos(yaw), std::sin(yaw)};
		cylinder(batch.of({0.62F, 0.50F, 0.24F}), world(spot - axis * 0.6F, ground + 0.75F), world(spot + axis * 0.6F, ground + 0.75F), 0.75F, 14);
	}

	/// @brief A house or barn: walls, gabled roof, a door and a row of windows on its +Z side (no windows: a barn door).
	void building(MeshBatch &batch, vve::Vec3 base, float yaw, float halfX, float halfZ, float height, float rise, vve::Vec3 wall,
		vve::Vec3 roofColor, int windows) {
		const vve::Vec3 doorColor{0.22F, 0.13F, 0.07F}, glass{0.10F, 0.13F, 0.17F};
		box(batch.of(wall), base, yaw, halfX, halfZ, height);
		roof(batch.of(roofColor), batch.of(wall), turn(base, yaw, {0, height, 0}), yaw, halfX + 0.5F, halfZ + 0.6F, rise);
		box(batch.of(doorColor), turn(base, yaw, {0, 0, halfZ}), yaw, windows == 0 ? 0.3F * halfX : 0.6F, 0.05F, windows == 0 ? 0.75F * height : 2.1F);
		for (int k{}; k < windows; ++k) {
			const float x = halfX * (-0.75F + 1.5F * (static_cast<float>(k) + 0.5F) / static_cast<float>(windows));
			if (std::abs(x) < 1.0F) { continue; }	// Leave room for the door.
			box(batch.of(glass), turn(base, yaw, {x, 1.1F, halfZ}), yaw, 0.45F, 0.05F, 1.1F);
		}
	}

	Vec2f grandstandCentre(const Track &track) { return frameAt(track, -6.0F).point(0.0F, -19.0F); }

	/// @brief Builds the countryside; see the file comment for the parts and the README for the map.
	auto buildCountryside(vve::RenderSystem &render, vve::AssetSystem &assets, const Track &track, const std::filesystem::path &root)
		-> std::expected<Countryside, vve::Error> {
		Countryside result{};
		MeshBatch batch{};
		Kit kit{.render = render, .assets = assets, .folder = root / "assets/racer/kenney"};
		std::mt19937 random{2026U};
		std::uniform_real_distribution<float> unit{0.0F, 1.0F};
		const float quarter = 0.5F * std::numbers::pi_v<float>;	// Kenney fronts and roads point along +Z; frames turn +X along.
		// The see-through grass variation layer lies 7 cm above the grass and tints whatever is lower, so the asphalt of
		// the Kenney street tiles (0.2 m above their base at scale 8) and the pond stay just above it.
		constexpr float streetBase{-0.26F};

		// Start line: the inflatable arch across the road, scaled so that the inner faces of its legs meet the gate posts.
		// It stands 1.3 m deep in the ground, which hides the model's own road piece and the low anchor blocks along it.
		const auto start = frameAt(track, 0.0F);
		kit.place("racing/models/track-finish.glb", world(start.origin, -1.45F), start.yaw() + quarter, 1.68F);
		result.startArch = kit.placed > 0U;

		// Grandstand outside the start straight: six concrete steps, benches, a crowd, a back wall and a roof on columns.
		const auto stand = frameAt(track, -6.0F);
		constexpr int rows{6};
		constexpr float front{-14.0F}, depth{1.5F}, rise{0.75F}, standHalf{22.0F};
		const std::array<vve::Vec3, 7> shirts{{{0.75F, 0.08F, 0.06F}, {0.10F, 0.25F, 0.70F}, {0.85F, 0.85F, 0.82F}, {0.95F, 0.70F, 0.08F},
			{0.12F, 0.45F, 0.15F}, {0.08F, 0.08F, 0.09F}, {0.55F, 0.20F, 0.55F}}};
		const std::array<vve::Vec3, 3> skins{{{0.80F, 0.60F, 0.45F}, {0.55F, 0.38F, 0.25F}, {0.30F, 0.20F, 0.14F}}};
		for (int k{}; k < rows; ++k) {
			const float back = front - depth * static_cast<float>(k + 1), top = 0.5F + rise * static_cast<float>(k);
			box(batch.of({0.58F, 0.58F, 0.56F}), world(stand.point(0.0F, back + 0.5F * depth), ground), stand.yaw(), standHalf, 0.5F * depth, top - ground);
			for (int section{}; section < 4; ++section) {
				const vve::Vec3 seat = section % 2 == 0 ? vve::Vec3{0.12F, 0.30F, 0.75F} : vve::Vec3{0.80F, 0.12F, 0.10F};
				box(batch.of(seat), world(stand.point(-16.5F + 11.0F * static_cast<float>(section), back + 0.45F), top), stand.yaw(), 5.4F, 0.25F, 0.4F);
			}
			for (float along = -21.4F; along <= 21.4F; along += 0.62F) {
				if (unit(random) > 0.72F) { continue; }
				const auto seated = world(stand.point(along, back + 0.5F), top + 0.4F);
				box(batch.of(shirts[static_cast<std::size_t>(unit(random) * 6.999F)]), seated, stand.yaw(), 0.2F, 0.14F, 0.55F);
				box(batch.of(skins[static_cast<std::size_t>(unit(random) * 2.999F)]), vm::add(seated, vve::Vec3{0, 0.55F, 0}), stand.yaw(), 0.11F, 0.11F, 0.24F);
			}
		}
		const float standBack = front - depth * rows, roofHeight = 0.5F + rise * rows + 3.4F;
		box(batch.of({0.50F, 0.50F, 0.49F}), world(stand.point(0.0F, standBack - 0.2F), ground), stand.yaw(), standHalf, 0.2F, roofHeight - ground);
		for (float along = -standHalf; along <= standHalf + 0.1F; along += 11.0F) {
			box(batch.of({0.42F, 0.43F, 0.46F}), world(stand.point(along, front + 0.3F), ground), stand.yaw(), 0.15F, 0.15F, roofHeight - ground);
		}
		box(batch.of({0.88F, 0.88F, 0.86F}), world(stand.point(0.0F, 0.5F * (front + standBack)), roofHeight), stand.yaw(), standHalf + 1.0F,
			0.5F * (front - standBack) + 1.2F, 0.25F);
		box(batch.of({0.80F, 0.12F, 0.10F}), world(stand.point(0.0F, front + 0.15F), ground), stand.yaw(), standHalf, 0.05F, 1.1F);	// Advertising board.
		result.reserved.push_back({.centre = grandstandCentre(track), .axis = stand.along, .halfLength = standHalf + 2.0F, .halfWidth = 7.0F});

		// Paddock in the infield behind the start straight: pit garages facing the track, a lit paddock road, tents and trucks.
		const auto paddock = frameAt(track, -6.0F);
		for (float along = -16.0F; along <= 16.0F; along += 8.0F) {
			kit.place("city/models/building-garage.glb", world(paddock.point(along, 19.5F), ground), paddock.yaw() + 2.0F * quarter, 8.0F);
		}
		for (float along = -20.0F; along <= 20.0F; along += 8.0F) {
			kit.place("city/models/road-straight-lightposts.glb", world(paddock.point(along, 29.5F), streetBase), paddock.yaw() + quarter, 8.0F);
		}
		for (const float along : {-12.0F, 6.0F}) {
			kit.place("racing/models/decoration-tents.glb", world(paddock.point(along, 41.0F), -0.35F), paddock.yaw(), 1.2F);
		}
		const std::array<std::tuple<const char *, float, float, float>, 4> trucks{{{"racing/models/vehicle-truck-red.glb", 19.0F, 38.0F, 0.3F},
			{"racing/models/vehicle-truck-yellow.glb", 23.0F, 43.0F, 0.6F}, {"racing/models/vehicle-truck-green.glb", -23.0F, 37.0F, -0.4F},
			{"racing/models/vehicle-truck-purple.glb", -24.5F, 44.5F, 2.8F}}};
		for (const auto &[file, along, lateral, angle] : trucks) {
			kit.place(file, world(paddock.point(along, lateral), ground), paddock.yaw() + angle, 2.2F);
		}
		result.reserved.push_back({.centre = paddock.point(0.0F, 31.5F), .axis = paddock.along, .halfLength = 27.0F, .halfWidth = 17.0F});

		// A small town east of the circuit: a lit street of nine tiles, houses on both sides and a fountain square.
		const std::array houses{"city/models/building-small-a.glb", "city/models/building-small-b.glb", "city/models/building-small-c.glb",
			"city/models/building-small-d.glb"};
		for (int k{}; k < 9; ++k) {
			const float z = -52.0F + 8.0F * static_cast<float>(k);
			kit.place("city/models/road-straight-lightposts.glb", {180.0F, streetBase, z}, 0.0F, 8.0F);
			for (const float side : {-1.0F, 1.0F}) {
				if ((k == 3 && side < 0.0F) || (k == 6 && side > 0.0F)) { continue; }	// Two gaps between the houses.
				kit.place(houses[static_cast<std::size_t>(unit(random) * 3.999F)], {180.0F + 8.0F * side, ground, z}, -side * quarter, 8.0F);
			}
		}
		for (const float x : {172.0F, 188.0F}) { kit.place("city/models/pavement.glb", {x, streetBase, -60.0F}, 0.0F, 8.0F); }
		kit.place("city/models/pavement-fountain.glb", {180.0F, streetBase, -60.0F}, 0.0F, 8.0F);
		result.reserved.push_back(area({180.0F, -22.0F}, 16.0F, 44.0F));

		// Farm west of the circuit: barn, farmhouse, silo, an open shed with straw, a fenced yard, a pasture and a hay meadow.
		const vve::Vec3 barnRed{0.45F, 0.08F, 0.05F}, slate{0.20F, 0.20F, 0.23F}, white{0.85F, 0.82F, 0.75F}, tiles{0.50F, 0.16F, 0.10F},
			straw{0.66F, 0.54F, 0.27F}, timber{0.22F, 0.13F, 0.07F};
		building(batch, {-188.0F, ground, 12.0F}, 0.0F, 7.5F, 5.0F, 5.5F, 4.0F, barnRed, slate, 0);
		building(batch, {-168.0F, ground, 30.0F}, 0.0F, 5.0F, 4.0F, 5.0F, 3.2F, white, tiles, 4);
		cylinder(batch.of({0.55F, 0.60F, 0.62F}), {-198.0F, ground, 30.0F}, {-198.0F, ground + 11.0F, 30.0F}, 2.6F, 18);
		cone(batch.of({0.40F, 0.42F, 0.45F}), {-198.0F, ground + 11.0F, 30.0F}, 2.8F, 2.2F, 18);
		for (const float x : {-176.0F, -168.0F}) {
			for (const float z : {1.5F, 6.5F}) { box(batch.of(timber), {x, ground, z}, 0.0F, 0.12F, 0.12F, 3.6F); }
		}
		box(batch.of(slate), {-172.0F, ground + 3.6F, 4.0F}, 0.0F, 4.8F, 3.2F, 0.2F);
		for (int k{}; k < 18; ++k) {
			box(batch.of(straw), {-175.0F + 1.2F * static_cast<float>(k % 6), ground + 0.45F * static_cast<float>(k / 6), 3.5F}, 0.0F, 0.55F, 0.9F, 0.45F);
		}
		kit.place("racing/models/vehicle-truck-red.glb", {-178.0F, ground, 22.0F}, 2.2F, 2.2F);
		const auto yard = area({-180.0F, 20.0F}, 24.0F, 24.0F), pasture = area({-177.0F, 73.0F}, 27.0F, 25.0F), meadow = area({-134.0F, 76.0F}, 12.0F, 24.0F);
		fenceAround(batch, yard, 1);
		fenceAround(batch, pasture);
		fenceAround(batch, meadow, 3);
		for (int k{}; k < 16; ++k) { hayBale(batch, meadow.centre + Vec2f{(unit(random) - 0.5F) * 20.0F, (unit(random) - 0.5F) * 42.0F}, 6.28F * unit(random)); }
		result.reserved.insert(result.reserved.end(), {yard, pasture, meadow});

		// Fields north and south of the circuit and one in the infield; the harvested wheat field carries bales.
		const std::array<std::pair<Box2, const char *>, 8> fields{{
			{area({-111.0F, 170.0F}, 29.0F, 38.0F, 0.04F), "wheat"}, {area({-49.5F, 170.0F}, 27.0F, 38.0F, -0.03F), "plowed"},
			{area({10.5F, 172.0F}, 27.0F, 36.0F, 0.02F), "crops"}, {area({71.5F, 168.0F}, 28.0F, 39.0F, -0.05F), "wheat"},
			{area({-51.0F, -157.0F}, 29.0F, 35.0F, 0.03F), "crops"}, {area({14.0F, -157.0F}, 31.0F, 35.0F, -0.02F), "wheat"},
			{area({79.0F, -157.0F}, 29.0F, 35.0F, 0.05F), "plowed"}, {area({-23.0F, 8.0F}, 22.0F, 19.0F, 0.3F), "crops"},
		}};
		std::map<std::string, vve::MeshDescriptor> fieldMeshes{};
		for (const auto &[where, kind] : fields) {
			auto &mesh = fieldMeshes[kind];
			mesh.base_color_texture = root / "assets/racer" / (std::string{kind} + ".tga");
			patch(mesh, where, -0.05F, 8.0F);
			result.reserved.push_back(where);
		}
		for (int k{}; k < 20; ++k) {
			const auto &stubble = fields[5].first;
			hayBale(batch, stubble.centre + Vec2f{(unit(random) - 0.5F) * 50.0F, (unit(random) - 0.5F) * 60.0F}, 6.28F * unit(random));
		}
		fence(batch, {-142.0F, 130.0F}, {102.0F, 130.0F}, 6.0F);
		fence(batch, {-82.0F, -119.0F}, {110.0F, -119.0F}, 6.0F);
		for (auto &[kind, mesh] : fieldMeshes) {
			const auto object = render.addMesh(std::move(mesh));
			if (!object) { return std::unexpected(object.error()); }
			(void)render.setObjectCastsShadow(*object, false);
			++result.pieces;
		}

		// Pond in the infield: dark water inside a muddy shore, with a few reeds.
		const Vec2f pond{26.0F, 4.0F};
		for (const auto &[grow, height, color] : std::array<std::tuple<float, float, vve::Vec3>, 2>{{{1.2F, -0.07F, {0.30F, 0.24F, 0.16F}},
				 {0.0F, -0.06F, {0.08F, 0.18F, 0.26F}}}}) {
			std::vector<vve::Vec3> rim{};
			for (int k{}; k < 32; ++k) {
				const float angle = 6.2832F * static_cast<float>(k) / 32.0F;
				rim.push_back(world(pond + Vec2f{(10.0F + grow) * std::cos(angle), (7.5F + grow) * std::sin(angle)}, height));
			}
			polygon(batch.of(color, false), rim, vve::Vec3{0, 1, 0});
		}
		for (int k{}; k < 26; ++k) {
			const float angle = 6.2832F * unit(random);
			const auto spot = pond + Vec2f{10.6F * std::cos(angle), 8.1F * std::sin(angle)};
			box(batch.of({0.25F, 0.40F, 0.12F}), world(spot, ground), angle, 0.03F, 0.03F, 0.8F + 0.7F * unit(random));
		}
		result.reserved.push_back(area(pond, 12.0F, 9.5F));

		// Tyre stacks behind the outer wall of every sharp bend, black with every third stack white.
		for (std::size_t i{}; i < track.samples().size(); i += 2U) {
			const auto &sample = track.samples()[i];
			if (std::abs(sample.curvature) < 0.035F) { continue; }
			const float side = sample.curvature > 0.0F ? -1.0F : 1.0F;	// A right bend has its outside on the left.
			const Vec2f spot = sample.position + rightOf(sample.tangent) * (side * (Track::wallOffset + 0.8F));
			const vve::Vec3 tyre = i % 6U == 0U ? vve::Vec3{0.80F, 0.80F, 0.78F} : vve::Vec3{0.05F, 0.05F, 0.055F};
			for (int level{}; level < 4; ++level) {
				const float bottom = ground + 0.25F * static_cast<float>(level);
				cylinder(batch.of(tyre), world(spot, bottom), world(spot, bottom + 0.23F), 0.33F, 10);
			}
		}

		if (kit.failure) { return std::unexpected(*kit.failure); }
		if (const auto added = batch.addTo(render, result.pieces); !added) { return std::unexpected(added.error()); }
		result.pieces += kit.placed;
		result.missing = kit.missing;
		return result;
	}

} // namespace racer
