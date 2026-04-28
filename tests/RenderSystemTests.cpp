#include <algorithm>

import std;
import VEEngine;
import VEEngine.V3;

/**
 * @file
 * @brief Regression tests for renderer-selected render graph construction.
 */
namespace {

   /// @brief Returns the first render pass with a non-postprocessing kernel.
   [[nodiscard]] const vve::v3::RenderPassDesc *mainPass(const vve::v3::RenderGraph &graph) {
      const auto pass = std::ranges::find_if(graph.passes, [](const vve::v3::RenderPassDesc &candidate) {
         return candidate.kernel != vve::v3::RenderKernelId::post_process &&
                candidate.kernel != vve::v3::RenderKernelId::post_post_process &&
                candidate.kernel != vve::v3::RenderKernelId::imgui;
      });

      return pass == graph.passes.end() ? nullptr : std::addressof(*pass);
   }

   /// @brief Verifies that a renderer id produces the expected primary kernel.
   [[nodiscard]] bool graphUsesKernel(vve::v3::RenderSystem &render_system, vve::v3::GraphicsBackend &backend,
                                      std::string_view renderer_id, vve::v3::RenderKernelId expected_kernel) {
      const auto renderer = backend.createRenderer(renderer_id);
      if (!renderer) {
         return false;
      }

      const auto graph = render_system.buildStaticGraph(
          vve::v3::WindowHandle{.value = vve::Handle::fromHash(std::string(renderer_id))}, *renderer);
      const auto *pass = mainPass(graph);
      return pass != nullptr && pass->kernel == expected_kernel && pass->debug_name == renderer->display_name;
   }

   /// @brief Builds a minimal reflected layout suitable for renderer binding tests.
   [[nodiscard]] vve::v3::PipelineLayoutDesc testLayout(const vve::v3::RendererDesc &renderer,
                                                        vve::v3::ShaderHandle shader) {
      return vve::v3::PipelineLayoutDesc{
          .renderer = renderer.handle,
          .renderer_id = renderer.id,
          .shader_program = shader,
          .shader_stages = {vve::v3::PipelineShaderStageDesc{.stage = vve::v3::ShaderStage::vertex,
                                                             .entry_point = "vertexMain",
                                                             .spirv_word_count = 16},
                            vve::v3::PipelineShaderStageDesc{.stage = vve::v3::ShaderStage::fragment,
                                                             .entry_point = "fragmentMain",
                                                             .spirv_word_count = 16}},
          .descriptor_sets = {vve::v3::PipelineDescriptorSetLayoutDesc{
              .set = 0,
              .bindings = {vve::v3::PipelineDescriptorBindingDesc{
                  .set = 0,
                  .binding = 0,
                  .kind = vve::v3::DescriptorBindingKind::uniform_buffer,
                  .name = "frame",
                  .type_name = "FrameData",
                  .visible_stages = {vve::v3::ShaderStage::vertex, vve::v3::ShaderStage::fragment}}}}}};
   }

   /// @brief Builds fake backend resource metadata for renderer-side binding validation.
   [[nodiscard]] vve::v3::PipelineBackendResources testResources(const vve::v3::PipelineLayoutDesc &layout) {
      return vve::v3::PipelineBackendResources{
          .handle = vve::v3::PipelineResourceHandle{.value = vve::Handle::fromHash(layout.renderer_id + ".resources")},
          .renderer = layout.renderer,
          .shader_program = layout.shader_program,
          .shader_module_count = layout.shader_stages.size(),
          .descriptor_set_layout_count = layout.descriptor_sets.size(),
          .pipeline_layout_created = true};
   }

   /// @brief Assembles a complete window pipeline for binding tests.
   [[nodiscard]] vve::v3::WindowRenderPipeline testPipeline(vve::v3::RenderSystem &render_system,
                                                           const vve::v3::RendererDesc &renderer) {
      const auto shader = vve::v3::ShaderHandle{
          .value = vve::Handle::fromHash(std::string{"shader."} + renderer.id)};
      auto layout = testLayout(renderer, shader);
      const auto window = vve::v3::WindowHandle{
          .value = vve::Handle::fromHash(std::string{"window."} + renderer.id)};

      return vve::v3::WindowRenderPipeline{
          .window = window,
          .window_id = std::string{"window."} + renderer.id,
          .renderer = renderer,
          .shader_program = shader,
          .pipeline_layout = layout,
          .backend_resources = testResources(layout),
          .swapchain = vve::v3::WindowSwapchainResources{
              .color_attachment_format = vve::v3::GraphicsColorFormat::rgba8_unorm,
              .depth_attachment_format = vve::v3::GraphicsDepthFormat::depth32_float},
          .graph = render_system.buildStaticGraph(window, renderer)};
   }

