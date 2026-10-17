/** @file @brief Streamed Kenney airport workers, passengers and service vehicles, using only VVE's facade. */
export module Island.Airport;
import std;
import VVEngine;
import Island.Logic;

export namespace island {

inline constexpr unsigned airportPeopleCount{12U}; ///< Walking passengers and ground crew per nearby airport.
inline constexpr unsigned airportActorCount{15U}; ///< Twelve people and three service vehicles; no unbounded spawning.

/** @brief Selects a shared character or vehicle model; repeated instances reuse its imported meshes and palette. */
std::string airportModel(unsigned actor) {
	constexpr std::array<std::string_view,6> files{"people/character-male-a.glb", "people/character-female-a.glb",
		"people/character-male-b.glb", "people/character-female-b.glb",
		"vehicles/vehicle-truck-yellow.glb", "vehicles/vehicle-truck-green.glb"};
	return std::string{files[actor < airportPeopleCount ? actor % 4U : 4U + (actor - airportPeopleCount) % 2U]};
}

/** @brief Follows a closed service lane with rounded ends, clear of the runway, unloading area, hangar and cargo stacks.
 * @param seconds Shared flight clock: retaining the time freezes activity, and resetting it repeats the same scene.
 */
vve::Transform airportPose(std::size_t id, unsigned actor, double seconds) {
	const bool person = actor < airportPeopleCount;
	const bool terminal = person && actor >= 6U;
	const float radius = person ? (terminal ? 7.0F : 2.0F) : 5.0F;
	const float centreX = person ? (terminal ? 60.0F : 45.0F) : 35.0F;
	const float north = person ? (terminal ? -145.0F : -176.0F) : -246.0F;
	const float south = person ? (terminal ? -143.0F : -134.0F) : -187.0F;
	const float straight = south - north, arc = std::numbers::pi_v<float> * radius;
	const float length = 2.0F * (straight + arc), speed = person ? 1.15F : 2.8F;
	const double travel = static_cast<double>(length / speed), pause = person ? 4.0 : 0.0;
	// Stagger arrivals at the terminal; people pause briefly before continuing their circuit.
	const double phase = std::fmod(std::max(0.0, seconds) + actor * 11.7 + id * 0.37, travel + pause);
	float distance = static_cast<float>(std::min(phase, travel)) * speed;
	float x{}, z{}, yaw{};
	if (distance < straight) { x = centreX + radius; z = north + distance; }
	else if ((distance -= straight) < arc) {
		const float angle = distance / radius;
		x = centreX + radius * std::cos(angle); z = south + radius * std::sin(angle); yaw = -angle;
	} else if ((distance -= arc) < straight) {
		x = centreX - radius; z = south - distance; yaw = std::numbers::pi_v<float>;
	} else {
		const float angle = std::numbers::pi_v<float> + (distance - straight) / radius;
		x = centreX + radius * std::cos(angle); z = north + radius * std::sin(angle); yaw = -angle;
	}
	const auto centre = islandCenter(id); const float scale = person ? 2.65F : 1.35F;
	return {.translation={.value={centre.x+x, runwayHeight+0.04F, centre.z+z}},
		.rotation={.value={std::cos(yaw*0.5F),0,std::sin(yaw*0.5F),0}}, .scale={.value={scale,scale,scale}}};
}

/** @brief Keeps activity only at detailed resident airports and animates the retained instances without reloading models.
 * @param traffic Owns instances by island; removing a resident airport releases its actors and copied scene nodes.
 * @param models Small per-game cache of six source scenes, shared across all airports.
 */
auto updateAirportActivity(vve::RenderSystem &render, vve::AssetSystem &assets, const std::filesystem::path &root,
	const std::map<std::size_t,vve::Vector<vve::RenderObjectHandle>> &resident,
	std::map<std::size_t,vve::Vector<vve::RenderSceneInstanceHandle>> &traffic,
	std::map<std::string,vve::SceneHandle> &models, double seconds) -> std::expected<void,vve::Error> {
	// Distant terrain needs no people; remove entire instances instead of leaving hidden objects behind.
	for (auto entry=traffic.begin(); entry!=traffic.end();) {
		const auto found=resident.find(entry->first);
		if (found!=resident.end() && found->second.size()>8U) { ++entry; continue; }
		for (const auto instance : entry->second) {
			if (auto result=render.removeSceneInstance(instance); !result) { return result; }
		}
		entry=traffic.erase(entry);
	}
	// Model imports occur once per game; only new detailed airports create instances.
	for (const auto &[id,objects] : resident) {
		if (objects.size()<=8U) { continue; }
		auto &actors=traffic[id];
		for (unsigned actor=static_cast<unsigned>(actors.size()); actor<airportActorCount; ++actor) {
			const auto file=airportModel(actor);
			if (!models.contains(file)) {
				const auto model=assets.loadScene(root/file);
				if (!model) { return std::unexpected(model.error()); }
				models.emplace(file,*model);
			}
			const auto instance=render.instantiateScene(models.at(file),{.keep_hierarchy=true});
			if (!instance) { return std::unexpected(instance.error()); }
			actors.push_back(*instance); // Retain ownership even if a subsequent import or placement fails.
		}
		// Placement and joint poses are absolute functions of time, so pauses and repeated test frames never drift.
		for (unsigned actor{}; actor<actors.size(); ++actor) {
			const auto instance=actors[actor]; const auto model=models.at(airportModel(actor));
			const auto parent=render.sceneInstanceRoot(instance);
			if (!parent) { return std::unexpected(parent.error()); }
			if (auto result=render.setNodeTransform(*parent,airportPose(id,actor,seconds)); !result) { return result; }
			const auto nodes=assets.sceneNodes(model); if (!nodes) { return std::unexpected(nodes.error()); }
			// A stationary traveller rests their arms and legs instead of walking on the spot.
			const auto now=airportPose(id,actor,seconds).translation.value;
			const auto next=airportPose(id,actor,seconds+0.01).translation.value;
			const bool moving=vve::math::lengthSquared(vve::math::subtract(next,now))>0.000001F;
			for (const auto node : *nodes) {
				const auto name=assets.nodeName(node); if (!name) { return std::unexpected(name.error()); }
				const bool arm=name->value.starts_with("arm-"), leg=name->value.starts_with("leg-");
				const bool wheel=name->value.starts_with("wheel-");
				if (!(arm || leg || wheel)) { continue; }
				auto local=assets.nodeTransform(node); if (!local) { return std::unexpected(local.error()); }
				const float side=name->value.ends_with("left") ? 1.0F : -1.0F;
				const float swing=moving ? std::sin(static_cast<float>(std::fmod(seconds,1000.0))*6.2F+actor)*0.48F : 0.0F;
				const float a=wheel ? static_cast<float>(std::fmod(seconds*2.8/0.405,2*std::numbers::pi)) : swing*side*(arm?-0.7F:1.0F);
				const vve::Quat step{std::cos(a*0.5F),std::sin(a*0.5F),0,0};
				const float rest=arm ? -side*1.32F : 0.0F;
				local->rotation.value=vve::math::multiply(step,vve::Quat{std::cos(rest*0.5F),0,0,std::sin(rest*0.5F)});
				if (auto result=render.setSceneInstanceNodeTransform(instance,node,*local); !result) { return result; }
			}
		}
	}
	return {};
}

} // namespace island
