module;
#include <imgui.h>

/** @file @brief Interactive archipelago chart and airport cargo offers, drawn through the public GUI callback. */
export module Island.Map;
import std;
import Island.Logic;

namespace island {

/** @brief Draws one island's world-scaled coast, north/south runway and the target's actual unloading rectangle. */
void drawChartAirport(ImDrawList *draw, std::size_t index, ImVec2 at, float scale, bool target) {
   const float variation = index < 2 ? 1.0F : 0.85F + static_cast<float>((index * 37) % 31) * 0.01F; ///< Match physical terrain.
   const float minimum = std::max(1.0F, 2.5F / (650.0F * scale)); ///< Keep distant coastlines visible.
   std::array<ImVec2, 32> coast{}; ///< Enough samples for the terrain's five- and nine-wave coastline.
   // Use the same bounded coastal footprint as terrainHeight instead of inventing a different navigational shape.
   for (const auto corner : std::views::iota(std::size_t{}, coast.size())) {
      const float angle = static_cast<float>(corner) * 2.0F * std::numbers::pi_v<float> / static_cast<float>(coast.size());
      const float shape = variation * (1.0F + 0.07F * std::sin(5.0F * angle) + 0.04F * std::cos(9.0F * angle));
      coast[corner] = ImVec2{at.x + std::cos(angle) * 650.0F * shape * scale * minimum,
                            at.y + std::sin(angle) * 750.0F * shape * scale * minimum};
   }
   draw->AddConcavePolyFilled(coast.data(), static_cast<int>(coast.size()), IM_COL32(104, 151, 111, 255));
   draw->AddPolyline(coast.data(), static_cast<int>(coast.size()), IM_COL32(191, 192, 144, 240), ImDrawFlags_Closed, 1.0F);
   const float halfWidth = std::max(0.7F, runwayHalfWidth * scale), halfLength = std::max(2.0F, runwayHalfLength * scale);
   // All current physical airstrips are 700 by 36 metres, aligned with the world Z axis.
   draw->AddRectFilled(ImVec2{at.x - halfWidth, at.y - halfLength}, ImVec2{at.x + halfWidth, at.y + halfLength}, IM_COL32(50, 58, 63, 255));
   draw->AddLine(ImVec2{at.x, at.y - halfLength}, ImVec2{at.x, at.y + halfLength}, IM_COL32(236, 235, 216, 255));
   if (halfLength > 16.0F) {
      draw->AddText(ImVec2{at.x + halfWidth + 4.0F, at.y - halfLength - 5.0F}, IM_COL32(235, 240, 223, 255), "18");
      draw->AddText(ImVec2{at.x + halfWidth + 4.0F, at.y + halfLength - 7.0F}, IM_COL32(235, 240, 223, 255), "36");
   }
   // The yellow unloading area is exactly the destination's x +/-25, z -300..-130 metre ground zone.
   if (target && halfLength > 12.0F) {
      draw->AddRectFilled(ImVec2{at.x - 25.0F * scale, at.y - 300.0F * scale},
                          ImVec2{at.x + 25.0F * scale, at.y - 130.0F * scale}, IM_COL32(244, 193, 98, 210));
   }
}

/** @brief Draws a bearing or landing-direction arrow between two chart coordinates. */
void drawChartArrow(ImDrawList *draw, ImVec2 from, ImVec2 to, ImU32 colour, float size) {
   const float length = std::hypot(to.x - from.x, to.y - from.y); ///< Screen-space direction length.
   if (length < size * 1.5F) { return; }
   const float x = (to.x - from.x) / length, y = (to.y - from.y) / length; ///< Unit bearing.
   draw->AddLine(from, ImVec2{to.x - x * size, to.y - y * size}, colour, 2.0F);
   draw->AddTriangleFilled(to, ImVec2{to.x - x * size - y * size * 0.6F, to.y - y * size + x * size * 0.6F},
                          ImVec2{to.x - x * size + y * size * 0.6F, to.y - y * size - x * size * 0.6F}, colour);
}

/** @brief Draws the actual aircraft heading with a cyan marker above chart routes and terrain. */
void drawChartAircraft(ImDrawList *draw, ImVec2 at, float heading, float size) {
   const float sine = std::sin(heading), cosine = std::cos(heading); ///< North is zero, east is positive.
   draw->AddCircleFilled(at, size * 1.7F, IM_COL32(8, 23, 35, 230));
   draw->AddTriangleFilled(ImVec2{at.x + sine * size, at.y - cosine * size},
      ImVec2{at.x - sine * size * 0.7F - cosine * size * 0.6F, at.y + cosine * size * 0.7F - sine * size * 0.6F},
      ImVec2{at.x - sine * size * 0.7F + cosine * size * 0.6F, at.y + cosine * size * 0.7F + sine * size * 0.6F}, IM_COL32(88, 231, 240, 255));
}

} // namespace island