   /// @brief Builds a minimal uploaded scene suitable for draw-packet generation tests.
   [[nodiscard]] vve::v3::SceneData testUploadedScene() {
      const auto mesh_handle = vve::v3::MeshHandle{
          .value = vve::Handle::fromHash(std::string_view{"tests.render.mesh"})};
      const auto material_handle = vve::v3::MaterialHandle{
          .value = vve::Handle::fromHash(std::string_view{"tests.render.material"})};
      const auto texture_handle = vve::v3::TextureHandle{
          .value = vve::Handle::fromHash(std::string_view{"tests.render.texture"})};
      const auto node_handle = vve::v3::SceneNodeHandle{
          .value = vve::Handle::fromHash(std::string_view{"tests.render.node"})};
      const auto mesh_instance_handle = vve::Handle::fromHash(std::string_view{"tests.render.mesh_instance"});

      vve::v3::SceneData scene{};
      scene.active_camera.position = vve::math::Vec3(0.0F, 0.0F, 10.0F);
      scene.active_camera.view_transform =
          vve::math::translate(vve::math::identityMat4(), vve::math::Vec3(0.0F, 0.0F, -10.0F));
      scene.active_camera.vertical_fov_radians = 0.9F;
      scene.active_camera.near_plane = 0.25F;
      scene.active_camera.far_plane = 500.0F;

      vve::v3::ImportedMesh mesh{};
      mesh.handle = mesh_handle;
      mesh.submeshes.push_back(vve::v3::ImportedSubmesh{
          .index_offset = 6,
          .index_count = 12,
          .material = material_handle});
      scene.mesh_indices.emplace(mesh_handle.value.value(), scene.meshes.size());
      scene.meshes.push_back(std::move(mesh));

      vve::v3::ImportedMaterial material{};
      material.handle = material_handle;
      material.base_color_factor = vve::math::Vec4(0.25F, 0.5F, 0.75F, 0.9F);
      material.emissive_factor = vve::math::Vec3(0.1F, 0.2F, 0.3F);
      material.roughness_factor = 0.65F;
      material.metallic_factor = 0.35F;
      material.normal_scale = 1.75F;
      material.alpha_cutoff = 0.4F;
      material.double_sided = true;
      material.alpha_blend = true;
      scene.material_indices.emplace(material_handle.value.value(), scene.materials.size());
      scene.materials.push_back(std::move(material));

      vve::v3::SceneNodeDesc node{};
      node.handle = node_handle;
      node.world_transform = vve::math::translate(vve::math::identityMat4(), vve::math::Vec3(1.0F, 2.0F, 3.0F));
      scene.node_indices.emplace(node_handle.value.value(), scene.nodes.size());
      scene.nodes.push_back(std::move(node));

      scene.mesh_instance_indices.emplace(mesh_instance_handle.value(), scene.mesh_instances.size());
      scene.mesh_instances.push_back(vve::v3::SceneMeshInstanceDesc{
          .handle = mesh_instance_handle,
          .node = node_handle,
          .mesh = mesh_handle});

      scene.lights.push_back(vve::v3::ImportedLight{
          .handle = vve::v3::LightHandle{.value = vve::Handle::fromHash(std::string_view{"tests.render.light"})},
          .node = node_handle,
          .name = "Key",
          .type = vve::v3::SceneLightType::directional,
          .local_direction = vve::math::Vec3(0.0F, -1.0F, 0.0F),
          .color = vve::math::Vec3(0.5F, 0.6F, 0.7F),
          .intensity = 2.0F});

      vve::v3::GpuMeshResources gpu_mesh{};
      gpu_mesh.mesh = mesh_handle;
      gpu_mesh.vertex_buffer =
          vve::v3::GpuBufferHandle{.value = vve::Handle::fromHash(std::string_view{"tests.render.vertex_buffer"})};
      gpu_mesh.index_buffer =
          vve::v3::GpuBufferHandle{.value = vve::Handle::fromHash(std::string_view{"tests.render.index_buffer"})};
      gpu_mesh.index_count = 24;
      gpu_mesh.resident = true;
      scene.gpu_mesh_indices.emplace(mesh_handle.value.value(), scene.gpu_meshes.size());
      scene.gpu_meshes.push_back(std::move(gpu_mesh));

      vve::v3::GpuMaterialResources gpu_material{};
      gpu_material.material = material_handle;
      gpu_material.constants_buffer = vve::v3::GpuBufferHandle{
          .value = vve::Handle::fromHash(std::string_view{"tests.render.material_constants"})};
      gpu_material.textures.push_back(vve::v3::GpuMaterialTextureBinding{
          .texture = texture_handle,
          .image = vve::v3::GpuImageHandle{.value = vve::Handle::fromHash(std::string_view{"tests.render.image"})},
          .sampler = vve::v3::GpuSamplerHandle{.value = vve::Handle::fromHash(std::string_view{"tests.render.sampler"})},
          .semantic = vve::v3::TextureSemantic::base_color,
          .binding = 0,
          .uv_set = 0});
      gpu_material.generation = 7;
      gpu_material.constants_uploaded = true;
      gpu_material.textures_uploaded = true;
      scene.gpu_material_indices.emplace(material_handle.value.value(), scene.gpu_materials.size());
      scene.gpu_materials.push_back(std::move(gpu_material));

      return scene;
   }

