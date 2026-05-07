import std;
import VEEngine.V4;

int main() {
   constexpr std::string_view forward_shadow_pass{"forward.shadow_map_pass"};
   constexpr std::string_view forward_color_pass{"forward.color_pass"};

   const vve::v4::RenderSystem render_system{};
   const auto renderer = render_system.createForwardRenderer();

   if (!renderer.handle.valid()) { return 1; }
   if (renderer.id.value != "forward") { return 2; }
   if (!renderer.shadow_maps) { return 3; }
   if (renderer.passes.size() != 6) { return 4; }
   if (renderer.passes[0].name != vve::v4::RenderMilestone::frame_begin) { return 5; }
   if (!renderer.passes[0].milestone) { return 6; }
   if (renderer.passes[1].name != forward_shadow_pass) { return 7; }
   if (renderer.passes[1].depends_on[0] != vve::v4::RenderMilestone::frame_begin) { return 8; }
   if (renderer.passes[2].name != vve::v4::RenderMilestone::shadow_depth) { return 9; }
   if (renderer.passes[2].depends_on[0] != forward_shadow_pass) { return 10; }
   if (!renderer.passes[2].milestone) { return 11; }
   if (renderer.passes[3].name != forward_color_pass) { return 12; }
   if (renderer.passes[3].depends_on[0] != vve::v4::RenderMilestone::shadow_depth) { return 13; }
   if (renderer.passes[3].shader_file != "Forward.slang") { return 14; }
   if (renderer.passes[3].fragment_entry != "vveForwardFragmentMain") { return 15; }
   if (renderer.passes[4].depends_on[0] != forward_color_pass) { return 16; }
   if (renderer.passes[5].name != vve::v4::RenderMilestone::frame_finished) { return 17; }
   if (!renderer.passes[1].writes_debug_data || !renderer.passes[3].writes_debug_data) { return 18; }

   const auto milestones = vve::v4::RenderMilestone{};
   std::size_t milestone_count{};
   bool found_shadow_depth{};
   bool found_frame_finished{};
   for (const auto milestone : milestones) {
      ++milestone_count;
      found_shadow_depth |= milestone == vve::v4::RenderMilestone::shadow_depth;
      found_frame_finished |= milestone == vve::v4::RenderMilestone::frame_finished;
   }
   if (milestone_count != vve::v4::RenderMilestone::all().size()) { return 19; }
   if (!found_shadow_depth || !found_frame_finished) { return 20; }

   const auto second = render_system.createForwardRenderer();
   if (!second.handle.valid() || second.handle == renderer.handle) { return 21; }

   const auto selected = render_system.createRenderer(vve::v4::RendererId{.value = "forward"});
   if (!selected || selected->id.value != "forward") { return 22; }
   if (!selected->handle.valid() || selected->handle == renderer.handle) { return 23; }
   if (selected->passes[1].outputs != "shadow depth texture") { return 24; }

   const auto unsupported = render_system.createRenderer(vve::v4::RendererId{.value = "deferred"});
   if (unsupported || unsupported.error() != vve::v4::Error::invalid_argument) { return 25; }

   const auto graph = render_system.buildRenderGraph(renderer);
   if (!graph || graph->passCount() != 6) { return 26; }

   const auto order = graph->topologicalOrder();
   if (!order || order->size() != 6) { return 27; }

   const std::array expected_order{vve::v4::RenderMilestone::frame_begin, forward_shadow_pass,
                                   vve::v4::RenderMilestone::shadow_depth, forward_color_pass,
                                   vve::v4::RenderMilestone::scene_color,
                                   vve::v4::RenderMilestone::frame_finished};
   for (std::size_t i{}; i < expected_order.size(); ++i) {
      const auto pass_name = graph->passName((*order)[i]);
      if (!pass_name || pass_name->value != expected_order[i]) { return 28; }
   }

   const auto color_handle = graph->passHandle(vve::v4::RenderMilestone::scene_color);
   if (!color_handle || !graph->contains(*color_handle)) { return 29; }

   const vve::v4::GuiSystem gui_system{};
   const std::array system_lists{renderer.passes, gui_system.passes()};
   const auto gui_graph = render_system.buildRenderGraph(system_lists);
   if (!gui_graph || gui_graph->passCount() != 8) { return 30; }

   const auto gui_order = gui_graph->topologicalOrder();
   if (!gui_order || gui_order->empty()) { return 31; }
   const auto last_gui_pass = gui_graph->passName(gui_order->back());
   if (!last_gui_pass || last_gui_pass->value != vve::v4::RenderMilestone::frame_finished) { return 32; }

   constexpr std::string_view deferred_gbuffer_pass{"deferred.gbuffer_pass"};
   constexpr std::string_view deferred_shadow_pass{"deferred.shadow_map_pass"};
   constexpr std::string_view deferred_lighting_pass{"deferred.lighting_pass"};
   constexpr std::string_view deferred_color_pass{"deferred.color_pass"};
   const std::array deferred_gbuffer_pass_deps{vve::v4::RenderMilestone::frame_begin};
   const std::array deferred_gbuffer_done_deps{deferred_gbuffer_pass};
   const std::array deferred_shadow_pass_deps{vve::v4::RenderMilestone::frame_begin};
   const std::array deferred_shadow_done_deps{deferred_shadow_pass};
   const std::array deferred_lighting_pass_deps{vve::v4::RenderMilestone::gbuffer,
                                                vve::v4::RenderMilestone::shadow_depth};
   const std::array deferred_lighting_done_deps{deferred_lighting_pass};
   const std::array deferred_color_pass_deps{vve::v4::RenderMilestone::deferred_lighting};
   const std::array deferred_color_done_deps{deferred_color_pass};
   const std::array deferred_finished_deps{vve::v4::RenderMilestone::scene_color};
   const std::array deferred_passes{
       vve::v4::RenderPassContract{.name = vve::v4::RenderMilestone::frame_begin, .milestone = true},
       vve::v4::RenderPassContract{.name = deferred_gbuffer_pass, .depends_on = deferred_gbuffer_pass_deps},
       vve::v4::RenderPassContract{.name = vve::v4::RenderMilestone::gbuffer,
                                   .depends_on = deferred_gbuffer_done_deps,
                                   .milestone = true},
       vve::v4::RenderPassContract{.name = deferred_shadow_pass, .depends_on = deferred_shadow_pass_deps},
       vve::v4::RenderPassContract{.name = vve::v4::RenderMilestone::shadow_depth,
                                   .depends_on = deferred_shadow_done_deps,
                                   .milestone = true},
       vve::v4::RenderPassContract{.name = deferred_lighting_pass,
                                   .depends_on = deferred_lighting_pass_deps},
       vve::v4::RenderPassContract{.name = vve::v4::RenderMilestone::deferred_lighting,
                                   .depends_on = deferred_lighting_done_deps,
                                   .milestone = true},
       vve::v4::RenderPassContract{.name = deferred_color_pass, .depends_on = deferred_color_pass_deps},
       vve::v4::RenderPassContract{.name = vve::v4::RenderMilestone::scene_color,
                                   .depends_on = deferred_color_done_deps,
                                   .milestone = true},
       vve::v4::RenderPassContract{.name = vve::v4::RenderMilestone::frame_finished,
                                   .depends_on = deferred_finished_deps,
                                   .milestone = true}};
   const auto deferred_graph = render_system.buildRenderGraph(deferred_passes);
   if (!deferred_graph || deferred_graph->passCount() != 10) { return 33; }

   constexpr std::string_view ray_shadow_pass{"forward.raytraced_shadow_pass"};
   const std::array ray_shadow_pass_deps{vve::v4::RenderMilestone::frame_begin};
   const std::array ray_shadow_done_deps{ray_shadow_pass};
   const std::array ray_shadow_color_deps{vve::v4::RenderMilestone::raytraced_shadow};
   const std::array ray_shadow_scene_deps{forward_color_pass};
   const std::array ray_shadow_finished_deps{vve::v4::RenderMilestone::scene_color};
   const std::array forward_ray_shadow_passes{
       vve::v4::RenderPassContract{.name = vve::v4::RenderMilestone::frame_begin, .milestone = true},
       vve::v4::RenderPassContract{.name = ray_shadow_pass, .depends_on = ray_shadow_pass_deps},
       vve::v4::RenderPassContract{.name = vve::v4::RenderMilestone::raytraced_shadow,
                                   .depends_on = ray_shadow_done_deps,
                                   .milestone = true},
       vve::v4::RenderPassContract{.name = forward_color_pass, .depends_on = ray_shadow_color_deps},
       vve::v4::RenderPassContract{.name = vve::v4::RenderMilestone::scene_color,
                                   .depends_on = ray_shadow_scene_deps,
                                   .milestone = true},
       vve::v4::RenderPassContract{.name = vve::v4::RenderMilestone::frame_finished,
                                   .depends_on = ray_shadow_finished_deps,
                                   .milestone = true}};
   const auto forward_ray_shadow_graph = render_system.buildRenderGraph(forward_ray_shadow_passes);
   if (!forward_ray_shadow_graph || forward_ray_shadow_graph->passCount() != 6) { return 34; }

   constexpr std::string_view ray_scene_pass{"raytracing.scene_pass"};
   const std::array ray_scene_pass_deps{vve::v4::RenderMilestone::frame_begin};
   const std::array ray_scene_done_deps{ray_scene_pass};
   const std::array ray_scene_color_deps{vve::v4::RenderMilestone::raytraced_scene};
   const std::array ray_scene_finished_deps{vve::v4::RenderMilestone::scene_color};
   const std::array ray_scene_passes{
       vve::v4::RenderPassContract{.name = vve::v4::RenderMilestone::frame_begin, .milestone = true},
       vve::v4::RenderPassContract{.name = ray_scene_pass, .depends_on = ray_scene_pass_deps},
       vve::v4::RenderPassContract{.name = vve::v4::RenderMilestone::raytraced_scene,
                                   .depends_on = ray_scene_done_deps,
                                   .milestone = true},
       vve::v4::RenderPassContract{.name = vve::v4::RenderMilestone::scene_color,
                                   .depends_on = ray_scene_color_deps,
                                   .milestone = true},
       vve::v4::RenderPassContract{.name = vve::v4::RenderMilestone::frame_finished,
                                   .depends_on = ray_scene_finished_deps,
                                   .milestone = true}};
   const auto ray_scene_graph = render_system.buildRenderGraph(ray_scene_passes);
   if (!ray_scene_graph || ray_scene_graph->passCount() != 5) { return 35; }

   const std::array bad_dependencies{std::string_view{"missing"}};
   const std::array bad_passes{vve::v4::RenderPassContract{.name = "bad", .depends_on = bad_dependencies}};
   const auto bad_graph = render_system.buildRenderGraph(bad_passes);
   if (bad_graph || bad_graph.error() != vve::v4::Error::missing_object) { return 36; }

   vve::v4::RenderScene scene{};
   const auto material = scene.addMaterial(vve::v4::RenderMaterial{
      .base_color = vve::v4::LinearColor{.value = vve::v4::Vec3(0.75F, 0.50F, 0.25F)},
      .base_color_texture_source = "albedo.png"});

   vve::v4::Vector<vve::v4::RenderVertex> vertices{};
   vertices.push_back(vve::v4::RenderVertex{.position = vve::v4::Vec3(0.0F, 0.0F, 0.0F)});
   vertices.push_back(vve::v4::RenderVertex{.position = vve::v4::Vec3(1.0F, 0.0F, 0.0F)});
   vertices.push_back(vve::v4::RenderVertex{.position = vve::v4::Vec3(0.0F, 1.0F, 0.0F)});

   vve::v4::Vector<std::uint32_t> indices{};
   indices.push_back(0);
   indices.push_back(1);
   indices.push_back(2);

   const auto mesh = scene.addMesh(std::move(vertices), std::move(indices), vve::v4::Bounds{.valid = true});
   const auto instance = scene.addInstance(mesh, material);
   if (!mesh.valid() || !material.valid() || !instance) { return 37; }
   if (scene.meshCount() != 1 || scene.materialCount() != 1 || scene.instanceCount() != 1) { return 38; }
   if (!scene.findMesh(mesh) || !scene.findMaterial(material) || !scene.findInstance(*instance)) { return 39; }

   scene.setCamera(vve::v4::RenderCamera{.target_extent = vve::v4::PixelExtent{.width = 640, .height = 480}});
   scene.setDirectionalLight(vve::v4::RenderDirectionalLight{});
   if (!scene.camera() || !scene.directionalLight()) { return 40; }
   if (scene.camera()->target_extent.width != 640) { return 41; }

   const auto missing = scene.addInstance(vve::v4::RenderMeshHandle{}, material);
   if (missing || missing.error() != vve::v4::Error::missing_object) { return 42; }

   return 0;
}