export namespace island {

/** @brief Shows nearby islands, target bearing and a runway-scale approach chart during normal flight. */
void drawNavigation(const Flight &flight) {
   const auto &io = ImGui::GetIO(); ///< Inset follows the display, without touching engine camera state.
   const float uiScale = std::clamp(std::min(io.DisplaySize.x / 1920.0F, io.DisplaySize.y / 1080.0F), 0.58F, 1.0F);
   const auto destination = islandCenter(flight.targetIsland); ///< The committed cargo destination.
   const float dx = destination.x - flight.position.x, dz = destination.z - flight.position.z;
   const float distance = std::hypot(dx, dz); ///< Distance to the centre of the target airstrip.
   const float bearing = std::fmod(std::atan2(dx, -dz) * 180.0F / std::numbers::pi_v<float> + 360.0F, 360.0F);
   ImGui::SetNextWindowPos(ImVec2{io.DisplaySize.x - 12.0F, 12.0F}, ImGuiCond_Always, ImVec2{1, 0});
   ImGui::SetNextWindowSize(ImVec2{360.0F * uiScale, 318.0F * uiScale});
   ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.025F, 0.055F, 0.09F, 0.96F});
   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{10.0F * uiScale, 8.0F * uiScale});
   constexpr auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
      | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;
   ImGui::Begin("Regional navigation", nullptr, flags);
   ImGui::SetWindowFontScale(std::max(0.8F, uiScale));
   const auto name = islandName(flight.targetIsland); ///< Stable airport name shown above the map.
   ImGui::TextColored(ImVec4{0.95F, 0.76F, 0.39F, 1}, "TO %s", name.c_str());
   ImGui::Text("%.1f km  |  Bearing %03.0f deg", distance / 1000.0F, bearing);
   const ImVec2 top = ImGui::GetCursorScreenPos(), space = ImGui::GetContentRegionAvail();
   const ImVec2 canvas{space.x, std::max(60.0F, space.y - 20.0F)}; ///< Reserve a short map legend.
   const ImVec2 bottom{top.x + canvas.x, top.y + canvas.y}, middle{top.x + canvas.x * 0.5F, top.y + canvas.y * 0.5F};
   const float range = std::clamp(distance * 2.4F, 2200.0F, 12000.0F); ///< Runway detail within a kilometre; regional context en route.
   const float scale = std::min(canvas.x, canvas.y) / range; ///< Isotropic projection preserves runway direction.
   const float focus = 0.35F * std::clamp((2200.0F - distance) / 1600.0F, 0.0F, 1.0F); ///< Include the nearby target's full strip.
   const float centreX = flight.position.x + dx * focus, centreZ = flight.position.z + dz * focus;
   /** @brief Projects world metres onto a north-up regional chart. */
   const auto point = [&](float x, float z) { return ImVec2{middle.x + (x - centreX) * scale, middle.y + (z - centreZ) * scale}; };
   auto *draw = ImGui::GetWindowDrawList(); ///< The same public ImGui geometry path as the large map.
   constexpr auto gold = IM_COL32(244, 193, 98, 255); ///< Bearing, destination and unloading highlight.
   draw->AddRectFilled(top, bottom, IM_COL32(10, 34, 51, 255), 4.0F);
   draw->PushClipRect(top, bottom, true);
   // The region contains only actual neighbouring island sites; off-map coastlines are clipped.
   for (const auto index : std::views::iota(std::size_t{}, islandCount)) {
      const auto location = islandCenter(index);
      const ImVec2 at = point(location.x, location.z);
      const float coast = 1000.0F * scale; ///< Conservative bound for the largest physical island footprint.
      if (at.x < top.x - coast || at.x > bottom.x + coast || at.y < top.y - coast || at.y > bottom.y + coast) { continue; }
      drawChartAirport(draw, index, at, scale, index == flight.targetIsland);
      if (index == flight.targetIsland) { draw->AddCircle(at, 11.0F, gold, 20, 1.5F); }
   }
   const ImVec2 player = point(flight.position.x, flight.position.z), target = point(destination.x, destination.z);
   // Clamp the bearing arrow at the inset boundary when the assigned island is outside the regional view.
   if (distance > 20.0F) {
      const float x = dx / distance, y = dz / distance; ///< North-up unit bearing toward the target.
      /** @brief Returns ray travel to the nearest chart edge along one coordinate. */
      const auto edge = [](float direction, float at, float low, float high) {
         return std::abs(direction) < 0.00001F ? std::numeric_limits<float>::max()
              : (direction > 0 ? high - at : at - low) / std::abs(direction);
      };
      const float reach = std::min({distance * scale, edge(x, player.x, top.x + 12, bottom.x - 12), edge(y, player.y, top.y + 12, bottom.y - 12)});
      drawChartArrow(draw, player, ImVec2{player.x + x * reach, player.y + y * reach}, gold, 7.0F);
   }
   // Runway 36 is the northbound approach; the 18 label marks its opposite threshold.
   if (distance < 1800.0F) {
      drawChartArrow(draw, point(destination.x - 130.0F, destination.z + 260.0F),
                     point(destination.x - 130.0F, destination.z - 260.0F), IM_COL32(245, 245, 221, 255), 5.0F);
      draw->AddText(ImVec2{target.x - 45.0F, target.y + 420.0F * scale}, IM_COL32(233, 238, 218, 255), "LAND NORTH");
   }
   drawChartAircraft(draw, player, flight.heading, 6.5F * std::max(uiScale, 0.8F));
   draw->AddText(ImVec2{top.x + 5.0F, top.y + 4.0F}, IM_COL32(219, 230, 228, 255), "N ^");
   draw->PopClipRect();
   draw->AddRect(top, bottom, IM_COL32(66, 104, 117, 255), 4.0F);
   ImGui::Dummy(canvas);
   ImGui::TextUnformatted("Gold: fly  |  Cyan: you  |  M: map");
   ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor();
}