   /// @brief Adds a material with only the draw-state flags needed by sort tests.
   [[nodiscard]] vve::v3::MaterialHandle addSortMaterial(vve::v3::SceneData &scene, std::string_view suffix,
                                                         bool double_sided, bool alpha_blend) {
      vve::v3::ImportedMaterial material{};
      material.handle =
          vve::v3::MaterialHandle{.value = vve::Handle::fromHash(std::string{"tests.render.sort.material."} + std::string{suffix})};
      material.double_sided = double_sided;
      material.alpha_blend = alpha_blend;
      scene.material_indices.emplace(material.handle.value.value(), scene.materials.size());
      scene.materials.push_back(std::move(material));
      return scene.materials.back().handle;
   }

   /// @brief Adds a node translated along the renderer's default view axis.
   [[nodiscard]] vve::v3::SceneNodeHandle addSortNode(vve::v3::SceneData &scene, std::string_view suffix,
                                                      float z) {
      vve::v3::SceneNodeDesc node{};
      node.handle =
          vve::v3::SceneNodeHandle{.value = vve::Handle::fromHash(std::string{"tests.render.sort.node."} + std::string{suffix})};
      node.world_transform = vve::math::translate(vve::math::identityMat4(), vve::math::Vec3(0.0F, 0.0F, z));
      scene.node_indices.emplace(node.handle.value.value(), scene.nodes.size());
      scene.nodes.push_back(std::move(node));
      return scene.nodes.back().handle;
   }

   /// @brief Adds one mesh instance using a per-instance material override.
   void addSortMeshInstance(vve::v3::SceneData &scene, std::string_view suffix, vve::v3::SceneNodeHandle node,
                            vve::v3::MaterialHandle material) {
      const auto instance = vve::Handle::fromHash(std::string{"tests.render.sort.instance."} + std::string{suffix});
      scene.mesh_instance_indices.emplace(instance.value(), scene.mesh_instances.size());
      scene.mesh_instances.push_back(vve::v3::SceneMeshInstanceDesc{
          .handle = instance,
          .node = node,
          .mesh = scene.meshes.front().handle,
          .material_override = material});
   }

