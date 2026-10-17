module;
#include <imgui.h>

/** @file @brief Read-only cargo charts and regional flight guidance through the public GUI callback. */
export module Island.Map;
import std;
import Island.Logic;

namespace island {

/** @brief Draws an island's coast, runway and unloading bay in the chart's viewing orientation. */
void drawChartAirport(ImDrawList *draw, std::size_t index, ImVec2 at, float scale, bool target, float heading = 0.0F) {
   const int firstVertex = draw->VtxBuffer.Size; ///< This airport's geometry rotates together, including its threshold labels.
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
   // Rotate the whole airport around its projected centre so coastlines, pavement and labels stay aligned.
   if (heading != 0.0F) {
      const float sine = std::sin(heading), cosine = std::cos(heading);
      for (const auto index : std::views::iota(firstVertex, draw->VtxBuffer.Size)) {
         auto &position = draw->VtxBuffer[index].pos;
         const float x = position.x - at.x, y = position.y - at.y;
         position = ImVec2{at.x + x * cosine + y * sine, at.y - x * sine + y * cosine};
      }
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

/** @brief Shows a heading-up regional chart: the aircraft points up as islands and destination guidance rotate around it. */
void drawNavigation(const Flight &flight) {
   const auto &io = ImGui::GetIO(); ///< Inset follows the display, without touching engine camera state.
   const float uiScale = std::clamp(std::min(io.DisplaySize.x / 1920.0F, io.DisplaySize.y / 1080.0F), 0.58F, 1.0F);
   const auto destination = islandCenter(flight.targetIsland); ///< The committed cargo destination.
   const float dx = destination.x - flight.position.x, dz = destination.z - flight.position.z;
   const float distance = std::hypot(dx, dz); ///< Distance to the centre of the target airstrip.
   const float bearing = std::fmod(std::atan2(dx, -dz) * 180.0F / std::numbers::pi_v<float> + 360.0F, 360.0F);
   ImGui::SetNextWindowPos(ImVec2{io.DisplaySize.x - 12.0F, 12.0F}, ImGuiCond_Always, ImVec2{1, 0});
   ImGui::SetNextWindowSize(ImVec2{std::min(440.0F * std::max(0.8F,uiScale),io.DisplaySize.x*0.45F), 360.0F * uiScale});
   ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.025F, 0.055F, 0.09F, 0.96F});
   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{10.0F * uiScale, 8.0F * uiScale});
   constexpr auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
      | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;
   ImGui::Begin("Regional navigation", nullptr, flags);
   ImGui::SetWindowFontScale(std::max(0.75F, uiScale));
   const auto name = islandName(flight.targetIsland); ///< Stable airport name shown above the map.
   ImGui::TextColored(ImVec4{0.95F, 0.76F, 0.39F, 1}, "TO %s", name.c_str());
   ImGui::Text("%.1f km  |  Bearing %03.0f deg", distance / 1000.0F, bearing);
   const ImVec2 top = ImGui::GetCursorScreenPos(), space = ImGui::GetContentRegionAvail();
   const ImVec2 canvas{space.x, std::max(60.0F, space.y - ImGui::GetTextLineHeightWithSpacing())}; ///< Reserve a readable map legend.
   const ImVec2 bottom{top.x + canvas.x, top.y + canvas.y}, middle{top.x + canvas.x * 0.5F, top.y + canvas.y * 0.5F};
   const float range = std::clamp(distance * 2.4F, 2200.0F, 12000.0F); ///< Runway detail within a kilometre; regional context en route.
   const float scale = std::min(canvas.x, canvas.y) / range; ///< Isotropic projection preserves runway direction.
   const float focus = 0.35F * std::clamp((2200.0F - distance) / 1600.0F, 0.0F, 1.0F); ///< Include the nearby target's full strip.
   const float centreX = flight.position.x + dx * focus, centreZ = flight.position.z + dz * focus;
   const float sine = std::sin(flight.heading), cosine = std::cos(flight.heading); ///< Map axes follow the aircraft's nose.
   /** @brief Projects world metres into aircraft-relative right and forward directions. */
   const auto point = [&](float x, float z) {
      x -= centreX; z -= centreZ;
      return ImVec2{middle.x + (x * cosine + z * sine) * scale, middle.y + (-x * sine + z * cosine) * scale};
   };
   auto *draw = ImGui::GetWindowDrawList(); ///< The same public ImGui geometry path as the large map.
   constexpr auto gold = IM_COL32(244, 193, 98, 255); ///< Standalone direction cue.
   draw->AddRectFilled(top, bottom, IM_COL32(10, 34, 51, 255), 4.0F);
   draw->PushClipRect(top, bottom, true);
   // The region contains only actual neighbouring island sites; off-map coastlines are clipped.
   for (const auto index : std::views::iota(std::size_t{}, islandCount)) {
      const auto location = islandCenter(index);
      const ImVec2 at = point(location.x, location.z);
      const float coast = 1000.0F * scale; ///< Conservative bound for the largest physical island footprint.
      if (at.x < top.x - coast || at.x > bottom.x + coast || at.y < top.y - coast || at.y > bottom.y + coast) { continue; }
      drawChartAirport(draw, index, at, scale, index == flight.targetIsland, flight.heading);
   }
   const ImVec2 player = point(flight.position.x, flight.position.z); ///< Aircraft position is independent of the compass cue.
   drawChartAircraft(draw, player, 0.0F, 6.5F * std::max(uiScale, 0.8F));
   const float compass = std::max(uiScale, 0.8F), halfArrow = 14.0F * compass; ///< Fixed length, independent of target distance.
   const ImVec2 arrow{bottom.x - 25.0F * compass, top.y + 25.0F * compass}; ///< Clear corner placement.
   const float x = distance > 1.0F ? (dx * cosine + dz * sine) / distance : 0.0F;
   const float y = distance > 1.0F ? (-dx * sine + dz * cosine) / distance : -1.0F;
   // The short destination arrow uses the same heading-up orientation as the terrain and runways.
   draw->AddCircleFilled(arrow, 22.0F * compass, IM_COL32(8, 23, 35, 230), 24);
   drawChartArrow(draw, ImVec2{arrow.x - x * halfArrow, arrow.y - y * halfArrow},
                  ImVec2{arrow.x + x * halfArrow, arrow.y + y * halfArrow}, gold, 7.0F * compass);
   const ImVec2 north{top.x + 25.0F * compass, top.y + 25.0F * compass}; ///< North rotates independently of the fixed-up plane.
   constexpr auto northColour = IM_COL32(219, 230, 228, 255);
   draw->AddCircleFilled(north, 22.0F * compass, IM_COL32(8, 23, 35, 230), 24);
   drawChartArrow(draw, ImVec2{north.x + sine * 8.0F * compass, north.y + cosine * 8.0F * compass},
                  ImVec2{north.x - sine * 8.0F * compass, north.y - cosine * 8.0F * compass}, northColour, 4.0F * compass);
   const auto label = ImGui::CalcTextSize("N"); ///< Keep the cardinal label readable as its compass position rotates.
   draw->AddText(ImVec2{north.x - sine * 16.0F * compass - label.x * 0.5F,
                       north.y - cosine * 16.0F * compass - label.y * 0.5F}, northColour, "N");
   draw->PopClipRect();
   draw->AddRect(top, bottom, IM_COL32(66, 104, 117, 255), 4.0F);
   ImGui::Dummy(canvas);
   ImGui::TextUnformatted("Gold: target | Cyan: plane");
   ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor();
}

