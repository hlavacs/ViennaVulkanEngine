import std;

#if defined(VVE_ENGINE_IMPLEMENTATION_IS_V5)
import VEEngine.V5;
namespace ve = vve::v5;
#else
import VEEngine.V4;
namespace ve = vve::v4;
#endif

int main() {
   constexpr std::string_view forward_shadow_pass{"forward.shadow_map_pass"};
   constexpr std::string_view forward_color_pass{"forward.color_pass"};

   const ve::RenderSystem render_system{};
   const auto renderer = render_system.createForwardRenderer();

   if (!renderer.handle.valid()) { return 1; }
   if (renderer.id.value != "forward") { return 2; }
   if (!renderer.shadow_maps) { return 3; }
   if (renderer.passes.size() != 6) { return 4; }
   if (renderer.passes[0].name != ve::RenderMilestone::frame_begin()) { return 5; }
   if (!renderer.passes[0].milestone) { return 6; }
   if (renderer.passes[1].name != forward_shadow_pass) { return 7; }
   if (renderer.passes[1].depends_on[0] != ve::RenderMilestone::frame_begin()) { return 8; }
   if (renderer.passes[2].name != ve::RenderMilestone::shadow_depth()) { return 9; }
   if (renderer.passes[2].depends_on[0] != forward_shadow_pass) { return 10; }
   if (!renderer.passes[2].milestone) { return 11; }
   if (renderer.passes[3].name != forward_color_pass) { return 12; }
   if (renderer.passes[3].depends_on[0] != ve::RenderMilestone::shadow_depth()) { return 13; }
   if (renderer.passes[3].shader_file != "Forward.slang") { return 14; }
   if (renderer.passes[3].fragment_entry != "vveForwardFragmentMain") { return 15; }
   if (renderer.passes[4].depends_on[0] != forward_color_pass) { return 16; }
   if (renderer.passes[5].name != ve::RenderMilestone::frame_finished()) { return 17; }
   if (!renderer.passes[1].writes_debug_data || !renderer.passes[3].writes_debug_data) { return 18; }

   const auto milestones = ve::RenderMilestone{};
   std::size_t milestone_count{};
   bool found_shadow_depth{};
   bool found_frame_finished{};
   for (const auto milestone : milestones) {
      ++milestone_count;
      found_shadow_depth |= milestone == ve::RenderMilestone::shadow_depth();
      found_frame_finished |= milestone == ve::RenderMilestone::frame_finished();
   }
   if (milestone_count != ve::RenderMilestone::all().size()) { return 19; }
   if (!found_shadow_depth || !found_frame_finished) { return 20; }

   const auto second = render_system.createForwardRenderer();
   if (!second.handle.valid() || second.handle == renderer.handle) { return 21; }

   const auto selected = render_system.createRenderer(ve::RendererId{.value = "forward"});
   if (!selected || selected->id.value != "forward") { return 22; }
   if (!selected->handle.valid() || selected->handle == renderer.handle) { return 23; }
   if (selected->passes[1].outputs != "shadow depth texture") { return 24; }

   const auto unsupported = render_system.createRenderer(ve::RendererId{.value = "deferred"});
   if (unsupported || unsupported.error() != ve::Error::invalid_argument) { return 25; }

   const auto graph = render_system.buildRenderGraph(renderer);
   if (!graph || graph->nodeCount() != 6) { return 26; }

   const auto order = graph->topologicalOrder();
   if (!order || order->size() != 6) { return 27; }

   const std::array expected_order{ve::RenderMilestone::frame_begin(), forward_shadow_pass,
                                   ve::RenderMilestone::shadow_depth(), forward_color_pass,
                                   ve::RenderMilestone::scene_color(),
                                   ve::RenderMilestone::frame_finished()};
   for (std::size_t i{}; i < expected_order.size(); ++i) {
      const auto pass_name = graph->nodeName((*order)[i]);
      if (!pass_name || pass_name->value != expected_order[i]) { return 28; }
   }

   const auto color_handle = graph->nodeHandle(ve::RenderMilestone::scene_color());
   if (!color_handle || !graph->contains(*color_handle)) { return 29; }

   const ve::GuiSystem gui_system{};
   const std::array system_lists{renderer.passes, gui_system.passes()};
   const auto gui_graph = render_system.buildRenderGraph(system_lists);
   if (!gui_graph || gui_graph->nodeCount() != 8) { return 30; }

   const auto gui_order = gui_graph->topologicalOrder();
   if (!gui_order || gui_order->empty()) { return 31; }
   const auto last_gui_pass = gui_graph->nodeName(gui_order->back());
   if (!last_gui_pass || last_gui_pass->value != ve::RenderMilestone::frame_finished()) { return 32; }

   constexpr std::string_view deferred_gbuffer_pass{"deferred.gbuffer_pass"};
   constexpr std::string_view deferred_shadow_pass{"deferred.shadow_map_pass"};
   constexpr std::string_view deferred_lighting_pass{"deferred.lighting_pass"};
   constexpr std::string_view deferred_color_pass{"deferred.color_pass"};
   const std::array deferred_gbuffer_pass_deps{ve::RenderMilestone::frame_begin()};
   const std::array deferred_gbuffer_done_deps{deferred_gbuffer_pass};
   const std::array deferred_shadow_pass_deps{ve::RenderMilestone::frame_begin()};
   const std::array deferred_shadow_done_deps{deferred_shadow_pass};
   const std::array deferred_lighting_pass_deps{ve::RenderMilestone::gbuffer(),
                                                ve::RenderMilestone::shadow_depth()};
   const std::array deferred_lighting_done_deps{deferred_lighting_pass};
   const std::array deferred_color_pass_deps{ve::RenderMilestone::deferred_lighting()};
   const std::array deferred_color_done_deps{deferred_color_pass};
   const std::array deferred_finished_deps{ve::RenderMilestone::scene_color()};
   const std::array deferred_passes{
       ve::RenderPassContract{.name = ve::RenderMilestone::frame_begin(), .milestone = true},
       ve::RenderPassContract{.name = deferred_gbuffer_pass, .depends_on = deferred_gbuffer_pass_deps},
       ve::RenderPassContract{.name = ve::RenderMilestone::gbuffer(),
                                   .depends_on = deferred_gbuffer_done_deps,
                                   .milestone = true},
       ve::RenderPassContract{.name = deferred_shadow_pass, .depends_on = deferred_shadow_pass_deps},
       ve::RenderPassContract{.name = ve::RenderMilestone::shadow_depth(),
                                   .depends_on = deferred_shadow_done_deps,
                                   .milestone = true},
       ve::RenderPassContract{.name = deferred_lighting_pass,
                                   .depends_on = deferred_lighting_pass_deps},
       ve::RenderPassContract{.name = ve::RenderMilestone::deferred_lighting(),
                                   .depends_on = deferred_lighting_done_deps,
                                   .milestone = true},
       ve::RenderPassContract{.name = deferred_color_pass, .depends_on = deferred_color_pass_deps},
       ve::RenderPassContract{.name = ve::RenderMilestone::scene_color(),
                                   .depends_on = deferred_color_done_deps,
                                   .milestone = true},
       ve::RenderPassContract{.name = ve::RenderMilestone::frame_finished(),
                                   .depends_on = deferred_finished_deps,
                                   .milestone = true}};
   const auto deferred_graph = render_system.buildRenderGraph(deferred_passes);
   if (!deferred_graph || deferred_graph->nodeCount() != 10) { return 33; }

   constexpr std::string_view ray_shadow_pass{"forward.raytraced_shadow_pass"};
   const std::array ray_shadow_pass_deps{ve::RenderMilestone::frame_begin()};
   const std::array ray_shadow_done_deps{ray_shadow_pass};
   const std::array ray_shadow_color_deps{ve::RenderMilestone::raytraced_shadow()};
   const std::array ray_shadow_scene_deps{forward_color_pass};
   const std::array ray_shadow_finished_deps{ve::RenderMilestone::scene_color()};
   const std::array forward_ray_shadow_passes{
       ve::RenderPassContract{.name = ve::RenderMilestone::frame_begin(), .milestone = true},
       ve::RenderPassContract{.name = ray_shadow_pass, .depends_on = ray_shadow_pass_deps},
       ve::RenderPassContract{.name = ve::RenderMilestone::raytraced_shadow(),
                                   .depends_on = ray_shadow_done_deps,
                                   .milestone = true},
       ve::RenderPassContract{.name = forward_color_pass, .depends_on = ray_shadow_color_deps},
       ve::RenderPassContract{.name = ve::RenderMilestone::scene_color(),
                                   .depends_on = ray_shadow_scene_deps,
                                   .milestone = true},
       ve::RenderPassContract{.name = ve::RenderMilestone::frame_finished(),
                                   .depends_on = ray_shadow_finished_deps,
                                   .milestone = true}};
   const auto forward_ray_shadow_graph = render_system.buildRenderGraph(forward_ray_shadow_passes);
   if (!forward_ray_shadow_graph || forward_ray_shadow_graph->nodeCount() != 6) { return 34; }

   constexpr std::string_view ray_scene_pass{"raytracing.scene_pass"};
   const std::array ray_scene_pass_deps{ve::RenderMilestone::frame_begin()};
   const std::array ray_scene_done_deps{ray_scene_pass};
   const std::array ray_scene_color_deps{ve::RenderMilestone::raytraced_scene()};
   const std::array ray_scene_finished_deps{ve::RenderMilestone::scene_color()};
   const std::array ray_scene_passes{
       ve::RenderPassContract{.name = ve::RenderMilestone::frame_begin(), .milestone = true},
       ve::RenderPassContract{.name = ray_scene_pass, .depends_on = ray_scene_pass_deps},
       ve::RenderPassContract{.name = ve::RenderMilestone::raytraced_scene(),
                                   .depends_on = ray_scene_done_deps,
                                   .milestone = true},
       ve::RenderPassContract{.name = ve::RenderMilestone::scene_color(),
                                   .depends_on = ray_scene_color_deps,
                                   .milestone = true},
       ve::RenderPassContract{.name = ve::RenderMilestone::frame_finished(),
                                   .depends_on = ray_scene_finished_deps,
                                   .milestone = true}};
   const auto ray_scene_graph = render_system.buildRenderGraph(ray_scene_passes);
   if (!ray_scene_graph || ray_scene_graph->nodeCount() != 5) { return 35; }

   const std::array bad_dependencies{std::string_view{"missing"}};
   const std::array bad_passes{ve::RenderPassContract{.name = "bad", .depends_on = bad_dependencies}};
   const auto bad_graph = render_system.buildRenderGraph(bad_passes);
   if (bad_graph || bad_graph.error() != ve::Error::missing_object) { return 36; }

   ve::RenderScene scene{};
   const auto material = scene.addMaterial(ve::RenderMaterial{
      .base_color = ve::LinearColor{.value = ve::Vec3(0.75F, 0.50F, 0.25F)},
      .base_color_texture_source = "albedo.png"});

   ve::Vector<ve::RenderVertex> vertices{};
   vertices.push_back(ve::RenderVertex{.position = ve::Vec3(0.0F, 0.0F, 0.0F)});
   vertices.push_back(ve::RenderVertex{.position = ve::Vec3(1.0F, 0.0F, 0.0F)});
   vertices.push_back(ve::RenderVertex{.position = ve::Vec3(0.0F, 1.0F, 0.0F)});

   ve::Vector<std::uint32_t> indices{};
   indices.push_back(0);
   indices.push_back(1);
   indices.push_back(2);

   const auto mesh = scene.addMesh(std::move(vertices), std::move(indices), ve::Bounds{.valid = true});
   const auto instance = scene.addInstance(mesh, material);
   if (!mesh.valid() || !material.valid() || !instance) { return 37; }
   if (scene.meshCount() != 1 || scene.materialCount() != 1 || scene.instanceCount() != 1) { return 38; }
   if (!scene.findMesh(mesh) || !scene.findMaterial(material) || !scene.findInstance(*instance)) { return 39; }

   scene.setCamera(ve::RenderCamera{.target_extent = ve::PixelExtent{.width = 640, .height = 480}});
   scene.setDirectionalLight(ve::RenderDirectionalLight{});
   if (!scene.camera() || !scene.directionalLight()) { return 40; }
   if (scene.camera()->target_extent.width != 640) { return 41; }

   const auto missing = scene.addInstance(ve::RenderMeshHandle{}, material);
   if (missing || missing.error() != ve::Error::missing_object) { return 42; }

   ve::RenderScene debug_scene{};
   const auto plane_material = debug_scene.addMaterial();
   const auto cuboid_material = debug_scene.addMaterial();
   const auto plane_mesh = debug_scene.addPlaneMesh(ve::Vec2{3.0F, 3.0F});
   const auto cuboid_mesh = debug_scene.addCuboidMesh(ve::Vec3{-0.225F, 0.0F, -0.225F},
                                                      ve::Vec3{0.225F, 2.0F, 0.225F});
   const auto plane_instance = debug_scene.addInstance(plane_mesh, plane_material);
   const auto cuboid_instance = debug_scene.addInstance(cuboid_mesh, cuboid_material);
   if (!plane_instance || !cuboid_instance) { return 43; }
   if (debug_scene.meshCount() != 2 || debug_scene.materialCount() != 2 || debug_scene.instanceCount() != 2) {
      return 44;
   }

   const auto *plane_source = debug_scene.findMesh(plane_mesh);
   const auto *cuboid_source = debug_scene.findMesh(cuboid_mesh);
   if (plane_source == nullptr || cuboid_source == nullptr) { return 45; }
   if (plane_source->vertices.size() != 4 || plane_source->indices.size() != 6) { return 46; }
   if (cuboid_source->vertices.size() != 24 || cuboid_source->indices.size() != 36) { return 47; }
   if (!plane_source->bounds.valid || !cuboid_source->bounds.valid) { return 48; }
   if (cuboid_source->bounds.maximum.value.y != 2.0F) { return 49; }
   if (debug_scene.vertexCount() != 28 || debug_scene.indexCount() != 42) { return 52; }

   debug_scene.setCamera(ve::RenderCamera{.target_extent = ve::PixelExtent{.width = 960, .height = 540}});
   debug_scene.setDirectionalLight(ve::RenderDirectionalLight{});
   if (!debug_scene.camera() || !debug_scene.directionalLight()) { return 50; }
   if (debug_scene.camera()->target_extent.width != 960 || debug_scene.camera()->target_extent.height != 540) {
      return 51;
   }

   ve::RenderSystem stateful_render_system{};
   if (const auto result = stateful_render_system.addPlane(ve::Vec2{2.0F, 2.0F}, ve::LinearColor{});
       !result) {
      return 53;
   }
   stateful_render_system.setCamera(ve::Camera{}, ve::PixelExtent{.width = 320, .height = 200});
   stateful_render_system.setDirectionalLight(ve::Direction{}, ve::LinearColor{},
                                              ve::LightIntensity{}, ve::LinearColor{});
   if (stateful_render_system.sceneMeshCount() != 1 || stateful_render_system.sceneVertexCount() != 4) {
      return 54;
   }
   const auto frame = ve::WindowFrameData{.windows = ve::Vector<ve::WindowInfo>{ve::WindowInfo{}}};
   if (const auto result = stateful_render_system.renderFrame(frame); !result) { return 55; }
   if (stateful_render_system.renderedFrameCount() != 1 ||
       stateful_render_system.lastRenderedWindowCount() != 1) {
      return 56;
   }
   if (stateful_render_system.preparedGpuTargetCount() != 0) { return 57; }

   return 0;
}