   /// @brief Verifies that renderer binding stores the backend resources for the selected renderer.
   [[nodiscard]] bool pipelineBindsRenderer(vve::v3::RenderSystem &render_system, vve::v3::GraphicsBackend &backend,
                                            std::string_view renderer_id, vve::v3::RenderKernelId expected_kernel) {
      const auto renderer = backend.createRenderer(renderer_id);
      if (!renderer) {
         return false;
      }

      const auto pipeline = testPipeline(render_system, *renderer);
      const auto binding = render_system.bindPipelineResources(pipeline);
      if (!binding) {
         return false;
      }

      const auto stored = render_system.rendererPipeline(pipeline.window);
      return stored && stored->has_value() &&
             binding->renderer.value == renderer->handle.value &&
             binding->backend_resources.value == pipeline.backend_resources.handle.value &&
             binding->shader_program.value == pipeline.shader_program.value &&
             binding->main_kernel == expected_kernel &&
             binding->color_format == pipeline.swapchain.color_attachment_format &&
             binding->depth_format == pipeline.swapchain.depth_attachment_format &&
             binding->shader_stage_count == pipeline.pipeline_layout.shader_stages.size() &&
             binding->descriptor_set_layout_count == pipeline.pipeline_layout.descriptor_sets.size() &&
             binding->ready_for_pipeline_creation &&
             (*stored)->backend_resources.value == binding->backend_resources.value;
   }

   /// @brief Verifies renderer-specific graphics pipeline requests reach the backend cleanly.
   [[nodiscard]] bool pipelineRequestRequiresBackendInit(vve::v3::RenderSystem &render_system,
                                                         vve::v3::GraphicsBackend &backend,
                                                         std::string_view renderer_id) {
      const auto renderer = backend.createRenderer(renderer_id);
      if (!renderer) {
         return false;
      }

      const auto pipeline = testPipeline(render_system, *renderer);
      const auto binding = render_system.bindPipelineResources(pipeline);
      if (!binding) {
         return false;
      }

      const auto graphics_pipeline = render_system.createGraphicsPipeline(backend, *binding);
      return !graphics_pipeline && graphics_pipeline.error() == vve::Error::not_initialized;
   }

   /// @brief Verifies uploaded mesh resources are translated to backend-neutral draw packets.
   [[nodiscard]] bool drawPacketsReferenceUploadedMesh(vve::v3::RenderSystem &render_system,
                                                       vve::v3::GraphicsBackend &backend) {
      const auto renderer = backend.createRenderer("forward");
      if (!renderer) {
         return false;
      }

      const auto pipeline = testPipeline(render_system, *renderer);
      const auto binding = render_system.bindPipelineResources(pipeline);
      if (!binding) {
         return false;
      }

      const auto scene = testUploadedScene();
      const auto *pass = mainPass(pipeline.graph);
      if (pass == nullptr) {
         return false;
      }

      const auto build = render_system.buildDrawPackets(
          vve::v3::FrameContext{.frame_index = 42, .delta_seconds = 0.0}, scene, pipeline.window, pipeline.graph);
      if (!build) {
         return false;
      }

      const auto packet_list = render_system.drawPackets(pipeline.window);
      if (!packet_list || !packet_list->has_value() || (*packet_list)->packets.size() != 1 ||
          (*packet_list)->window.value != pipeline.window.value || (*packet_list)->frame_index != 42) {
         return false;
      }

      const auto &packet = (*packet_list)->packets.front();
      const auto &gpu_mesh = scene.gpu_meshes.front();
      const auto &gpu_material = scene.gpu_materials.front();
      const auto &mesh_instance = scene.mesh_instances.front();
      const auto &submesh = scene.meshes.front().submeshes.front();
      const auto &material = scene.materials.front();
      return packet.window.value == pipeline.window.value &&
             packet.pass.value.value() == pass->handle.value.value() &&
             packet.kernel == pass->kernel &&
             packet.pipeline_variant == vve::v3::GraphicsPipelineVariant::double_sided_alpha_blend &&
             packet.draw_index == 0 &&
             packet.camera.position.z == scene.active_camera.position.z &&
             packet.camera.view_transform[3][2] == scene.active_camera.view_transform[3][2] &&
             packet.camera.vertical_fov_radians == scene.active_camera.vertical_fov_radians &&
             packet.camera.near_plane == scene.active_camera.near_plane &&
             packet.camera.far_plane == scene.active_camera.far_plane &&
             packet.camera_depth == 7.0F &&
             packet.node.value.value() == mesh_instance.node.value.value() &&
             packet.mesh_instance.value() == mesh_instance.handle.value() &&
             packet.mesh.value.value() == mesh_instance.mesh.value.value() &&
             packet.material.value.value() == submesh.material.value.value() &&
             packet.material_index.has_value() && *packet.material_index == 0 &&
             packet.material_constants_buffer.value.value() == gpu_material.constants_buffer.value.value() &&
             packet.material_textures.size() == 1 &&
             packet.material_textures.front().texture.value == gpu_material.textures.front().texture.value &&
             packet.material_textures.front().image.value == gpu_material.textures.front().image.value &&
             packet.material_textures.front().sampler.value == gpu_material.textures.front().sampler.value &&
             packet.material_textures.front().semantic == vve::v3::TextureSemantic::base_color &&
             packet.material_constants.base_color_factor.x == material.base_color_factor.x &&
             packet.material_constants.base_color_factor.y == material.base_color_factor.y &&
             packet.material_constants.base_color_factor.z == material.base_color_factor.z &&
             packet.material_constants.base_color_factor.w == material.base_color_factor.w &&
             packet.material_constants.emissive_factor.x == material.emissive_factor.x &&
             packet.material_constants.emissive_factor.y == material.emissive_factor.y &&
             packet.material_constants.emissive_factor.z == material.emissive_factor.z &&
             packet.material_constants.roughness_factor == material.roughness_factor &&
             packet.material_constants.metallic_factor == material.metallic_factor &&
             packet.material_constants.normal_scale == material.normal_scale &&
             packet.material_constants.alpha_cutoff == material.alpha_cutoff &&
             packet.lighting_constants.light_count == 1 &&
             packet.lighting_constants.ambient_color.x == static_cast<vve::math::Scalar>(0.32) &&
             packet.lighting_constants.lights.front().type == vve::v3::SceneLightType::directional &&
             packet.lighting_constants.lights.front().direction.y == 1.0F &&
             packet.lighting_constants.lights.front().color.z == 0.7F &&
             packet.lighting_constants.lights.front().intensity == 2.0F &&
             packet.world_transform[3][0] == 1.0F &&
             packet.world_transform[3][1] == 2.0F &&
             packet.world_transform[3][2] == 3.0F &&
             packet.vertex_buffer.value.value() == gpu_mesh.vertex_buffer.value.value() &&
             packet.index_buffer.value.value() == gpu_mesh.index_buffer.value.value() &&
             packet.first_index == submesh.index_offset &&
             packet.index_count == submesh.index_count &&
             packet.instance_count == 1 &&
             packet.double_sided &&
             packet.alpha_blend;
   }

