import std;
import VEEngine.V4;

int main() {
   const vve::v4::RenderSystem render_system{};
   const auto renderer = render_system.createForwardRenderer();

   if (!renderer.handle.valid()) { return 1; }
   if (renderer.id.value != "forward") { return 2; }
   if (!renderer.shadow_maps) { return 3; }
   if (renderer.passes.size() != 2) { return 4; }
   if (renderer.passes[0].id != vve::v4::RenderMilestone::shadow_depth) { return 5; }
   if (renderer.passes[0].depends_on.size() != 1) { return 6; }
   if (renderer.passes[0].depends_on[0] != vve::v4::RenderMilestone::frame_begin) { return 7; }
   if (renderer.passes[1].id != vve::v4::RenderMilestone::scene_color) { return 8; }
   if (renderer.passes[1].depends_on.size() != 1) { return 9; }
   if (renderer.passes[1].depends_on[0] != vve::v4::RenderMilestone::shadow_depth) { return 10; }
   if (renderer.passes[1].shader_file != "Forward.slang") { return 11; }
   if (renderer.passes[1].fragment_entry != "vveForwardFragmentMain") { return 12; }
   if (!renderer.passes[0].writes_debug_data || !renderer.passes[1].writes_debug_data) { return 13; }

   const auto milestones = vve::v4::RenderMilestone{};
   std::size_t milestone_count{};
   bool found_shadow_depth{};
   bool found_present{};
   for (const auto milestone : milestones) {
      ++milestone_count;
      found_shadow_depth |= milestone == vve::v4::RenderMilestone::shadow_depth;
      found_present |= milestone == vve::v4::RenderMilestone::present;
   }
   if (milestone_count != vve::v4::RenderMilestone::all().size()) { return 35; }
   if (!found_shadow_depth || !found_present) { return 36; }

   const auto second = render_system.createForwardRenderer();
   if (!second.handle.valid() || second.handle == renderer.handle) { return 14; }

   const auto selected = render_system.createRenderer(vve::v4::RendererId{.value = "forward"});
   if (!selected || selected->id.value != "forward") { return 15; }
   if (!selected->handle.valid() || selected->handle == renderer.handle) { return 16; }
   if (selected->passes[0].outputs != "shadow depth texture") { return 17; }

   const auto unsupported = render_system.createRenderer(vve::v4::RendererId{.value = "deferred"});
   if (unsupported || unsupported.error() != vve::v4::Error::invalid_argument) { return 18; }

   const auto graph = render_system.buildRenderGraph(renderer);
   if (!graph || graph->passCount() != 3) { return 19; }

   const auto order = graph->topologicalOrder();
   if (!order || order->size() != 3) { return 20; }

   const auto first_pass = graph->passName((*order)[0]);
   const auto second_pass = graph->passName((*order)[1]);
   const auto third_pass = graph->passName((*order)[2]);
   if (!first_pass || first_pass->value != vve::v4::RenderMilestone::frame_begin) { return 21; }
   if (!second_pass || second_pass->value != vve::v4::RenderMilestone::shadow_depth) { return 22; }
   if (!third_pass || third_pass->value != vve::v4::RenderMilestone::scene_color) { return 23; }

   const vve::v4::GuiSystem gui_system{};
   const std::array system_lists{vve::v4::RenderPassList{.passes = renderer.passes},
                                 vve::v4::RenderPassList{.passes = gui_system.passes()}};
   const auto gui_graph = render_system.buildRenderGraph(system_lists);
   if (!gui_graph || gui_graph->passCount() != 4) { return 24; }

   const std::array deferred_gbuffer_deps{vve::v4::RenderMilestone::frame_begin};
   const std::array deferred_shadow_deps{vve::v4::RenderMilestone::frame_begin};
   const std::array deferred_lighting_deps{vve::v4::RenderMilestone::gbuffer,
                                           vve::v4::RenderMilestone::shadow_depth};
   const std::array deferred_scene_deps{vve::v4::RenderMilestone::deferred_lighting};
   const std::array deferred_passes{
       vve::v4::RenderPassContract{.id = vve::v4::RenderMilestone::gbuffer,
                                   .depends_on = deferred_gbuffer_deps},
       vve::v4::RenderPassContract{.id = vve::v4::RenderMilestone::shadow_depth,
                                   .depends_on = deferred_shadow_deps},
       vve::v4::RenderPassContract{.id = vve::v4::RenderMilestone::deferred_lighting,
                                   .depends_on = deferred_lighting_deps},
       vve::v4::RenderPassContract{.id = vve::v4::RenderMilestone::scene_color,
                                   .depends_on = deferred_scene_deps}};
   const auto deferred_graph = render_system.buildRenderGraph(std::array{vve::v4::RenderPassList{deferred_passes}});
   if (!deferred_graph || deferred_graph->passCount() != 5) { return 25; }

   const std::array forward_ray_shadow_deps{vve::v4::RenderMilestone::frame_begin};
   const std::array forward_ray_shadow_scene_deps{vve::v4::RenderMilestone::raytraced_shadow};
   const std::array forward_ray_shadow_passes{
       vve::v4::RenderPassContract{.id = vve::v4::RenderMilestone::raytraced_shadow,
                                   .depends_on = forward_ray_shadow_deps},
       vve::v4::RenderPassContract{.id = vve::v4::RenderMilestone::scene_color,
                                   .depends_on = forward_ray_shadow_scene_deps}};
   const auto forward_ray_shadow_graph =
       render_system.buildRenderGraph(std::array{vve::v4::RenderPassList{forward_ray_shadow_passes}});
   if (!forward_ray_shadow_graph || forward_ray_shadow_graph->passCount() != 3) { return 26; }

   const std::array ray_scene_deps{vve::v4::RenderMilestone::frame_begin};
   const std::array ray_scene_color_deps{vve::v4::RenderMilestone::raytraced_scene};
   const std::array ray_scene_passes{
       vve::v4::RenderPassContract{.id = vve::v4::RenderMilestone::raytraced_scene,
                                   .depends_on = ray_scene_deps},
       vve::v4::RenderPassContract{.id = vve::v4::RenderMilestone::scene_color,
                                   .depends_on = ray_scene_color_deps}};
   const auto ray_scene_graph = render_system.buildRenderGraph(std::array{vve::v4::RenderPassList{ray_scene_passes}});
   if (!ray_scene_graph || ray_scene_graph->passCount() != 3) { return 27; }

   const std::array bad_dependencies{std::string_view{"missing"}};
   const std::array bad_passes{vve::v4::RenderPassContract{.id = "bad", .depends_on = bad_dependencies}};
   const std::array bad_lists{vve::v4::RenderPassList{.passes = bad_passes}};
   const auto bad_graph = render_system.buildRenderGraph(bad_lists);
   if (bad_graph || bad_graph.error() != vve::v4::Error::missing_object) { return 28; }

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

   const auto mesh = scene.addMesh(std::move(vertices), std::move(indices),
                                   vve::v4::Bounds{.valid = true});
   const auto instance = scene.addInstance(mesh, material);
   if (!mesh.valid() || !material.valid() || !instance) { return 29; }
   if (scene.meshCount() != 1 || scene.materialCount() != 1 || scene.instanceCount() != 1) { return 30; }
   if (!scene.findMesh(mesh) || !scene.findMaterial(material) || !scene.findInstance(*instance)) { return 31; }

   scene.setCamera(vve::v4::RenderCamera{.target_extent = vve::v4::PixelExtent{.width = 640, .height = 480}});
   scene.setDirectionalLight(vve::v4::RenderDirectionalLight{});
   if (!scene.camera() || !scene.directionalLight()) { return 32; }
   if (scene.camera()->target_extent.width != 640) { return 33; }

   const auto missing = scene.addInstance(vve::v4::RenderMeshHandle{}, material);
   if (missing || missing.error() != vve::v4::Error::missing_object) { return 34; }

   return 0;
}
