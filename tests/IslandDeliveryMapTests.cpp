/** @file @brief CPU ImGui verification of assigned cargo charts and compact regional bearing guidance. */
#include <imgui.h>
import std;
import Island.Map;
import Island.Logic;

namespace {
int checks{}; ///< Successful named checks, retained in failure output for automated diagnosis.
constexpr ImU32 coast=IM_COL32(104,151,111,255),gold=IM_COL32(244,193,98,255); ///< Semantic chart geometry colours.
constexpr ImU32 runway=IM_COL32(236,235,216,255),runwayLabels=IM_COL32(235,240,223,255); ///< Physical runway and 18/36 labels.

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
/** @brief Measures actual draw geometry by semantic colour, independently of font metrics or hard-coded screen coordinates. */
std::array<float,5> geometry(ImU32 colour) {
	std::array<float,5> result{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),
		std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest(),0};
	// Draw lists are public CPU data; clipping is validated separately from their uncut vertex positions.
	for (const auto *list : ImGui::GetDrawData()->CmdLists) {
		for (const auto &vertex : list->VtxBuffer) {
			if (vertex.col!=colour) { continue; }
			result[0]=std::min(result[0],vertex.pos.x); result[1]=std::min(result[1],vertex.pos.y);
			result[2]=std::max(result[2],vertex.pos.x); result[3]=std::max(result[3],vertex.pos.y); ++result[4];
		}
	}
	return result;
}
/** @brief Finds the directional mass of solid arrow vertices relative to their enclosing rectangle. */
vve::Vec2 arrowDirection() {
	const auto box=geometry(gold); vve::Vec2 sum{}; float count{};
	// The arrowhead contributes more geometry toward its tip; opposite compass bearings reverse this offset.
	for (const auto *list : ImGui::GetDrawData()->CmdLists) {
		for (const auto &vertex : list->VtxBuffer) {
			if (vertex.col==gold) { sum.x+=vertex.pos.x; sum.y+=vertex.pos.y; ++count; }
		}
	}
	return {sum.x/count-(box[0]+box[2])*0.5F,sum.y/count-(box[1]+box[3])*0.5F};
}
/** @brief Checks individual north/south runway strokes without merging neighbouring airports into one bounding box. */
bool verticalRunway() {
	std::map<int,std::array<float,4>> strips; ///< Screen-X buckets separate parallel physical runway strokes.
	// Several neighbouring airports may share a clipped draw list, so check each runway's orientation locally.
	for (const auto *list : ImGui::GetDrawData()->CmdLists) {
		for (const auto &vertex : list->VtxBuffer) {
			if (vertex.col!=runway) { continue; }
			const auto p=vertex.pos; const int key=static_cast<int>(std::round(p.x/4.0F));
			auto [it,inserted]=strips.try_emplace(key,std::array{p.x,p.y,p.x,p.y}); auto &box=it->second;
			box[0]=std::min(box[0],p.x); box[1]=std::min(box[1],p.y); box[2]=std::max(box[2],p.x); box[3]=std::max(box[3],p.y);
		}
	}
	return std::ranges::any_of(strips,[](const auto &entry) { const auto &b=entry.second; return b[3]-b[1]>20 && b[3]-b[1]>2*(b[2]-b[0]); });
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
	open=true; const int vertices=frame(size,draw);
	check(vertices>25000,"overview_has_complete_archipelago_geometry");
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
	std::println("[IslandDeliveryMapTests] display={}x{} overview_vertices={} coast_vertices={}",size.x,size.y,vertices,32*island::islandCount);
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
	const int approach=frame(size,draw);
	check(verticalRunway(),"approach_runway_direction_is_north_south");
	check(geometry(runwayLabels)[4]>0,"approach_shows_runway_18_36_threshold_labels");
	check(geometry(IM_COL32(244,193,98,210))[4]>=4,"approach_marks_actual_unloading_bay");
	check(geometry(IM_COL32(88,231,240,255))[4]>=3,"navigation_draws_aircraft_heading_marker");
	std::println("[IslandDeliveryMapTests] display={}x{} regional_vertices={} approach_vertices={}",size.x,size.y,regional,approach);
}
} // namespace

/** @brief Builds only an ImGui font atlas, then exercises both supported display sizes without SDL or Vulkan. */
int main() {
	try {
		ImGui::CreateContext(); auto &io=ImGui::GetIO(); io.IniFilename=nullptr; io.ConfigInputTrickleEventQueue=false;
		io.DisplaySize={1920,1080}; io.DisplayFramebufferScale={1,1}; io.DeltaTime=1.0F/60.0F; io.FontGlobalScale=1;
		unsigned char *pixels{}; int width{},height{}; io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
		check(pixels && width>0 && height>0,"cpu_font_atlas_built");
		for (const auto size : {ImVec2{1920,1080},ImVec2{640,360}}) { chart(size); navigation(size); }
		ImGui::DestroyContext(); std::println("[IslandDeliveryMapTests] passed=true checks={}",checks); return 0;
	} catch (const std::exception &error) {
		if (ImGui::GetCurrentContext()) { ImGui::DestroyContext(); }
		std::cerr<<"[IslandDeliveryMapTests] passed=false checks="<<checks<<" error="<<error.what()<<'\n'; return 1;
	}
}