   /// @brief Verifies opaque bins precede transparent back-to-front ordering.
   [[nodiscard]] bool drawPacketsAreSortedForForwardRenderer(vve::v3::RenderSystem &render_system,
                                                             vve::v3::GraphicsBackend &backend) {
      const auto renderer = backend.createRenderer("forward");
      if (!renderer) {
         return false;
      }

      const auto pipeline = testPipeline(render_system, *renderer);
      const auto binding = render_system.bindPipelineResources(pipeline);
      if (!binding) {
         return false;
      }

      auto scene = testUploadedScene();
      auto &near_alpha_material = scene.materials.front();
      near_alpha_material.double_sided = false;
      near_alpha_material.alpha_blend = true;
      scene.nodes.front().world_transform =
          vve::math::translate(vve::math::identityMat4(), vve::math::Vec3(0.0F, 0.0F, 2.0F));

      const auto opaque = addSortMaterial(scene, "opaque", false, false);
      const auto alpha_far = addSortMaterial(scene, "alpha_far", false, true);
      const auto double_opaque = addSortMaterial(scene, "double_opaque", true, false);
      const auto double_alpha_mid = addSortMaterial(scene, "double_alpha_mid", true, true);

      addSortMeshInstance(scene, "opaque", addSortNode(scene, "opaque", 0.0F), opaque);
      addSortMeshInstance(scene, "alpha_far", addSortNode(scene, "alpha_far", -8.0F), alpha_far);
      addSortMeshInstance(scene, "double_opaque", addSortNode(scene, "double_opaque", 1.0F), double_opaque);
      addSortMeshInstance(scene, "double_alpha_mid", addSortNode(scene, "double_alpha_mid", -2.0F),
                          double_alpha_mid);

      const auto build = render_system.buildDrawPackets(
          vve::v3::FrameContext{.frame_index = 77, .delta_seconds = 0.0}, scene, pipeline.window, pipeline.graph);
      if (!build) {
         return false;
      }

      const auto packet_list = render_system.drawPackets(pipeline.window);
      if (!packet_list || !packet_list->has_value() || (*packet_list)->packets.size() != 5) {
         return false;
      }

      const auto &packets = (*packet_list)->packets;
      return packets[0].material.value.value() == opaque.value.value() &&
             packets[0].pipeline_variant == vve::v3::GraphicsPipelineVariant::opaque &&
             packets[0].sort_bucket == 0 && packets[0].draw_index == 0 &&
             packets[1].material.value.value() == double_opaque.value.value() &&
             packets[1].pipeline_variant == vve::v3::GraphicsPipelineVariant::double_sided &&
             packets[1].sort_bucket == 1 && packets[1].draw_index == 1 &&
             packets[2].material.value.value() == alpha_far.value.value() &&
             packets[2].pipeline_variant == vve::v3::GraphicsPipelineVariant::alpha_blend &&
             packets[2].camera_depth > packets[3].camera_depth && packets[2].draw_index == 2 &&
             packets[3].material.value.value() == double_alpha_mid.value.value() &&
             packets[3].pipeline_variant == vve::v3::GraphicsPipelineVariant::double_sided_alpha_blend &&
             packets[3].camera_depth > packets[4].camera_depth && packets[3].draw_index == 3 &&
             packets[4].material.value.value() == scene.materials.front().handle.value.value() &&
             packets[4].pipeline_variant == vve::v3::GraphicsPipelineVariant::alpha_blend &&
             packets[4].draw_index == 4;
   }

