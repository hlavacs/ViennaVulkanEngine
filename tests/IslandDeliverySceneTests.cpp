/** @file @brief CPU-only facade checks for downloaded island art, grounded aircraft placement and propeller animation. */
import std;
import VVEngine;
import Island.Scene;

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
	check(render.sceneMeshCount()>=60 && render.sceneMeshCount()<=72,"bounded_fine_water_tiles_and_scenery_batches");
	check(render.sceneTextureCount()>=7 && render.sceneTextureCount()<=9,"downloaded_photo_materials_present");
	check(render.sceneVertexCount()>50000 && render.sceneIndexCount()>100000,"terrain_and_feather_palms_have_geometry");
	check(render.sceneInstanceCount()>=60 && render.sceneInstanceCount()<=75 && render.renderedFrameCount()==0,"environment_needs_no_gpu_frames");
	std::println("[IslandDeliverySceneTests] meshes={} materials={} textures={} vertices={} indices={} haze={}",
		render.sceneMeshCount(),render.sceneMaterialCount(),render.sceneTextureCount(),render.sceneVertexCount(),
		render.sceneIndexCount(),render.atmosphere().density);
}
/** @brief Verifies the real imported aircraft, including wheel contact, forward direction and deterministic propeller poses. */
void aircraft(vve::RenderSystem &render, vve::AssetSystem &assets, const std::filesystem::path &root) {
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
}
} // namespace

/** @brief Runs native C++ scene checks through the facade and emits a reproducible failure name. */
int main() {
	try {
		vve::Engine<> engine; auto world=engine.world();
		auto &render=world.get<vve::RenderSystem>(); auto &assets=world.get<vve::AssetSystem>();
		const std::filesystem::path root{VVE_ISLAND_ASSET_DIR};
		environment(render,assets,root); aircraft(render,assets,root);
		std::println("[IslandDeliverySceneTests] passed=true checks={}",checks); return 0;
	} catch (const std::exception &error) {
		std::cerr<<"[IslandDeliverySceneTests] passed=false checks="<<checks<<" error="<<error.what()<<'\n'; return 1;
	}
}
