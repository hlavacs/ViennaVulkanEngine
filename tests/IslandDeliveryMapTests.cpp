/** @file @brief CPU ImGui verification of the complete archipelago chart, cargo actions and regional approach guidance. */
#include <imgui.h>
import std;
import Island.Map;
import Island.Logic;

namespace {
int checks{}; ///< Successful named checks, retained in failure output for automated diagnosis.
constexpr ImU32 coast=IM_COL32(104,151,111,255),gold=IM_COL32(244,193,98,255); ///< Semantic chart geometry colours.
constexpr ImU32 runway=IM_COL32(245,245,221,255),cargoButton=IM_COL32(33,66,82,255); ///< Landing arrow and cargo controls.

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
/** @brief Verifies selection, all 256 physical coastlines, disabled retargeting and the real load-cargo button. */
void chart(ImVec2 size) {
	island::Flight flight; flight.deliveries=3; const auto original=state(flight);
	std::size_t selected=255; bool open=false; float zoom=1; vve::Vec2 pan{}; std::optional<std::size_t> request{};
	const auto draw=[&] { request=island::drawMap(flight,selected,open,zoom,pan); };
	check(frame(size,draw)==0 && !request,"closed_chart_emits_nothing");
	open=true; const int vertices=frame(size,draw);
	check(!request && selected==255 && vertices>25000,"overview_preserves_last_island_selection");
	check(geometry(coast)[4]>=32*island::islandCount,"overview_draws_all_256_coastlines");
	check(state(flight)==original,"overview_preserves_cargo_and_flight");
	const auto button=geometry(cargoButton); check(button[4]>0,"cargo_button_has_real_draw_geometry");
	const ImVec2 click{button[0]+10,(button[1]+button[3])*0.5F};
	// Derive the leftmost cargo button from its actual pixels, then deliver ordinary public mouse events.
	frame(size,draw,click); frame(size,draw,click,true); frame(size,draw,click,false);
	check(request && *request==255 && !open,"cargo_button_requests_selected_destination");
	check(state(flight)==original,"cargo_button_leaves_commit_to_game_system");
	open=true; selected=std::numeric_limits<std::size_t>::max(); zoom=-5;
	frame(size,draw); check(selected==255 && zoom==1 && !request,"invalid_selection_and_small_zoom_clamp");
	zoom=99; frame(size,draw); check(zoom==12,"large_zoom_clamps"); zoom=1;
	// Cargo remains assigned both in flight and after an intermediate landing.
	flight.originIsland=17; flight.targetIsland=255; flight.deliveries=7; flight.takeoffs=4; flight.landings=3;
	for (const bool grounded : {false,true}) {
		flight.grounded=grounded; flight.phase=grounded?island::Phase::landed:island::Phase::flying;
		flight.velocity=grounded?vve::Vec3{0,0,0}:vve::Vec3{12,0,-35}; const auto committed=state(flight); selected=0;
		frame(size,draw); check(!request && open && selected==flight.targetIsland,"active_cargo_cannot_be_retargeted");
		check(state(flight)==committed,"assigned_cargo_render_is_read_only");
	}
	flight.phase=island::Phase::delivered; selected=128; const auto delivered=state(flight);
	frame(size,draw); check(selected==128 && !request && state(flight)==delivered,"delivered_pilot_can_preview_next_cargo");
	std::println("[IslandDeliveryMapTests] display={}x{} overview_vertices={} coast_vertices={}",size.x,size.y,vertices,32*island::islandCount);
}
/** @brief Confirms an off-map target has a bearing arrow and an approach shows the physical north/south runway and unloading bay. */
void navigation(ImVec2 size) {
	island::Flight flight; flight.targetIsland=255; flight.grounded=false; flight.phase=island::Phase::flying;
	const auto original=state(flight); const auto draw=[&] { island::drawNavigation(flight); };
	const int regional=frame(size,draw); check(regional>100,"regional_navigation_has_geometry");
	check(geometry(gold)[4]>0,"distant_target_has_bearing_arrow");
	check(geometry(runway)[4]==0,"distant_target_has_no_false_landing_arrow");
	check(state(flight)==original,"navigation_preserves_gameplay_state");
	flight.position=vve::math::add(island::islandCenter(flight.targetIsland),{0,80,260});
	const int approach=frame(size,draw); const auto direction=geometry(runway);
	check(direction[4]>0 && direction[3]-direction[1]>(direction[2]-direction[0])*2,"approach_runway_direction_is_north_south");
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