/** @brief Shows the already assigned cargo route; chart interaction changes only viewing state. */
void drawMap(const Flight &flight, bool &open, float &zoom, vve::Vec2 &pan) {
   if (!open) { return; }
   const auto &io = ImGui::GetIO(); ///< Current display and pointing-device snapshot.
   const float uiScale = std::clamp(std::min(io.DisplaySize.x / 1280.0F, io.DisplaySize.y / 720.0F), 0.85F, 1.3F); ///< Legible at small sizes.
   const auto origin = flight.originIsland, target = flight.targetIsland; ///< Authoritative assigned job, never changed by map input.
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
   ImGui::SetWindowFontScale(std::clamp(uiScale,0.75F,1.0F));
   ImGui::TextColored(ImVec4{0.95F, 0.76F, 0.39F, 1}, "I S L A N D   A I R  /  ASSIGNED DELIVERY");
   ImGui::Text("%zu islands  |  %zu deliveries completed", islandCount, static_cast<std::size_t>(flight.deliveries));
   ImGui::Separator();
   const ImVec2 top = ImGui::GetCursorScreenPos(); ///< Ocean chart's upper-left corner.
   const ImVec2 available = ImGui::GetContentRegionAvail(); ///< Space after the heading.
   const ImVec2 canvas{available.x, std::max(80.0F, available.y - 4.0F*ImGui::GetFrameHeightWithSpacing())}; ///< Leave room for larger cargo text and buttons.
   const ImVec2 bottom{top.x + canvas.x, top.y + canvas.y}; ///< Chart clip boundary.
   const ImVec2 centre{top.x + canvas.x * 0.5F, top.y + canvas.y * 0.5F}; ///< Chart's screen-space origin.
   vve::Vec2 low{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()}, high{-low.x, -low.y}; ///< Actual site bounds.
   // Fit the generated archipelago itself, including its coastlines, without assuming rows or columns.
   for (const auto index : std::views::iota(std::size_t{}, islandCount)) {
      const auto location = islandCenter(index);
      low.x = std::min(low.x, location.x); low.y = std::min(low.y, location.z);
      high.x = std::max(high.x, location.x); high.y = std::max(high.y, location.z);
   }
   const float baseScale = std::min(std::max(1.0F, canvas.x - 28.0F) / (high.x - low.x + 2100.0F),
                                    std::max(1.0F, canvas.y - 28.0F) / (high.y - low.y + 2100.0F)); ///< Fit every coastline.
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
      return ImVec2{centre.x + (x - (low.x + high.x) * 0.5F + pan.x) * scale,
                    centre.y + (z - (low.y + high.y) * 0.5F + pan.y) * scale};
   };
   auto *draw = ImGui::GetWindowDrawList(); ///< ImGui geometry uses the public GUI renderer.
   constexpr auto gold = IM_COL32(244, 193, 98, 255), cyan = IM_COL32(88, 231, 240, 255); ///< Route and aircraft colours.
   draw->AddRectFilled(top, bottom, IM_COL32(10, 34, 51, 255), 5.0F);
   draw->PushClipRect(top, bottom, true);
   std::optional<std::size_t> hoveredIsland; ///< At most one island owns a pointer interaction.
   float nearestPixels = std::max(7.0F, 680.0F * scale); ///< Readable hit area even when zoomed out.
   // Hovering reveals airport names while the assigned destination remains untouched.
   for (const auto index : std::views::iota(std::size_t{}, islandCount)) {
      const auto location = islandCenter(index);
      const ImVec2 at = point(location.x, location.z);
      const float distance = std::hypot(at.x - io.MousePos.x, at.y - io.MousePos.y);
      if (hovered && distance < nearestPixels) { nearestPixels = distance; hoveredIsland = index; }
   }
   const auto departure = islandCenter(origin), destination = islandCenter(target); ///< Assigned airport coordinates.
   const ImVec2 start = point(departure.x, departure.z), finish = point(destination.x, destination.z);
   // Bounded island silhouettes and tiny runway strokes keep all 256 airports visible at the overview scale.
   for (const auto index : std::views::iota(std::size_t{}, islandCount)) {
      const auto location = islandCenter(index);
      const ImVec2 at = point(location.x, location.z);
      const float radius = std::max(2.8F, 1000.0F * scale); ///< Bound the world-scaled coastline and highlight.
      if (at.x < top.x - radius || at.x > bottom.x + radius || at.y < top.y - radius || at.y > bottom.y + radius) { continue; }
      drawChartAirport(draw, index, at, scale, index == target);
      if (index == target || index == origin) { draw->AddCircle(at, radius + 4.0F, index == target ? gold : cyan, 16, 1.8F); }
      if (hoveredIsland == index) { draw->AddCircle(at, radius + 7.0F, IM_COL32(240, 244, 237, 220), 16); }
   }
   // The optional large chart shows the complete assigned route above the coastlines.
   drawChartArrow(draw, start, finish, gold, 9.0F * uiScale);
   const ImVec2 player = point(flight.position.x, flight.position.z); ///< Actual aircraft chart position.
   // The same aircraft marker preserves its meaning across regional and archipelago views.
   drawChartAircraft(draw, player, flight.heading, 8.0F * uiScale);
   draw->AddText(ImVec2{top.x + 12.0F, top.y + 10.0F}, IM_COL32(182, 208, 215, 255), "N ^  /  CYAN: AIRCRAFT  /  GOLD: ROUTE");
   draw->AddText(ImVec2{top.x + 12.0F, bottom.y - ImGui::GetTextLineHeight()-6.0F}, IM_COL32(151, 180, 190, 255), "Wheel: zoom | Right drag: pan | Cargo assigned automatically");
   draw->PopClipRect();
   draw->AddRect(top, bottom, IM_COL32(66, 104, 117, 255), 5.0F);
   // Names appear on demand instead of obscuring the ocean with hundreds of labels.
   if (hoveredIsland) {
      const auto name = islandName(*hoveredIsland);
      ImGui::BeginTooltip(); ImGui::TextUnformatted(name.c_str());
      ImGui::TextUnformatted(*hoveredIsland == target ? "Assigned destination" : *hoveredIsland == origin ? "Cargo origin" : "Island airstrip");
      ImGui::EndTooltip();
   }
   ImGui::Separator();
   const auto fromName = islandName(origin), toName = islandName(target);
   const float distance = std::hypot(destination.x - departure.x, destination.z - departure.z); ///< Direct route length in metres.
   ImGui::Text("%s  ->  %s  |  %.1f km", fromName.c_str(), toName.c_str(), distance / 1000.0F);
   ImGui::Text("Cargo: %s  |  %s", cargoName(flight).data(), phaseName(flight.phase).data());
   // Map controls affect only chart viewing; the delivery loop assigns every cargo job.
   if (ImGui::Button("Fit archipelago")) { zoom = 1.0F; pan = {}; }
   ImGui::SameLine();
   if (ImGui::Button("Return to game [M]")) { open = false; }
   ImGui::End();
   ImGui::PopStyleVar(); ImGui::PopStyleColor(3);
}

} // namespace island