   /// @brief Verifies draw-packet generation respects the active camera clip range.
   [[nodiscard]] bool drawPacketsCullCameraClipRange(vve::v3::RenderSystem &render_system,
                                                     vve::v3::GraphicsBackend &backend) {
      const auto renderer = backend.createRenderer("forward");
      if (!renderer) {
         return false;
      }

      const auto pipeline = testPipeline(render_system, *renderer);
      const auto binding = render_system.bindPipelineResources(pipeline);
      if (!binding) {
         return false;
      }

      auto scene = testUploadedScene();
      scene.active_camera.far_plane = 6.0F;
      const auto culled_build = render_system.buildDrawPackets(
          vve::v3::FrameContext{.frame_index = 90, .delta_seconds = 0.0}, scene, pipeline.window, pipeline.graph);
      if (!culled_build) {
         return false;
      }

      const auto culled_packets = render_system.drawPackets(pipeline.window);
      if (!culled_packets || !culled_packets->has_value() || !(*culled_packets)->packets.empty()) {
         return false;
      }

      scene.active_camera.far_plane = 8.0F;
      const auto visible_build = render_system.buildDrawPackets(
          vve::v3::FrameContext{.frame_index = 91, .delta_seconds = 0.0}, scene, pipeline.window, pipeline.graph);
      if (!visible_build) {
         return false;
      }

      const auto visible_packets = render_system.drawPackets(pipeline.window);
      return visible_packets && visible_packets->has_value() && (*visible_packets)->packets.size() == 1 &&
             (*visible_packets)->packets.front().camera_depth == 7.0F &&
             (*visible_packets)->frame_index == 91;
   }

   /// @brief Verifies near-plane culling keeps large meshes whose bounds still intersect the camera frustum.
   [[nodiscard]] bool drawPacketsKeepBoundsThatOverlapNearPlane(vve::v3::RenderSystem &render_system,
                                                                vve::v3::GraphicsBackend &backend) {
      const auto renderer = backend.createRenderer("forward");
      if (!renderer) {
         return false;
      }

      const auto pipeline = testPipeline(render_system, *renderer);
      const auto binding = render_system.bindPipelineResources(pipeline);
      if (!binding) {
         return false;
      }

      auto scene = testUploadedScene();
      scene.meshes.front().bounds_min = vve::math::Vec3(-1.0F, -1.0F, -2.0F);
      scene.meshes.front().bounds_max = vve::math::Vec3(1.0F, 1.0F, 0.0F);
      scene.nodes.front().world_transform =
          vve::math::translate(vve::math::identityMat4(), vve::math::Vec3(0.0F, 0.0F, 9.9F));

      const auto build = render_system.buildDrawPackets(
          vve::v3::FrameContext{.frame_index = 93, .delta_seconds = 0.0}, scene, pipeline.window, pipeline.graph);
      if (!build) {
         return false;
      }

      const auto packet_list = render_system.drawPackets(pipeline.window);
      return packet_list && packet_list->has_value() && (*packet_list)->packets.size() == 1 &&
             (*packet_list)->packets.front().camera_depth < scene.active_camera.near_plane &&
             (*packet_list)->frame_index == 93;
   }

