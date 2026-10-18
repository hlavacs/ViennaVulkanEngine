/** @file @brief CPU-only facade checks for downloaded island art, grounded aircraft placement and propeller animation. */
import std;
import VVEngine;
import Island.Scene;
import Island.Airport;
import Island.Game;

namespace {
int checks{}; ///< Successful named assertions, printed for deterministic automated diagnosis.

/** @brief Stops at the first broken scene contract and includes its stable name in the output. */
void check(bool condition, std::string_view name) {
	if (!condition) { throw std::runtime_error{std::string{name}}; }
	++checks;
}
/** @brief Compares world coordinates without relying on unexported implementation vector operators. */
[[nodiscard]] bool close(vve::Vec3 a, vve::Vec3 b, float epsilon = 0.001F) {
	return vve::math::length(vve::math::subtract(a,b)) < epsilon;
}
/** @brief Applies a unit quaternion to a direction to independently verify the visible aircraft's nose. */
[[nodiscard]] vve::Vec3 rotated(vve::Quat q, vve::Vec3 point) {
	const vve::Vec3 axis{q.x,q.y,q.z};
	const auto turn=vve::math::add(vve::math::cross(axis,point),vve::math::scale(point,q.w));
	return vve::math::add(point,vve::math::scale(vve::math::cross(axis,turn),2.0F));
}
/** @brief Finds the model's two propeller pivots using only its public render-scene hierarchy. */
[[nodiscard]] vve::Vector<vve::SceneNodeHandle> propellers(vve::RenderSystem &render, vve::SceneNodeHandle parent) {
	auto nodes=render.nodeChildren(parent);
	// The pinned glTF has one root chain followed by the two propeller leaves.
	for (unsigned depth{}; nodes && nodes->size()==1U && depth<8U; ++depth) { nodes=render.nodeChildren(nodes->front()); }
	check(nodes && nodes->size()==2U,"two_public_propeller_pivots");
	return *nodes;
}
/** @brief Checks the environment's asset errors, lighting and bounded batch counts without initializing a renderer. */
void environment(vve::RenderSystem &render, vve::AssetSystem &assets, const std::filesystem::path &root) {
	const auto missing=island::buildEnvironment(render,assets,root/"nonexistent-scene-test-directory");
	check(!missing && missing.error()==vve::Error::io_error,"missing_panorama_reports_io_error");
	check(render.sceneMeshCount()==0 && render.renderedFrameCount()==0,"missing_sky_has_no_geometry_or_frames");
	const auto result=island::buildEnvironment(render,assets,root);
	check(result.has_value(),"downloaded_environment_builds");
	check(render.materialLighting()==vve::MaterialLighting::physically_based,"physically_based_lighting");
	check(render.atmosphere().density>0 && render.atmosphere().height_falloff>0,"height_dependent_coastal_haze");
	check(render.sceneDirectionalLightCount()==1 && render.scenePointLightCount()==0,"one_sun_without_fake_point_lights");
	check(render.sceneMeshCount()==2 && render.sceneInstanceCount()==2,"static_setup_defers_island_geometry");
	check(render.sceneTextureCount()==1 && render.renderedFrameCount()==0,"static_panorama_needs_no_gpu_frames");
	std::println("[IslandDeliverySceneTests] meshes={} materials={} textures={} vertices={} indices={} haze={}",
		render.sceneMeshCount(),render.sceneMaterialCount(),render.sceneTextureCount(),render.sceneVertexCount(),
		render.sceneIndexCount(),render.atmosphere().density);
}
/** @brief Verifies the real imported aircraft, including wheel contact, forward direction and deterministic propeller poses. */
std::pair<vve::SceneNodeHandle,vve::RenderObjectHandle> aircraft(vve::RenderSystem &render, vve::AssetSystem &assets, const std::filesystem::path &root) {
	// Isolate the aircraft so ray picking cannot select the large ground batches' bounding boxes.
	render.clearScene();
	const auto parent=render.createNode(render.sceneRoot()); check(parent.has_value(),"aircraft_parent_created");
	const auto missing=island::buildAircraft(render,assets,*parent,root/"nonexistent-scene-test-directory");
	check(!missing && missing.error()==vve::Error::asset_import_failed,"missing_aircraft_reports_import_failure");
	check(render.sceneInstanceCount()==0,"missing_aircraft_leaves_no_instance");
	check(island::buildAircraft(render,assets,*parent,root).has_value(),"downloaded_textured_aircraft_imports");
	check(render.sceneInstanceCount()>=3 && render.sceneInstanceCount()<=6 && render.sceneMeshCount()>=3,"aircraft_meshes_and_instance_exist");
	island::Flight flight;
	check(island::updateAircraft(render,*parent,flight).has_value(),"initial_aircraft_pose");
	const auto roots=render.nodeChildren(*parent); check(roots && roots->size()==1U,"one_attached_aircraft_instance");
	const auto model=render.nodeTransform(roots->front()); check(model.has_value(),"model_placement_is_public");
	check(close(model->scale.value,{0.54F,0.54F,0.54F}),"aircraft_scaled_to_fourteen_metre_span");
	check(close(rotated(model->rotation.value,{0,0,1}),{0,0,-1}),"source_nose_reversed_to_flight_forward");
	const auto hit=render.raycast({.value={0,40,240}},{.value={0,-1,0}},100);
	check(hit.has_value(),"body_picked_without_gpu");
	const auto bounds=render.objectBounds(hit->object); check(bounds && bounds->valid,"aircraft_world_bounds_exist");
	check(std::abs(bounds->minimum.value.y-island::runwayHeight)<0.002F,"real_model_tyres_touch_pavement");
	check(bounds->maximum.value.x-bounds->minimum.value.x>13.8F && bounds->maximum.value.x-bounds->minimum.value.x<14.0F,
		"realistic_aircraft_wingspan");
	const auto props=propellers(render,*parent);
	const auto before=render.nodeTransform(props.front()); check(before.has_value(),"initial_propeller_pose");
	flight.position={50,80,-700}; flight.heading=0.61F; flight.pitch=0.15F; flight.bank=0.30F; flight.throttle=1; flight.elapsed=1;
	check(island::updateAircraft(render,*parent,flight).has_value(),"banked_airborne_pose");
	const auto pose=render.nodeTransform(*parent); check(pose.has_value(),"flight_parent_transform_exists");
	check(close(pose->translation.value,flight.position),"scene_tree_follows_flight_position");
	check(close(rotated(pose->rotation.value,{0,0,-1}),island::forward(flight)),"visible_nose_matches_physics_heading_and_pitch");
	const auto after=render.nodeTransform(props.front()); check(after.has_value(),"animated_propeller_pose");
	const auto a=before->rotation.value,b=after->rotation.value;
	check(std::abs(a.w*b.w+a.x*b.x+a.y*b.y+a.z*b.z)<0.99F,"propeller_rotates_with_elapsed_time");
	check(close(before->translation.value,after->translation.value) && close(before->scale.value,after->scale.value),"propeller_keeps_its_axle");
	check(island::updateAircraft(render,*parent,flight).has_value(),"repeat_same_clock");
	const auto repeated=render.nodeTransform(props.front()); check(repeated.has_value(),"repeated_propeller_pose");
	check(repeated->rotation.value.w==b.w && repeated->rotation.value.x==b.x,"same_clock_reproduces_propeller_pose");
	flight.heading=std::numbers::pi_v<float>/2; flight.pitch=0; flight.bank=0;
	check(island::updateAircraft(render,*parent,flight).has_value(),"quarter_turn_pose");
	const auto turned=render.nodeTransform(*parent); check(turned.has_value(),"quarter_turn_transform");
	check(close(rotated(turned->rotation.value,{0,0,-1}),{1,0,0}),"positive_heading_turns_nose_right");
	check(render.renderedFrameCount()==0,"aircraft_checks_are_entirely_cpu_only");
	std::println("[IslandDeliverySceneTests] tyre_height={} wingspan={} propeller_x={} checks={}",
		bounds->minimum.value.y,bounds->maximum.value.x-bounds->minimum.value.x,b.x,checks);
	return {*parent,hit->object};
}
/** @brief Travels between scattered sites while proving scenery ownership, residency bounds and stable wave geometry. */
void streaming(vve::RenderSystem &render,vve::AssetSystem &assets,const std::filesystem::path &root,
	vve::SceneNodeHandle parent,vve::RenderObjectHandle aircraftBody) {
	check(island::buildEnvironment(render,assets,root).has_value(),"static_sky_survives_aircraft_scene");
	std::map<std::size_t,vve::Vector<vve::RenderObjectHandle>> resident{};
	vve::Vector<vve::RenderObjectHandle> water{},originalWater{};
	std::optional<vve::Bounds> originBounds{}; // Re-entering an unloaded island must reproduce its original terrain.
	const auto aircraftBefore=render.objectBounds(aircraftBody); check(aircraftBefore.has_value(),"streaming_aircraft_source_bounds");
	const auto pivots=propellers(render,parent); const auto propBefore=render.nodeTransform(pivots.front());
	check(propBefore.has_value(),"streaming_preserves_original_propeller");
	std::size_t peakMeshes{},peakVertices{},released{};
	// Include random remote sites selected from the actual layout, rather than interpreting IDs as grid coordinates.
	std::array<vve::Vec3,7> sites{}; std::array<std::size_t,6> ids{0,1,17,128,240,255};
	float east=std::numeric_limits<float>::lowest(),north=std::numeric_limits<float>::lowest(),south=std::numeric_limits<float>::max();
	for (const auto id : std::views::iota(std::size_t{},island::islandCount)) {
		const auto at=island::islandCenter(id);
		if (at.x>east) { east=at.x; ids[4]=id; } if (at.z<south) { south=at.z; ids[5]=id; } north=std::max(north,at.z);
	}
	for (const auto index : std::views::iota(std::size_t{},ids.size())) { sites[index]=island::islandCenter(ids[index]); }
	sites.back()=vve::math::add(island::islandCenter(0),{1200,80,0});
	check(island::terrainHeight(sites.back().x,sites.back().z)==0,"nearby_channel_sample_is_ocean");
	for (const auto eye : sites) {
		const auto old=resident;
		check(island::updateEnvironment(render,root,eye,resident,water).has_value(),"streamed_site_builds");
		check(!resident.empty() && resident.size()<=9 && water.size()==45,"bounded_islands_and_wave_tiles");
		check(resident.contains(island::nearestIsland(eye.x,eye.z)),"nearest_island_is_resident");
		// Every resident batch must be placed at its own randomly scattered world site.
		for (const auto &[id,objects] : resident) {
			const auto at=island::islandCenter(id); const auto bounds=render.objectBounds(objects.front());
			check(bounds && bounds->minimum.value.x>=at.x-1000.1F && bounds->maximum.value.x<=at.x+1000.1F
				&& bounds->minimum.value.z>=at.z-1000.1F && bounds->maximum.value.z<=at.z+1000.1F,"resident_geometry_uses_scattered_centre");
		}
		if (originalWater.empty()) { originalWater=water; originBounds=render.objectBounds(resident.at(0).front()).value(); }
		check(water==originalWater,"travel_reuses_original_water_geometry");
		// Superseded detail and departed islands must release their actual meshes, rather than leave detached objects alive.
		for (const auto &[id,objects] : old) {
			if (resident.contains(id) && resident.at(id)==objects) { continue; }
			for (const auto object : objects) { check(!render.objectBounds(object),"departed_object_released"); ++released; }
		}
		peakMeshes=std::max(peakMeshes,render.sceneMeshCount()); peakVertices=std::max(peakVertices,render.sceneVertexCount());
		const auto batchLimit=resident.size()*18U+water.size()+5U;
		check(render.sceneMeshCount()<=batchLimit && render.sceneMaterialCount()<=batchLimit && render.sceneTextureCount()<=12,"bounded_streaming_resources");
		check(render.sceneVertexCount()<resident.size()*950000U+water.size()*5000U+20000U,"bounded_geometry_per_resident_island");
		const auto stable=resident; const auto waves=water; const auto meshCount=render.sceneMeshCount();
		check(island::updateEnvironment(render,root,eye,resident,water).has_value(),"repeat_stationary_streaming_update");
		check(resident==stable && water==waves && render.sceneMeshCount()==meshCount,"stationary_frames_keep_handles_and_geometry");
		const auto position=render.objectTransform(water.front()); check(position.has_value(),"streaming_water_transform");
		check(close(position->translation.value,{std::round(eye.x/512)*512-1024,-0.16F,std::round(eye.z/512)*512-2048}),"wave_grid_tracks_current_location");
		const auto left=render.objectBounds(water[0]),right=render.objectBounds(water[1]);
		check(left && right && std::abs(left->maximum.value.x-right->minimum.value.x)<0.01F,"adjacent_wave_tiles_share_world_edge");
		const auto aircraftAfter=render.objectBounds(aircraftBody);
		check(aircraftAfter && close(aircraftBefore->minimum.value,aircraftAfter->minimum.value),"streaming_does_not_move_or_remove_aircraft");
		std::println("[IslandDeliverySceneTests] nearest={} x={} z={} resident={} meshes={}",
			island::nearestIsland(eye.x,eye.z),eye.x,eye.z,resident.size(),render.sceneMeshCount());
	}
	const auto waveHandles=water;
	check(island::updateEnvironment(render,root,{east+8000,80,(north+south)*0.5F},resident,water).has_value(),"open_ocean_beyond_scattered_world");
	check(resident.empty() && water==waveHandles && render.sceneMeshCount()<=52,"all_departed_islands_release_resources");
	check(render.sceneTextureCount()<=3 && render.sceneDirectionalLightCount()==1,"unused_land_textures_released_sky_and_sun_preserved");
	check(island::updateEnvironment(render,root,{0,0,0},resident,water).has_value(),"return_to_unloaded_origin");
	const auto restored=render.objectBounds(resident.at(0).front());
	check(restored && originBounds && close(restored->minimum.value,originBounds->minimum.value)
		&& close(restored->maximum.value,originBounds->maximum.value),"reloaded_island_geometry_is_deterministic");
	const auto propAfter=render.nodeTransform(pivots.front());
	check(propAfter && propAfter->rotation.value.x==propBefore->rotation.value.x && propAfter->rotation.value.w==propBefore->rotation.value.w,
		"streaming_preserves_deterministic_animation_pose");
	check(render.renderedFrameCount()==0,"streaming_verification_uses_no_gpu_frames");
	std::println("[IslandDeliverySceneTests] islands={} sites={} peak_meshes={} peak_vertices={} released={} water_tiles={}",
		island::islandCount,sites.size(),peakMeshes,peakVertices,released,water.size());
}
/** @brief Exercises default manual play and automatic cargo collection without initializing a window or an engine. */
void cargoContinuation() {
	island::GameSystem game;
	check(!game.mapOpen && !game.openMapOnStart && !game.automatic,"default_manual_start_has_no_big_map");
	check(game.displayed.targetIsland<island::islandCount && game.displayed.targetIsland!=game.displayed.originIsland,"default_assignment_is_valid");
	island::Flight delivered; delivered.position=vve::math::add(island::islandCenter(17),{0,island::aircraftGroundHeight,-200});
	delivered.targetIsland=17; delivered.phase=island::Phase::delivered; delivered.deliveries=4;
	delivered.heading=0.37F; delivered.elapsed=137.25F; delivered.takeoffs=delivered.landings=4; delivered.stoppedSeconds=2;
	const auto history=[](const island::Flight &f) { return std::tuple{f.position.x,f.position.y,f.position.z,f.velocity.x,f.velocity.y,f.velocity.z,
		f.heading,f.pitch,f.bank,f.throttle,f.elapsed,f.maxAltitude,f.grounded,f.takeoffs,f.landings,f.touchdownSpeed,f.deliveries}; };
	const auto before=history(delivered); const auto next=island::suggestedTarget(delivered); auto flight=delivered;
	game.commandedThrottle=0.8F; game.accumulator=0.05F;
	check(game.collectNextCargo(flight),"manual_delivery_automatically_collects_next_cargo");
	check(!game.mapOpen && flight.phase==island::Phase::ready && flight.originIsland==17 && flight.targetIsland==next
		&& next<island::islandCount && next!=17,"next_assignment_needs_no_map_selection");
	check(history(flight)==before,"new_cargo_preserves_aircraft_pose_clock_and_delivery_history");
	check(game.commandedThrottle==0 && game.accumulator==0 && flight.stoppedSeconds==0 && flight.cargoTakeoffs==4,"new_cargo_resets_only_leg_controls");
	check(!game.collectNextCargo(flight) && flight.targetIsland==next && history(flight)==before,"new_assignment_is_not_repeated");
	island::GameSystem bounded; bounded.frameLimit=2; bounded.deliveryGoal=4; flight=delivered;
	check(!bounded.collectNextCargo(flight) && flight.phase==island::Phase::delivered && history(flight)==before,"bounded_run_stops_at_delivery_goal");
	bounded.deliveryGoal=5;
	check(bounded.collectNextCargo(flight) && flight.targetIsland==next && history(flight)==before,"bounded_run_collects_below_goal");
	game.mapOpen=game.openMapOnStart=true; flight=delivered;
	check(game.collectNextCargo(flight) && game.mapOpen && game.openMapOnStart,"cargo_collection_does_not_change_explicit_map_flags");
	std::println("[IslandDeliverySceneTests] automatic_collection_origin={} target={} completed={} map_default=false",flight.originIsland,flight.targetIsland,flight.deliveries);
}
/** @brief Samples service routes independently of imported art, including turns and several complete circuits. */
void airportRoutes() {
	float greatestStep{}; ///< A route may turn, but its placement must never teleport between frames.
	for (const auto id : {std::size_t{0},std::size_t{17},island::islandCount-1}) {
		const auto centre=island::islandCenter(id);
		for (unsigned actor{}; actor<island::airportActorCount; ++actor) {
			// Quarter-second samples span multiple circuits; short adjacent samples also catch wrap discontinuities.
			for (unsigned tick{}; tick<2400; ++tick) {
				const double seconds=static_cast<double>(tick)*0.25;
				const auto pose=island::airportPose(id,actor,seconds),next=island::airportPose(id,actor,seconds+0.01);
				const auto point=vve::math::subtract(pose.translation.value,centre); const auto q=pose.rotation.value;
				check(std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z)
					&& std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w),"airport_route_pose_is_finite");
				check(std::abs(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w-1)<0.002F,"airport_route_orientation_is_unit_quaternion");
				check(point.x>=27.99F && point.x<=67.01F && point.z>=-252 && point.z<=-127,"airport_routes_stay_on_service_apron");
				check(std::abs(point.y-island::runwayHeight-0.04F)<0.003F,"airport_routes_follow_apron_height");
				if (actor>=island::airportPeopleCount) {
					check(point.x>=29.99F && point.x<=40.01F && point.z<=-181.99F,"airport_vehicles_keep_to_service_lane");
				}
				const float step=vve::math::length(vve::math::subtract(next.translation.value,pose.translation.value));
				greatestStep=std::max(greatestStep,step); check(step<0.2F,"airport_routes_wrap_without_position_teleports");
				const auto forwardStep=vve::math::subtract(island::airportPose(id,actor,seconds+0.1).translation.value,pose.translation.value);
				const float forwardDistance=vve::math::length(forwardStep);
				if (forwardDistance>0.05F) { // A tenth of a second avoids world-coordinate quantization at remote islands.
					const auto direction=vve::math::scale(forwardStep,1.0F/forwardDistance);
					check(vve::math::dot(rotated(q,{0,0,1}),direction)>0.95F,"airport_actors_face_their_direction_of_travel");
				}
				const auto repeated=island::airportPose(id,actor,seconds);
				check(close(repeated.translation.value,pose.translation.value) && repeated.rotation.value.w==q.w
					&& repeated.rotation.value.y==q.y,"airport_pose_repeats_at_same_clock");
			}
		}
	}
	std::println("[IslandDeliverySceneTests] airport_route_islands=3 actors={} sampled_seconds=600 greatest_step_10ms={}",
		island::airportActorCount,greatestStep);
}
/** @brief Checks real animated Kenney instances, bounded model reuse and cleanup when scenery streams away. */
void airportTraffic(vve::RenderSystem &render,vve::AssetSystem &assets,const std::filesystem::path &root) {
	render.clearScene();
	std::map<std::size_t,vve::Vector<vve::RenderObjectHandle>> resident;
	std::map<std::size_t,vve::Vector<vve::RenderSceneInstanceHandle>> traffic;
	std::map<std::string,vve::SceneHandle> models;
	resident[0].resize(9); resident[17].resize(8); ///< Residency alone decides detail; terrain geometry is unnecessary here.
	check(island::updateAirportActivity(render,assets,root,resident,traffic,models,0).has_value(),"airport_imported_traffic_builds");
	check(traffic.size()==1 && traffic.contains(0) && traffic.at(0).size()==island::airportActorCount,"only_detailed_airports_have_activity");
	check(!models.empty() && models.size()<=6 && render.sceneTextureCount()>0,"airport_models_and_palette_are_loaded_once");
	const auto original=traffic; const auto cached=models; const auto meshCount=render.sceneMeshCount();
	std::vector<vve::SceneNodeHandle> roots;
	std::vector<vve::Transform> placements;
	// Query visible imported meshes rather than trusting the game's ownership map alone.
	for (const auto instance : traffic.at(0)) {
		const auto node=render.sceneInstanceRoot(instance); const auto objects=render.sceneInstanceObjects(instance);
		check(node.has_value() && objects && !objects->empty(),"airport_actor_has_real_scene_hierarchy_and_meshes");
		roots.push_back(*node);
		const auto placement=render.nodeTransform(*node); check(placement.has_value(),"airport_actor_root_placement_exists");
		placements.push_back(*placement);
		for (const auto object : *objects) {
			const auto bounds=render.objectBounds(object);
			check(bounds && bounds->valid && bounds->minimum.value.x>25,"airport_actor_geometry_clears_runway_and_unloading_bay");
		}
	}
	std::vector<std::pair<vve::SceneNodeHandle,vve::Transform>> limbs;
	auto pending=render.nodeChildren(roots.front()); check(pending.has_value(),"airport_person_has_poseable_children");
	// Preserve local joint poses separately from the actor's moving world placement.
	for (std::size_t next{}; next<pending->size(); ++next) {
		const auto node=(*pending)[next]; const auto pose=render.nodeTransform(node); const auto children=render.nodeChildren(node);
		check(pose.has_value() && children.has_value(),"airport_person_child_is_publicly_queryable");
		limbs.emplace_back(node,*pose); pending->insert(pending->end(),children->begin(),children->end());
	}
	check(!limbs.empty(),"airport_person_contains_rigid_body_parts");
	const auto initial=render.nodeTransform(roots.front()); check(initial.has_value(),"airport_initial_root_pose");
	check(island::updateAirportActivity(render,assets,root,resident,traffic,models,0.35).has_value(),"airport_advances_activity_clock");
	const auto moving=render.nodeTransform(roots.front());
	check(moving && !close(moving->translation.value,initial->translation.value,0.01F),"airport_person_walks_between_frames");
	for (unsigned actor=island::airportPeopleCount; actor<island::airportActorCount; ++actor) {
		const auto pose=render.nodeTransform(roots[actor]);
		check(pose && !close(pose->translation.value,placements[actor].translation.value,0.1F),"airport_service_vehicles_drive_between_frames");
	}
	bool walks=false;
	for (const auto &[node,before] : limbs) {
		const auto after=render.nodeTransform(node); check(after.has_value(),"airport_animated_part_survives_update");
		const auto a=before.rotation.value,b=after->rotation.value;
		walks=walks || std::abs(a.w*b.w+a.x*b.x+a.y*b.y+a.z*b.z)<0.9999F;
	}
	check(walks,"airport_people_swing_rigid_limbs_while_walking");
	check(traffic==original && models==cached && render.sceneMeshCount()==meshCount,"airport_motion_reuses_instances_and_models");
	std::vector<vve::Transform> movingLimbs;
	for (const auto &[node,before] : limbs) { movingLimbs.push_back(render.nodeTransform(node).value()); }
	check(island::updateAirportActivity(render,assets,root,resident,traffic,models,0.35).has_value(),"airport_pause_repeats_current_clock");
	const auto paused=render.nodeTransform(roots.front());
	check(paused && close(paused->translation.value,moving->translation.value) && paused->rotation.value.w==moving->rotation.value.w,
		"airport_pause_keeps_actor_placement");
	for (std::size_t i{}; i<limbs.size(); ++i) {
		const auto pose=render.nodeTransform(limbs[i].first); const auto q=movingLimbs[i].rotation.value;
		check(pose && pose->rotation.value.x==q.x && pose->rotation.value.y==q.y && pose->rotation.value.z==q.z
			&& pose->rotation.value.w==q.w,"airport_pause_keeps_all_limb_poses");
	}
	resident[0].resize(8); resident[17].resize(9);
	check(island::updateAirportActivity(render,assets,root,resident,traffic,models,12).has_value(),"airport_activity_moves_with_detailed_residency");
	check(traffic.size()==1 && traffic.contains(17) && traffic.at(17).size()==island::airportActorCount,"airport_downgrade_releases_old_activity");
	for (const auto instance : original.at(0)) { check(!render.sceneInstanceObjects(instance),"departed_airport_instances_are_invalid"); }
	for (const auto node : roots) { check(!render.nodeTransform(node),"departed_airport_hierarchies_are_removed"); }
	check(models==cached && render.sceneMeshCount()==meshCount,"remote_airport_reuses_cached_asset_geometry");
	const auto remote=traffic; resident.clear();
	check(island::updateAirportActivity(render,assets,root,resident,traffic,models,20).has_value(),"airport_activity_unloads_over_ocean");
	check(traffic.empty() && render.sceneInstanceCount()==0,"ocean_has_no_remaining_airport_objects");
	for (const auto instance : remote.at(17)) { check(!render.sceneInstanceRoot(instance),"last_airport_instance_root_is_released"); }
	check(render.renderedFrameCount()==0,"airport_activity_checks_need_no_gpu_frames");
	std::println("[IslandDeliverySceneTests] airport_people={} airport_vehicles={} cached_models={} meshes={} unloaded=true",
		island::airportPeopleCount,island::airportActorCount-island::airportPeopleCount,models.size(),meshCount);
}
/** @brief Measures the actual XZ overlap of a generated triangle and a rectangle, including boundary-crossing terrain cells. */
double groundOverlap(vve::Vec3 a,vve::Vec3 b,vve::Vec3 c,const std::array<float,4> &rectangle) {
	using Point=std::pair<double,double>;
	std::array<Point,8> polygon{{{a.x,a.z},{b.x,b.z},{c.x,c.z}}},clipped{}; std::size_t count{3};
	// Clip against each axis-aligned half plane; shared edges have zero area rather than duplicate surface coverage.
	for (unsigned edge{}; edge<4U && count; ++edge) {
		const auto coordinate=[edge](Point p) { return edge<2U?p.first:p.second; };
		const double boundary=rectangle[edge]; const bool lower=edge%2U==0;
		const auto inside=[&](Point p) { return lower?coordinate(p)>=boundary:coordinate(p)<=boundary; };
		std::size_t next{};
		for (std::size_t i{}; i<count; ++i) {
			const auto p=polygon[i],q=polygon[(i+1U)%count]; const bool pin=inside(p),qin=inside(q);
			if (pin) { clipped[next++]=p; }
			if (pin!=qin) { const double t=(boundary-coordinate(p))/(coordinate(q)-coordinate(p));
				clipped[next++]={p.first+t*(q.first-p.first),p.second+t*(q.second-p.second)}; }
		}
		polygon=clipped; count=next;
	}
	double area{};
	for (std::size_t i{}; i<count; ++i) { const auto p=polygon[i],q=polygon[(i+1U)%count]; area+=p.first*q.second-q.first*p.second; }
	return std::abs(area)*0.5;
}
/** @brief Counts generated ground surfaces at a point; diagonal-edge weights keep a quad's shared edge from counting twice. */
double groundCoverage(const vve::MeshDescriptor &mesh,float x,float z) {
	double coverage{};
	for (std::size_t i{}; i<mesh.indices.size(); i+=3U) {
		const auto a=mesh.positions[mesh.indices[i]],b=mesh.positions[mesh.indices[i+1U]],c=mesh.positions[mesh.indices[i+2U]];
		if (x<std::min({a.x,b.x,c.x}) || x>std::max({a.x,b.x,c.x}) || z<std::min({a.z,b.z,c.z}) || z>std::max({a.z,b.z,c.z})) { continue; }
		const double cross=(double(b.x)-a.x)*(double(c.z)-a.z)-(double(b.z)-a.z)*(double(c.x)-a.x);
		if (std::abs(cross)<1.0e-10) { continue; }
		const double u=((double(x)-a.x)*(double(c.z)-a.z)-(double(z)-a.z)*(double(c.x)-a.x))/cross;
		const double v=((double(b.x)-a.x)*(double(z)-a.z)-(double(b.z)-a.z)*(double(x)-a.x))/cross;
		if (u>=-1.0e-7 && v>=-1.0e-7 && u+v<=1.0000001) { coverage+=(std::min({u,v,1-u-v})<1.0e-7?0.5:1.0); }
	}
	return coverage;
}
/** @brief Proves both terrain LODs have a single collision-aligned paved surface without grass triangles crossing underneath it. */
void airportGround() {
	// These are the airport's visible runway, unloading area, apron, taxi connection and terminal forecourt footprints.
	constexpr std::array<std::array<float,4>,5> footprints{{{-18,18,-350,350},{-25,25,-300,-130},
		{25,77,-265,-135},{17,47,-240,-213},{42,72,-154,-130}}};
	for (const auto id : {std::size_t{0},std::size_t{17},island::islandCount-1}) {
		for (const bool detailed : {false,true}) {
			const auto centre=island::islandCenter(id); const auto meshes=island::groundMeshBatches(centre,detailed);
			const auto active=std::span{footprints}.first(detailed?5U:1U);
			const auto paved=[&](float x,float z) { return std::ranges::any_of(active,[&](const auto &r) {
				return x>r[0] && x<r[1] && z>r[2] && z<r[3]; }); };
			double pavedArea{}; std::size_t terrainTriangles{},samples{};
			// A pavement vertex is at the collision elevation, with a consistent four-metre repeat, regardless of material.
			for (const auto material : {4U,5U,8U,9U}) {
				const auto &mesh=meshes[material]; check(mesh.positions.size()==mesh.uvs.size(),"ground_pavement_has_uv_per_vertex");
				for (std::size_t i{}; i<mesh.positions.size(); ++i) {
					const auto p=mesh.positions[i]; const auto uv=mesh.uvs[i];
					check(std::abs(p.y-island::runwayHeight)<0.0001F,"ground_pavement_matches_collision_height");
					check(std::abs(uv.x-p.x*0.25F)<0.0001F && std::abs(uv.y-p.z*0.25F)<0.0001F,"ground_pavement_keeps_four_metre_texture_repeat");
				}
				for (std::size_t i{}; i<mesh.indices.size(); i+=3U) {
					const auto a=mesh.positions[mesh.indices[i]],b=mesh.positions[mesh.indices[i+1U]],c=mesh.positions[mesh.indices[i+2U]];
					const double area=std::abs((double(b.x)-a.x)*(double(c.z)-a.z)-(double(b.z)-a.z)*(double(c.x)-a.x))*0.5;
					check(area>0,"ground_pavement_has_no_degenerate_faces"); pavedArea+=area; double covered{};
					// Inclusion-exclusion checks the footprint union independently of the production floor tessellation.
					for (unsigned mask{1}; mask<(1U<<active.size()); ++mask) {
						std::array<float,4> r{-1000,1000,-1000,1000}; unsigned count{};
						for (unsigned j{}; j<active.size(); ++j) { if (mask&(1U<<j)) {
							++count; r[0]=std::max(r[0],active[j][0]); r[1]=std::min(r[1],active[j][1]);
							r[2]=std::max(r[2],active[j][2]); r[3]=std::min(r[3],active[j][3]); } }
						if (r[0]<r[1] && r[2]<r[3]) { covered+=(count%2U?1:-1)*groundOverlap(a,b,c,r); }
					}
					check(std::abs(covered-area)<0.001,"ground_pavement_does_not_extend_outside_airport");
				}
			}
			check(std::abs(pavedArea-(detailed?34490.0:25200.0))<0.001,"ground_paved_area_matches_airport_footprint");
			check(!meshes[8].indices.empty(),"ground_runway_white_paint_exists_at_both_lods");
			check(meshes[9].indices.empty()!=detailed,"ground_cargo_yellow_paint_matches_lod");
			// Painted surfaces replace their substrate, including the distant edge lines and close approach markings.
			std::vector<vve::Vec2> markings{{-17.031F,0.137F},{17.031F,0.137F}};
			if (detailed) { markings.insert(markings.end(),{{0.031F,-280.137F},{-13.031F,327.137F}}); }
			for (const auto p : markings) {
				const double substrate=groundCoverage(meshes[4],p.x,p.y)+groundCoverage(meshes[5],p.x,p.y);
				const double paint=groundCoverage(meshes[8],p.x,p.y)+groundCoverage(meshes[9],p.x,p.y);
				check(std::abs(substrate)<0.001 && std::abs(paint-1.0)<0.001,"ground_markings_replace_rather_than_overlap_substrate");
			}
			// The old ten/forty-metre cells crossed pavement edges even when selected corner heights were recessed.
			for (unsigned material{}; material<4U; ++material) { const auto &mesh=meshes[material];
				for (std::size_t i{}; i<mesh.indices.size(); i+=3U) {
					const auto a=mesh.positions[mesh.indices[i]],b=mesh.positions[mesh.indices[i+1U]],c=mesh.positions[mesh.indices[i+2U]];
					++terrainTriangles;
					for (const auto &r : active) {
						if (std::max({a.x,b.x,c.x})<=r[0] || std::min({a.x,b.x,c.x})>=r[1]
							|| std::max({a.z,b.z,c.z})<=r[2] || std::min({a.z,b.z,c.z})>=r[3]) { continue; }
						check(groundOverlap(a,b,c,r)<0.0001,"ground_terrain_has_no_area_beneath_pavement");
					}
				}
			}
			// Offset samples avoid grid corners while covering former overlapping runway/unloading and apron/forecourt regions.
			for (float x=-28.629F; x<81; x+=2.73F) { for (float z=-354.387F; z<354; z+=7.11F) {
				const double count=groundCoverage(meshes[4],x,z)+groundCoverage(meshes[5],x,z)
					+groundCoverage(meshes[8],x,z)+groundCoverage(meshes[9],x,z);
				check(std::abs(count-(paved(x,z)?1.0:0.0))<0.001,"ground_samples_have_exactly_one_pavement_surface"); ++samples;
			} }
			// Immediately outside every exposed paved boundary, ground must remain present instead of leaving a cut-out gap.
			for (const auto &r : active) { for (const float t : {0.113F,0.337F,0.619F,0.887F}) {
				const float x=r[0]+(r[1]-r[0])*t,z=r[2]+(r[3]-r[2])*t;
				for (const auto p : {vve::Vec2{r[0]-0.037F,z},{r[1]+0.037F,z},{x,r[2]-0.037F},{x,r[3]+0.037F}}) {
					if (paved(p.x,p.y)) { continue; } double count{};
					for (unsigned material{}; material<4U; ++material) { count+=groundCoverage(meshes[material],p.x,p.y); }
					check(std::abs(count-1.0)<0.001,"ground_exposed_pavement_edges_have_continuous_terrain");
				}
			} }
			std::println("[IslandDeliverySceneTests] ground_island={} detailed={} paved_area={} terrain_triangles={} coverage_samples={}",
				id,detailed,pavedArea,terrainTriangles,samples);
		}
	}
}

} // namespace

/** @brief Runs native C++ scene checks through the facade and emits a reproducible failure name. */
int main() {
	try {
		airportGround(); cargoContinuation();
		vve::Engine<> engine; auto world=engine.world();
		auto &render=world.get<vve::RenderSystem>(); auto &assets=world.get<vve::AssetSystem>();
		const std::filesystem::path root{VVE_ISLAND_ASSET_DIR};
		environment(render,assets,root); const auto [parent,body]=aircraft(render,assets,root);
		streaming(render,assets,root,parent,body);
		airportRoutes(); airportTraffic(render,assets,root);
		std::println("[IslandDeliverySceneTests] passed=true checks={}",checks); return 0;
	} catch (const std::exception &error) {
		std::cerr<<"[IslandDeliverySceneTests] passed=false checks="<<checks<<" error="<<error.what()<<'\n'; return 1;
	}
}
