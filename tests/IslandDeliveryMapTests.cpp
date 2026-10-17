/** @file @brief CPU ImGui verification of assigned cargo charts and compact regional bearing guidance. */
#include <imgui.h>
import std;
import Island.Map;
import Island.Logic;

namespace {
int checks{}; ///< Successful named checks, retained in failure output for automated diagnosis.
constexpr ImU32 coast=IM_COL32(104,151,111,255),gold=IM_COL32(244,193,98,255); ///< Semantic chart geometry colours.
constexpr ImU32 runway=IM_COL32(236,235,216,255),runwayLabels=IM_COL32(235,240,223,255); ///< Physical runway and 18/36 labels.
constexpr ImU32 cyan=IM_COL32(88,231,240,255),north=IM_COL32(219,230,228,255),bay=IM_COL32(244,193,98,210); ///< Aircraft, north and unloading.
constexpr ImU32 pavement=IM_COL32(50,58,63,255); ///< Physical runway rectangle, independently of its white centreline.

/** @brief Fails with a stable contract name while allowing main to destroy the CPU context. */
void check(bool condition,std::string_view name) {
	if (!condition) { throw std::runtime_error{std::string{name}}; } ++checks;
}
/** @brief Captures public gameplay values so rendering cannot silently alter a cargo manifest or flight state. */
auto state(const island::Flight &f) {
	return std::tuple{f.position.x,f.position.y,f.position.z,f.velocity.x,f.velocity.y,f.velocity.z,f.heading,f.pitch,f.bank,
		f.throttle,f.elapsed,f.stoppedSeconds,f.maxAltitude,f.phase,f.grounded,f.takeoffs,f.landings,f.touchdownSpeed,
		f.originIsland,f.targetIsland,f.deliveries,f.cargoTakeoffs,f.navigationStage,island::cargoName(f)};
}
/** @brief Reads public solid-colour vertices; an optional limit isolates the first airport from its neighbours. */
std::vector<vve::Vec2> vertices(ImU32 colour,std::size_t limit=std::numeric_limits<std::size_t>::max()) {
	std::vector<vve::Vec2> result;
	// Semantic colours distinguish navigation geometry without depending on internal widget IDs.
	for (const auto *list : ImGui::GetDrawData()->CmdLists) {
		for (const auto &vertex : list->VtxBuffer) {
			if (vertex.col!=colour) { continue; }
			result.push_back({vertex.pos.x,vertex.pos.y}); if (result.size()==limit) { return result; }
		}
	}
	return result;
}
/** @brief Measures draw bounds independently of fonts; clipping is validated separately from uncut vertices. */
std::array<float,5> bounds(std::span<const vve::Vec2> points) {
	std::array<float,5> result{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),
		std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest(),static_cast<float>(points.size())};
	// Bounds support both an entire semantic layer and a single airport's physical geometry.
	for (const auto p : points) {
		result[0]=std::min(result[0],p.x); result[1]=std::min(result[1],p.y);
		result[2]=std::max(result[2],p.x); result[3]=std::max(result[3],p.y);
	}
	return result;
}
/** @brief Measures an entire semantic chart layer. */
std::array<float,5> geometry(ImU32 colour) { return bounds(vertices(colour)); }
/** @brief Finds the arrowhead's directional mass relative to its rectangle. */
vve::Vec2 arrowDirection() {
	const auto points=vertices(gold); const auto box=bounds(points); vve::Vec2 sum{};
	for (const auto p : points) { sum.x+=p.x; sum.y+=p.y; } // Extra head geometry biases the centre toward the tip.
	return {sum.x/static_cast<float>(points.size())-(box[0]+box[2])*0.5F,
		sum.y/static_cast<float>(points.size())-(box[1]+box[3])*0.5F};
}
/** @brief Checks that geometry rotates rigidly, independently of its screen-space centre. */
bool rotated(std::span<const vve::Vec2> original,std::span<const vve::Vec2> actual,float heading) {
	if (original.size()!=actual.size() || original.empty()) { return false; }
	const float cosine=std::cos(heading),sine=std::sin(heading);
	// Relative vertices remove the viewport's translation, leaving only the expected heading rotation.
	for (const auto i : std::views::iota(std::size_t{},original.size())) {
		const float x=original[i].x-original[0].x,y=original[i].y-original[0].y;
		if (std::abs(actual[i].x-actual[0].x-(x*cosine+y*sine))>1.1F
			|| std::abs(actual[i].y-actual[0].y-(-x*sine+y*cosine))>1.1F) { return false; }
	}
	return true;
}
/** @brief Compares actual positions with a pixel of tolerance for antialiasing and readable text placement. */
bool samePoints(std::span<const vve::Vec2> a,std::span<const vve::Vec2> b) {
	return std::ranges::equal(a,b,[](const auto x,const auto y) { return std::abs(x.x-y.x)<1.1F && std::abs(x.y-y.y)<1.1F; });
}
/** @brief Runs one deterministic ImGui frame and checks finite geometry and valid viewport scissor rectangles. */
int frame(ImVec2 size,const std::function<void()> &draw,ImVec2 mouse={-1000,-1000},bool pressed=false) {
	auto &io=ImGui::GetIO(); io.DisplaySize=size; io.DeltaTime=1.0F/60.0F;
	io.AddMousePosEvent(mouse.x,mouse.y); io.AddMouseButtonEvent(ImGuiMouseButton_Left,pressed);
	ImGui::NewFrame(); draw(); ImGui::EndFrame(); ImGui::Render();
	const auto *data=ImGui::GetDrawData(); check(data && data->Valid,"valid_cpu_draw_data");
	bool finite=true,clipped=true;
	// Off-chart vertices are legal, but every emitted scissor must remain inside the display.
	for (const auto *list : data->CmdLists) {
		for (const auto &vertex : list->VtxBuffer) {
			finite=finite && std::isfinite(vertex.pos.x) && std::isfinite(vertex.pos.y) && std::isfinite(vertex.uv.x)
				&& std::isfinite(vertex.uv.y) && std::abs(vertex.pos.x)<size.x*64 && std::abs(vertex.pos.y)<size.y*64;
		}
		for (const auto &command : list->CmdBuffer) {
			if (command.ElemCount==0) { continue; } const auto c=command.ClipRect;
			clipped=clipped && std::isfinite(c.x) && std::isfinite(c.y) && std::isfinite(c.z) && std::isfinite(c.w)
				&& c.x>=-1 && c.y>=-1 && c.z<=size.x+1 && c.w<=size.y+1 && c.x<=c.z && c.y<=c.w;
		}
	}
	check(finite,"finite_chart_vertices"); check(clipped,"chart_scissors_inside_viewport"); return data->TotalVtxCount;
}
/** @brief Verifies all scattered coastlines and proves that viewing and clicking never change assigned cargo. */
void chart(ImVec2 size) {
	island::Flight flight; flight.deliveries=3; const auto original=state(flight);
	bool open=false; float zoom=1; vve::Vec2 pan{}; ///< Only chart viewing state is mutable.
	const auto draw=[&] { island::drawMap(flight,open,zoom,pan); };
	check(frame(size,draw)==0,"closed_chart_emits_nothing");
	open=true; const int overviewVertices=frame(size,draw);
	check(overviewVertices>25000,"overview_has_complete_archipelago_geometry");
	check(geometry(coast)[4]>=32*island::islandCount,"overview_draws_all_256_coastlines");
	check(geometry(IM_COL32(35,65,79,180))[4]==0,"overview_has_no_matrix_grid_lines");
	check(state(flight)==original,"overview_preserves_cargo_and_flight");
	std::set<float> xs,zs; ///< A matrix would have only sixteen distinct coordinates on each axis.
	for (const auto index : std::views::iota(std::size_t{},island::islandCount)) {
		const auto at=island::islandCenter(index); xs.insert(at.x); zs.insert(at.z);
	}
	check(xs.size()>240 && zs.size()>240,"overview_islands_are_scattered_not_a_matrix");
	const auto land=geometry(coast); const ImVec2 click{(land[0]+land[2])*0.5F,(land[1]+land[3])*0.5F};
	// Ordinary chart clicks remain read-only in every cargo phase, including before departure and after unloading.
	for (const auto phase : {island::Phase::ready,island::Phase::flying,island::Phase::landed,island::Phase::delivered}) {
		flight.phase=phase; flight.grounded=phase!=island::Phase::flying; const auto assigned=state(flight);
		frame(size,draw,click); frame(size,draw,click,true); frame(size,draw,click,false);
		check(open && state(flight)==assigned,"chart_click_cannot_reassign_cargo_in_any_phase");
	}
	zoom=-5;
	frame(size,draw); check(zoom==1,"small_zoom_clamps");
	zoom=99; frame(size,draw); check(zoom==12,"large_zoom_clamps"); zoom=1;
	flight.heading=0; frame(size,draw); const auto northUp=vertices(coast,32);
	flight.heading=std::numbers::pi_v<float>/2; const auto assigned=state(flight); frame(size,draw);
	const auto marker=vertices(cyan); check(marker.size()>=3,"large_chart_has_aircraft_marker");
	const auto plane=std::span{marker}.last(3); ///< The plane follows the departure-ring vertices in this semantic layer.
	check(samePoints(northUp,vertices(coast,32)),"large_chart_stays_north_up_when_plane_turns");
	check(plane[0].x>plane[1].x && plane[0].x>plane[2].x,"large_chart_aircraft_marker_shows_eastward_heading");
	check(state(flight)==assigned,"large_chart_heading_display_preserves_flight");
	std::println("[IslandDeliveryMapTests] display={}x{} overview_vertices={} coast_vertices={}",size.x,size.y,overviewVertices,32*island::islandCount);
}
/** @brief Confirms a single short compass arrow rotates with bearing and approaches retain physical runway geometry. */
void navigation(ImVec2 size) {
	island::Flight flight; flight.targetIsland=255; flight.grounded=false; flight.phase=island::Phase::flying;
	const auto original=state(flight); const auto draw=[&] { island::drawNavigation(flight); };
	const int regional=frame(size,draw); check(regional>100,"regional_navigation_has_geometry");
	const auto arrow=geometry(gold);
	check(arrow[4]>0 && arrow[4]<32,"regional_has_one_arrow_without_target_ring");
	check(arrow[2]-arrow[0]<34 && arrow[3]-arrow[1]<34,"regional_has_no_long_route_segment");
	check(state(flight)==original,"navigation_preserves_gameplay_state");
	const auto destination=island::islandCenter(flight.targetIsland);
	// Moving around a fixed destination must rotate the compact cue while retaining exactly the same cardinal length.
	float arrowLength{};
	for (const auto offset : {vve::Vec2{0,5000},vve::Vec2{-5000,0},vve::Vec2{0,-5000},vve::Vec2{5000,0},vve::Vec2{0,10000}}) {
		flight.position={destination.x+offset.x,80,destination.z+offset.y}; frame(size,draw);
		const auto shape=geometry(gold); const auto direction=arrowDirection();
		const float length=std::max(shape[2]-shape[0],shape[3]-shape[1]);
		if (arrowLength==0) { arrowLength=length; }
		check(std::abs(length-arrowLength)<1.1F,"compass_arrow_length_does_not_depend_on_range"); // Allow the antialiasing pixel when rotated.
		check(direction.x*(-offset.x)+direction.y*(-offset.y)>1,"compass_arrow_points_toward_target");
		check(shape[4]==arrow[4],"regional_never_adds_target_ring_or_route_geometry");
	}
	// A changed job also updates bearing without requiring any navigation UI state to reset.
	flight.position={0,80,240}; flight.targetIsland=1; frame(size,draw); const auto first=arrowDirection();
	flight.targetIsland=255; frame(size,draw); const auto second=arrowDirection();
	check(first.y<0 && std::hypot(first.x-second.x,first.y-second.y)>0.1F,"new_assigned_target_updates_compass_direction");
	flight.position=vve::math::add(island::islandCenter(flight.targetIsland),{0,80,260});
	const int approach=frame(size,draw); const auto strip=bounds(vertices(runway,4));
	check(strip[3]-strip[1]>20 && strip[3]-strip[1]>2*(strip[2]-strip[0]),"approach_runway_direction_is_north_south_at_heading_zero");
	check(geometry(runwayLabels)[4]>0,"approach_shows_runway_18_36_threshold_labels");
	check(geometry(IM_COL32(244,193,98,210))[4]>=4,"approach_marks_actual_unloading_bay");
	check(geometry(IM_COL32(88,231,240,255))[4]>=3,"navigation_draws_aircraft_heading_marker");
	std::println("[IslandDeliveryMapTests] display={}x{} regional_vertices={} approach_vertices={}",size.x,size.y,regional,approach);
}
/** @brief Verifies that the regional chart rotates with the aircraft while its nose remains fixed upward. */
void headingUpNavigation(ImVec2 size) {
	island::Flight flight; flight.targetIsland=0; flight.position={0,80,0}; flight.grounded=false; flight.phase=island::Phase::flying;
	const auto draw=[&] { island::drawNavigation(flight); }; frame(size,draw);
	const auto shore=vertices(coast,32),asphalt=vertices(pavement,4),centreline=vertices(runway,4);
	const auto labels=vertices(runwayLabels,16),unloading=vertices(bay,4),aircraft=vertices(cyan);
	const auto arrowCount=vertices(gold).size(); const auto needle=vertices(north,arrowCount);
	check(shore.size()==32 && asphalt.size()==4 && centreline.size()==4 && labels.size()==16 && unloading.size()==4,"approach_geometry_layers_exist");
	check(aircraft.size()==3 && aircraft[0].y<aircraft[1].y && aircraft[0].y<aircraft[2].y,"regional_aircraft_nose_points_up");
	const float pi=std::numbers::pi_v<float>; std::vector<vve::Vec2> plusPiCoast,plusPiNorth;
	// One physical airport supplies stable geometry at every angle, including the diagonal approach and signed wrap boundary.
	for (const float heading : {pi/4,pi/2,pi,-pi/2,-pi,2*pi,-2*pi}) {
		flight.heading=heading; const auto original=state(flight); frame(size,draw);
		check(rotated(shore,vertices(coast,32),heading),"regional_coast_rotates_with_aircraft");
		check(rotated(asphalt,vertices(pavement,4),heading),"regional_runway_rectangle_rotates_with_aircraft");
		check(rotated(centreline,vertices(runway,4),heading),"regional_runway_centreline_rotates_with_aircraft");
		check(rotated(labels,vertices(runwayLabels,16),heading),"regional_runway_18_36_labels_rotate_with_aircraft");
		check(rotated(unloading,vertices(bay,4),heading),"regional_unloading_area_rotates_with_aircraft");
		check(samePoints(aircraft,vertices(cyan)),"regional_aircraft_stays_fixed_up_while_map_turns");
		check(rotated(needle,vertices(north,arrowCount),heading),"north_compass_needle_rotates_against_aircraft_heading");
		const auto strip=bounds(vertices(runway,4)); const float width=strip[2]-strip[0],height=strip[3]-strip[1];
		if (heading==pi/4) { check(std::abs(width-height)<2,"regional_runway_is_diagonal_at_45_degrees"); }
		if (heading==pi/2) { check(width>20 && width>2*height,"regional_runway_is_horizontal_at_90_degrees"); }
		if (heading==pi) { plusPiCoast=vertices(coast,32); plusPiNorth=vertices(north); }
		if (heading==-pi) { check(samePoints(plusPiCoast,vertices(coast,32)) && samePoints(plusPiNorth,vertices(north)),"regional_signed_pi_wrap_is_repeatable"); }
		check(state(flight)==original,"heading_up_navigation_never_mutates_flight");
	}
	// A target due world north appears up, left, down or right as the aircraft points north, east, south or west.
	flight.position={0,80,5000}; flight.heading=0; frame(size,draw); const auto baseline=bounds(vertices(gold));
	const auto northLabel=vertices(north); std::vector<vve::Vec2> plusPiArrow;
	for (const float heading : {0.0F,pi/2,pi,-pi/2,-pi}) {
		flight.heading=heading; const auto original=state(flight); frame(size,draw);
		const auto direction=arrowDirection(); const auto shape=geometry(gold);
		const float expectedX=-std::sin(heading),expectedY=-std::cos(heading);
		check(direction.x*expectedX+direction.y*expectedY>0.5F,"destination_arrow_is_relative_to_aircraft_nose");
		check(shape[4]==baseline[4] && shape[2]-shape[0]<34 && shape[3]-shape[1]<34,"heading_up_arrow_remains_short_without_target_ring");
		if (heading==pi) { plusPiArrow=vertices(gold); }
		if (heading==-pi) { check(samePoints(plusPiArrow,vertices(gold)),"destination_arrow_signed_pi_wrap_is_repeatable"); }
		if (heading==pi/2) { check(!samePoints(northLabel,vertices(north)),"north_marker_moves_as_aircraft_turns"); }
		check(state(flight)==original,"relative_bearing_render_preserves_flight");
	}
	std::println("[IslandDeliveryMapTests] display={}x{} heading_up=true rotations=7 cardinal_bearings=5",size.x,size.y);
}
} // namespace