   /// @brief Verifies a window-specific camera overrides the scene fallback camera for packet generation.
   [[nodiscard]] bool drawPacketsUseWindowCameraOverride(vve::v3::RenderSystem &render_system,
                                                         vve::v3::GraphicsBackend &backend) {
      const auto renderer = backend.createRenderer("forward");
      if (!renderer) {
         return false;
      }

      const auto pipeline = testPipeline(render_system, *renderer);
      const auto binding = render_system.bindPipelineResources(pipeline);
      if (!binding) {
         return false;
      }

      auto scene = testUploadedScene();
      auto window_camera = scene.active_camera;
      window_camera.position = vve::math::Vec3(0.0F, 0.0F, 20.0F);
      window_camera.view_transform =
          vve::math::translate(vve::math::identityMat4(), vve::math::Vec3(0.0F, 0.0F, -20.0F));
      window_camera.far_plane = 25.0F;
      scene.window_cameras.emplace(pipeline.window_id, window_camera);

      const auto build = render_system.buildDrawPackets(
          vve::v3::FrameContext{.frame_index = 92, .delta_seconds = 0.0}, scene, pipeline.window, pipeline.graph);
      if (!build) {
         return false;
      }

      const auto packet_list = render_system.drawPackets(pipeline.window);
      return packet_list && packet_list->has_value() && (*packet_list)->packets.size() == 1 &&
             (*packet_list)->packets.front().camera.position.z == 20.0F &&
             (*packet_list)->packets.front().camera_depth == 17.0F;
   }

   /// @brief Verifies point-light selection follows the draw object instead of the viewer position.
   [[nodiscard]] bool drawPacketLightingUsesMeshPosition(vve::v3::RenderSystem &render_system,
                                                        vve::v3::GraphicsBackend &backend) {
      const auto renderer = backend.createRenderer("forward");
      if (!renderer) {
         return false;
      }

      const auto pipeline = testPipeline(render_system, *renderer);
      const auto binding = render_system.bindPipelineResources(pipeline);
      if (!binding) {
         return false;
      }

      auto scene = testUploadedScene();
      scene.lights.clear();
      scene.active_camera.position = vve::math::Vec3(1000.0F, 0.0F, 10.0F);

      for (int light_index = 0; light_index < 8; ++light_index) {
         vve::v3::SceneNodeDesc node{};
         node.handle = vve::v3::SceneNodeHandle{
             .value = vve::Handle::fromHash(std::format("tests.render.camera_light.node.{}", light_index))};
         node.world_transform = vve::math::translate(
             vve::math::identityMat4(),
             vve::math::Vec3(1000.0F + static_cast<float>(light_index), 0.0F, 10.0F));
         scene.node_indices.emplace(node.handle.value.value(), scene.nodes.size());
         scene.nodes.push_back(node);
         scene.lights.push_back(vve::v3::ImportedLight{
             .handle = vve::v3::LightHandle{
                 .value = vve::Handle::fromHash(std::format("tests.render.camera_light.{}", light_index))},
             .node = node.handle,
             .name = std::format("CameraLight{}", light_index),
             .type = vve::v3::SceneLightType::point,
             .color = vve::math::Vec3(0.0F, 1.0F, 0.0F),
             .intensity = 2.0F,
             .range = 22.0F});
      }

      vve::v3::SceneNodeDesc object_light_node{};
      object_light_node.handle = vve::v3::SceneNodeHandle{
          .value = vve::Handle::fromHash(std::string_view{"tests.render.object_light.node"})};
      object_light_node.world_transform =
          vve::math::translate(vve::math::identityMat4(), vve::math::Vec3(1.5F, 2.0F, 3.0F));
      scene.node_indices.emplace(object_light_node.handle.value.value(), scene.nodes.size());
      scene.nodes.push_back(object_light_node);
      scene.lights.push_back(vve::v3::ImportedLight{
          .handle = vve::v3::LightHandle{.value = vve::Handle::fromHash(std::string_view{"tests.render.object_light"})},
          .node = object_light_node.handle,
          .name = "ObjectLight",
          .type = vve::v3::SceneLightType::point,
          .color = vve::math::Vec3(1.0F, 0.0F, 0.0F),
          .intensity = 2.0F,
          .range = 22.0F});

      const auto build = render_system.buildDrawPackets(
          vve::v3::FrameContext{.frame_index = 94, .delta_seconds = 0.0}, scene, pipeline.window, pipeline.graph);
      if (!build) {
         return false;
      }

      const auto packet_list = render_system.drawPackets(pipeline.window);
      if (!packet_list || !packet_list->has_value() || (*packet_list)->packets.size() != 1) {
         return false;
      }

      const auto &lighting = (*packet_list)->packets.front().lighting_constants;
      return std::ranges::any_of(lighting.lights.begin(), lighting.lights.begin() + lighting.light_count,
                                 [](const vve::v3::DrawLightConstants &light) {
                                    return light.type == vve::v3::SceneLightType::point && light.color.x == 1.0F &&
                                           light.color.y == 0.0F;
                                 });
   }

} // namespace