/** @brief Draws the chart; returns a requested cargo destination for the caller to validate and apply to ECS state. */
std::optional<std::size_t> drawMap(const Flight &flight, std::size_t &selectedTarget, bool &open,
                                 float &zoom, vve::Vec2 &pan) {
   if (!open) { return std::nullopt; }
   const auto &io = ImGui::GetIO(); ///< Current display and pointing-device snapshot.
   const float uiScale = std::clamp(std::min(io.DisplaySize.x / 1280.0F, io.DisplaySize.y / 720.0F), 0.85F, 1.3F); ///< Legible at small sizes.
   const float speed = std::hypot(static_cast<float>(flight.velocity.x), static_cast<float>(flight.velocity.z)); ///< Ground roll in metres per second.
   const auto currentIsland = nearestIsland(flight.position.x, flight.position.z); ///< Airport offering the next job.
   const bool canLoad = flight.grounded && speed < 0.8F
                     && (flight.phase == Phase::ready || flight.phase == Phase::delivered);
   const auto origin = canLoad ? currentIsland : flight.originIsland; ///< Cargo collection point.
   // A committed delivery remains assigned throughout flight and any intermediate landing.
   selectedTarget = canLoad ? std::min(selectedTarget, islandCount - 1) : flight.targetIsland;
   const ImVec2 size{std::min(io.DisplaySize.x - 24.0F, 1560.0F), std::min(io.DisplaySize.y - 24.0F, 960.0F)}; ///< Chart panel extent.
   ImGui::SetNextWindowPos(ImVec2{io.DisplaySize.x * 0.5F, io.DisplaySize.y * 0.5F}, ImGuiCond_Always, ImVec2{0.5F, 0.5F});
   ImGui::SetNextWindowSize(size);
   ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.025F, 0.055F, 0.09F, 0.98F});
   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.13F, 0.26F, 0.32F, 1.0F});
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.19F, 0.39F, 0.44F, 1.0F});
   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{14.0F * uiScale, 10.0F * uiScale});
   constexpr auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
                        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;
   ImGui::Begin("Archipelago chart", nullptr, flags);
   ImGui::SetWindowFontScale(uiScale);
   ImGui::TextColored(ImVec4{0.95F, 0.76F, 0.39F, 1}, "I S L A N D   A I R  /  ROUTE PLANNER");
   ImGui::Text("%zu islands  |  %zu deliveries completed  |  %s", islandCount,
               static_cast<std::size_t>(flight.deliveries), canLoad ? "Choose a cargo destination" : "Assigned delivery");
   ImGui::Separator();
   const ImVec2 top = ImGui::GetCursorScreenPos(); ///< Ocean chart's upper-left corner.
   const ImVec2 available = ImGui::GetContentRegionAvail(); ///< Space after the heading.
   const ImVec2 canvas{available.x, std::max(80.0F, available.y - 84.0F * uiScale)}; ///< Leave room for cargo actions.
   const ImVec2 bottom{top.x + canvas.x, top.y + canvas.y}; ///< Chart clip boundary.
   const ImVec2 centre{top.x + canvas.x * 0.5F, top.y + canvas.y * 0.5F}; ///< Chart's screen-space origin.
   const float worldSpan = static_cast<float>(gridSide - 1) * islandSpacing; ///< Centre-to-centre archipelago width.
   const float baseScale = std::max(1.0F, std::min(canvas.x, canvas.y) - 28.0F) / (worldSpan + 1800.0F); ///< Fit every coastline.
   zoom = std::clamp(zoom, 1.0F, 12.0F);
   ImGui::InvisibleButton("Navigable island chart", canvas, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
   const bool hovered = ImGui::IsItemHovered(); ///< Chart interaction excludes surrounding buttons.
   // Preserve the world point under the cursor when zooming, and keep pan in world metres.
   if (hovered && io.MouseWheel != 0.0F) {
      const float oldScale = baseScale * zoom; ///< Before the cursor-anchored zoom change.
      zoom = std::clamp(zoom * std::pow(1.2F, io.MouseWheel), 1.0F, 12.0F);
      pan.x += (io.MousePos.x - centre.x) * (1.0F / (baseScale * zoom) - 1.0F / oldScale);
      pan.y += (io.MousePos.y - centre.y) * (1.0F / (baseScale * zoom) - 1.0F / oldScale);
   }
   const float scale = baseScale * zoom; ///< Screen pixels per world metre.
   if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
      pan.x += io.MouseDelta.x / scale; pan.y += io.MouseDelta.y / scale;
   }
   /** @brief Projects world X/Z onto a north-up chart without depending on implementation math types. */
   const auto point = [&](float x, float z) {
      return ImVec2{centre.x + (x - worldSpan * 0.5F + pan.x) * scale,
                    centre.y + (z + worldSpan * 0.5F + pan.y) * scale};
   };
   auto *draw = ImGui::GetWindowDrawList(); ///< ImGui geometry uses the public GUI renderer.
   constexpr auto gold = IM_COL32(244, 193, 98, 255), cyan = IM_COL32(88, 231, 240, 255); ///< Route and aircraft colours.
   draw->AddRectFilled(top, bottom, IM_COL32(10, 34, 51, 255), 5.0F);
   draw->PushClipRect(top, bottom, true);
   // Subtle chart grid conveys the archipelago's scale while keeping destination markers legible.
   for (const auto index : std::views::iota(std::size_t{}, gridSide)) {
      const float offset = static_cast<float>(index) * islandSpacing;
      const ImVec2 a = point(offset, 0), b = point(offset, -worldSpan);
      const ImVec2 c = point(0, -offset), d = point(worldSpan, -offset);
      draw->AddLine(a, b, IM_COL32(35, 65, 79, 180)); draw->AddLine(c, d, IM_COL32(35, 65, 79, 180));
   }
   std::optional<std::size_t> hoveredIsland; ///< At most one island owns a pointer interaction.
   float nearestPixels = std::max(7.0F, 680.0F * scale); ///< Readable hit area even when zoomed out.
   // Find the nearest marker before drawing so a click updates both route and highlight immediately.
   for (const auto index : std::views::iota(std::size_t{}, islandCount)) {
      const auto location = islandCenter(index);
      const ImVec2 at = point(location.x, location.z);
      const float distance = std::hypot(at.x - io.MousePos.x, at.y - io.MousePos.y);
      if (hovered && distance < nearestPixels) { nearestPixels = distance; hoveredIsland = index; }
   }
   if (canLoad && hoveredIsland && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { selectedTarget = *hoveredIsland; }
   const auto departure = islandCenter(origin), destination = islandCenter(selectedTarget); ///< Selected airport coordinates.
   const ImVec2 start = point(departure.x, departure.z), finish = point(destination.x, destination.z);
   // Bounded island silhouettes and tiny runway strokes keep all 256 airports visible at the overview scale.
   for (const auto index : std::views::iota(std::size_t{}, islandCount)) {
      const auto location = islandCenter(index);
      const ImVec2 at = point(location.x, location.z);
      const float radius = std::max(2.8F, 1000.0F * scale); ///< Bound the world-scaled coastline and highlight.
      if (at.x < top.x - radius || at.x > bottom.x + radius || at.y < top.y - radius || at.y > bottom.y + radius) { continue; }
      drawChartAirport(draw, index, at, scale, index == selectedTarget);
      if (index == selectedTarget || index == origin) { draw->AddCircle(at, radius + 4.0F, index == selectedTarget ? gold : cyan, 16, 1.8F); }
      if (hoveredIsland == index) { draw->AddCircle(at, radius + 7.0F, IM_COL32(240, 244, 237, 220), 16); }
   }
   // Keep the route arrow above the coastlines so its head stays visible at the selected destination.
   drawChartArrow(draw, start, finish, gold, 9.0F * uiScale);
   const ImVec2 player = point(flight.position.x, flight.position.z); ///< Actual aircraft chart position.
   // The same aircraft marker preserves its meaning across regional and archipelago views.
   drawChartAircraft(draw, player, flight.heading, 8.0F * uiScale);
   draw->AddText(ImVec2{top.x + 12.0F, top.y + 10.0F}, IM_COL32(182, 208, 215, 255), "N ^  /  CYAN: AIRCRAFT  /  GOLD: ROUTE");
   draw->AddText(ImVec2{top.x + 12.0F, bottom.y - 22.0F}, IM_COL32(151, 180, 190, 255),
      canLoad ? "Wheel: zoom  |  Right drag: pan  |  Click island: select cargo job" : "Wheel: zoom  |  Right drag: pan  |  Destination assigned until delivery");
   draw->PopClipRect();
   draw->AddRect(top, bottom, IM_COL32(66, 104, 117, 255), 5.0F);
   // Names appear on demand instead of obscuring the ocean with hundreds of labels.
   if (hoveredIsland) {
      const auto name = islandName(*hoveredIsland);
      ImGui::BeginTooltip(); ImGui::TextUnformatted(name.c_str());
      ImGui::TextUnformatted(*hoveredIsland == currentIsland ? "Current airport" : canLoad ? "Click to choose this cargo destination" : "Delivery already assigned");
      ImGui::EndTooltip();
   }
   ImGui::Separator();
   const auto fromName = islandName(origin), toName = islandName(selectedTarget);
   const float distance = std::hypot(destination.x - departure.x, destination.z - departure.z); ///< Direct route length in metres.
   Flight offered = flight; ///< Preview the manifest without changing the ECS flight component.
   offered.originIsland = origin; offered.targetIsland = selectedTarget;
   ImGui::Text("%s  ->  %s  |  %.1f km", fromName.c_str(), toName.c_str(), distance / 1000.0F);
   ImGui::Text("Cargo: %s  |  %s", cargoName(offered).data(), canLoad ? "Land, stop in unloading zone, then collect the next cargo" : phaseName(flight.phase).data());
   std::optional<std::size_t> requested; ///< Destination request consumed by the next game update.
   // Only the owning game system commits cargo changes; a chart cannot change an active delivery.
   if (canLoad) {
      ImGui::BeginDisabled(selectedTarget == currentIsland);
      if (ImGui::Button("Load cargo & depart")) { requested = selectedTarget; open = false; }
      ImGui::EndDisabled();
   } else if (ImGui::Button("Fly assigned route")) { open = false; }
   ImGui::SameLine();
   if (ImGui::Button("Fit archipelago")) { zoom = 1.0F; pan = {}; }
   ImGui::SameLine();
   if (ImGui::Button("Return to game [M]")) { open = false; }
   ImGui::End();
   ImGui::PopStyleVar(); ImGui::PopStyleColor(3);
   return requested;
}

} // namespace island