/** @brief Uses the game's readable 24-pixel atlas at full and compact display sizes without SDL or Vulkan. */
int main() {
	try {
		ImGui::CreateContext(); auto &io=ImGui::GetIO(); io.IniFilename=nullptr; io.ConfigInputTrickleEventQueue=false;
		io.DisplaySize={1920,1080}; io.DisplayFramebufferScale={1,1}; io.DeltaTime=1.0F/60.0F; io.FontGlobalScale=1;
		ImFontConfig font; font.SizePixels=24; ///< Catch chart clipping with the real readable font, rather than ImGui's tiny default.
		io.FontDefault=io.Fonts->AddFontDefault(&font);
		unsigned char *pixels{}; int width{},height{}; io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
		check(pixels && width>0 && height>0 && io.FontDefault && io.FontDefault->FontSize==24,"readable_cpu_font_atlas_built");
		for (const auto size : {ImVec2{1920,1080},ImVec2{640,360}}) { chart(size); navigation(size); headingUpNavigation(size); }
		ImGui::DestroyContext(); std::println("[IslandDeliveryMapTests] passed=true checks={}",checks); return 0;
	} catch (const std::exception &error) {
		if (ImGui::GetCurrentContext()) { ImGui::DestroyContext(); }
		std::cerr<<"[IslandDeliveryMapTests] passed=false checks="<<checks<<" error="<<error.what()<<'\n'; return 1;
	}
}