/**
 * @brief Executes the render-system graph selection regression tests.
 * @return Process exit code expected by the lightweight test runner.
 */
int main() {
   vve::v3::GraphicsBackend backend{};
   vve::v3::RenderSystem render_system{
       vve::RendererKind::forward_renderer,
       vve::ShadowKind::none,
       backend,
       false};

   if (!graphUsesKernel(render_system, backend, "forward", vve::v3::RenderKernelId::forward_opaque)) {
      return 1;
   }

   const auto unavailable_deferred = backend.createRenderer("deferred");
   if (unavailable_deferred || unavailable_deferred.error() != vve::Error::unsupported_version) {
      return 2;
   }

   const auto unavailable_path_tracing = backend.createRenderer("path_tracing");
   if (unavailable_path_tracing || unavailable_path_tracing.error() != vve::Error::unsupported_version) {
      return 3;
   }

   if (!pipelineBindsRenderer(render_system, backend, "forward", vve::v3::RenderKernelId::forward_opaque)) {
      return 4;
   }

   if (!pipelineRequestRequiresBackendInit(render_system, backend, "forward")) {
      return 5;
   }

   const auto forward = backend.createRenderer("forward");
   if (!forward) {
      return 6;
   }

   auto incompatible = testPipeline(render_system, *forward);
   incompatible.pipeline_layout.renderer =
       vve::v3::RendererHandle{.value = vve::Handle::fromHash(std::string_view{"tests.render.unavailable"})};
   incompatible.backend_resources = testResources(incompatible.pipeline_layout);
   const auto rejected = render_system.bindPipelineResources(incompatible);
   if (rejected || rejected.error() != vve::Error::invalid_argument) {
      return 7;
   }

   auto invalid_binding_source = testPipeline(render_system, *forward);
   const auto valid_binding = render_system.bindPipelineResources(invalid_binding_source);
   if (!valid_binding) {
      return 8;
   }
   auto invalid_binding = *valid_binding;
   invalid_binding.main_kernel = vve::v3::RenderKernelId::path_trace;
   const auto invalid_pipeline_request = render_system.createGraphicsPipeline(backend, invalid_binding);
   if (invalid_pipeline_request || invalid_pipeline_request.error() != vve::Error::invalid_argument) {
      return 9;
   }

   if (!drawPacketsReferenceUploadedMesh(render_system, backend)) {
      return 10;
   }

   if (!drawPacketsAreSortedForForwardRenderer(render_system, backend)) {
      return 11;
   }

   if (!drawPacketsCullCameraClipRange(render_system, backend)) {
      return 12;
   }

   if (!drawPacketsUseWindowCameraOverride(render_system, backend)) {
      return 13;
   }

   if (!drawPacketsKeepBoundsThatOverlapNearPlane(render_system, backend)) {
      return 14;
   }

   if (!drawPacketLightingUsesMeshPosition(render_system, backend)) {
      return 15;
   }

   return 0;
}
