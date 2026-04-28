#include <algorithm>
#include <filesystem>
#include <string_view>

import VEEngine;
import VEEngine.V3;

/**
 * @file
 * @brief Regression tests for resource-managed shader program loading.
 */
namespace {

   /// @brief Finds the repository root from the CTest working directory.
   [[nodiscard]] std::filesystem::path findRepositoryRoot() {
      auto current_path = std::filesystem::current_path();
      for (;;) {
         if (std::filesystem::exists(current_path / "src/versions/v3/shaders/rasterizer.slang")) {
            return current_path;
         }

         if (!current_path.has_parent_path() || current_path == current_path.parent_path()) {
            return {};
         }

         current_path = current_path.parent_path();
      }
   }

   /// @brief Returns whether a pipeline layout contains a reflected binding suffix and kind.
   [[nodiscard]] bool hasBindingSuffix(const vve::v3::PipelineLayoutDesc &layout, std::string_view suffix,
                                       vve::v3::DescriptorBindingKind kind) {
      return std::ranges::any_of(layout.descriptor_sets, [suffix, kind](const auto &descriptor_set) {
         return std::ranges::any_of(descriptor_set.bindings, [suffix, kind](const auto &binding) {
            return binding.kind == kind && binding.name.ends_with(suffix);
         });
      });
   }

   /// @brief Returns whether a pipeline layout has a shader stage.
   [[nodiscard]] bool hasPipelineStage(const vve::v3::PipelineLayoutDesc &layout, vve::v3::ShaderStage stage) {
      return std::ranges::any_of(layout.shader_stages, [stage](const auto &shader_stage) {
         return shader_stage.stage == stage && !shader_stage.entry_point.empty() &&
                shader_stage.spirv_word_count > 0;
      });
   }

   /// @brief Builds a small imported scene with one indexed triangle mesh and one base-color texture.
   [[nodiscard]] vve::v3::ImportedScene makeTriangleScene(const std::filesystem::path &source_path,
                                                          const std::filesystem::path &texture_path) {
      const auto scene_handle = vve::v3::SceneHandle{
          .value = vve::Handle::fromHash(std::string_view{"tests.resources.triangle_scene"})};
      const auto mesh_handle = vve::v3::MeshHandle{
          .value = vve::Handle::fromHash(std::string_view{"tests.resources.triangle_mesh"})};
      const auto material_handle = vve::v3::MaterialHandle{
          .value = vve::Handle::fromHash(std::string_view{"tests.resources.triangle_material"})};
      const auto texture_handle = vve::v3::TextureHandle{
          .value = vve::Handle::fromHash(std::string_view{"tests.resources.triangle_texture"})};
      const auto node_handle = vve::v3::SceneNodeHandle{
          .value = vve::Handle::fromHash(std::string_view{"tests.resources.triangle_node"})};
      const auto mesh_instance_handle = vve::Handle::fromHash(std::string_view{"tests.resources.triangle_instance"});

      vve::v3::ImportedMesh mesh{};
      mesh.handle = mesh_handle;
      mesh.name = "triangle";
      mesh.vertices.push_back(vve::v3::ImportedVertex{});
      mesh.vertices.push_back(vve::v3::ImportedVertex{});
      mesh.vertices.push_back(vve::v3::ImportedVertex{});
      mesh.indices.push_back(0U);
      mesh.indices.push_back(1U);
      mesh.indices.push_back(2U);
      mesh.submeshes.push_back(vve::v3::ImportedSubmesh{.index_offset = 0,
                                                        .index_count = 3,
                                                        .material = material_handle});
      mesh.source_path = source_path;

      vve::v3::ImportedMaterial material{};
      material.handle = material_handle;
      material.name = "triangle_material";
      material.base_color_factor = vve::math::Vec4(0.25F, 0.5F, 0.75F, 1.0F);
      material.normal_scale = 1.25F;
      material.roughness_factor = 0.6F;
      material.metallic_factor = 0.2F;
      material.alpha_cutoff = 0.35F;
      material.textures.push_back(vve::v3::ImportedTextureRef{.texture = texture_handle,
                                                              .semantic = vve::v3::TextureSemantic::base_color,
                                                              .uv_set = 0});

      vve::v3::ImportedTexture texture{};
      texture.handle = texture_handle;
      texture.name = "triangle_base_color";
      texture.resolved_path = texture_path;
      texture.original_path = texture_path.filename();
      texture.embedded = false;

      vve::v3::ImportedSceneNode node{};
      node.handle = node_handle;
      node.name = "triangle_node";
      node.mesh_instances.push_back(vve::v3::ImportedMeshInstance{.handle = mesh_instance_handle, .mesh = mesh_handle});

      vve::v3::ImportedScene scene{};
      scene.handle = scene_handle;
      scene.name = "triangle_scene";
      scene.source_path = source_path;
      scene.textures.push_back(std::move(texture));
      scene.meshes.push_back(std::move(mesh));
      scene.materials.push_back(std::move(material));
      scene.nodes.push_back(std::move(node));
      return scene;
   }

} // namespace

/**
 * @brief Executes the resource-system shader loading regression tests.
 * @return Process exit code expected by the lightweight test runner.
 */
int main() {
   const auto repository_root = findRepositoryRoot();
   if (repository_root.empty()) {
      return 1;
   }

   vve::v3::ResourceSystem resource_system{};
   vve::v3::ShaderSystem shader_system{};

   const auto shader_metadata =
       resource_system.loadShaderProgram(repository_root / "src/versions/v3/shaders/rasterizer.slang", shader_system,
                                         vve::RendererKind::forward_renderer, vve::ShadowKind::none);
   if (!shader_metadata) {
      return 2;
   }

   if (shader_metadata->binaries.size() != 2 || shader_metadata->parameters.empty()) {
      return 3;
   }

   const auto stored_shader = resource_system.shaderProgram(shader_metadata->handle);
   if (!stored_shader || !stored_shader->has_value()) {
      return 4;
   }

   if ((*stored_shader)->handle.value != shader_metadata->handle.value ||
       (*stored_shader)->binaries.size() != shader_metadata->binaries.size()) {
      return 5;
   }

   const auto records = resource_system.enumerate();
   if (!records) {
      return 6;
   }

   const auto shader_record_it = std::ranges::find_if(*records, [&](const vve::v3::ResourceRecord &record) {
      return record.id == shader_metadata->handle.value && record.kind == vve::v3::ResourceKind::shader_program &&
             record.location == vve::v3::ResourceLocation::cpu_memory;
   });
   if (shader_record_it == records->end()) {
      return 7;
   }

   const auto deferred_shader =
       resource_system.loadShaderProgram(repository_root / "src/versions/v3/shaders/rasterizer.slang", shader_system,
                                         vve::RendererKind::deferred_renderer, vve::ShadowKind::none);
   if (!deferred_shader) {
      return 8;
   }

   if (deferred_shader->handle.value == shader_metadata->handle.value ||
       deferred_shader->intended_renderer != "deferred") {
      return 9;
   }

   const auto stored_forward = resource_system.shaderProgram(shader_metadata->handle);
   const auto stored_deferred = resource_system.shaderProgram(deferred_shader->handle);
   if (!stored_forward || !stored_forward->has_value() || !stored_deferred || !stored_deferred->has_value()) {
      return 10;
   }

   if ((*stored_forward)->intended_renderer != "forward" || (*stored_deferred)->intended_renderer != "deferred") {
      return 11;
   }

   vve::v3::GraphicsBackend backend{};
   const auto forward_renderer = backend.createRenderer("forward");
   if (!forward_renderer) {
      return 12;
   }

   const auto forward_layout = backend.createPipelineLayout(*forward_renderer, *shader_metadata);
   if (!forward_layout) {
      return 13;
   }

   if (forward_layout->renderer_id != "forward" ||
       forward_layout->shader_program.value != shader_metadata->handle.value ||
       !hasPipelineStage(*forward_layout, vve::v3::ShaderStage::vertex) ||
       !hasPipelineStage(*forward_layout, vve::v3::ShaderStage::fragment) ||
       forward_layout->descriptor_sets.empty()) {
      return 14;
   }

   if (!hasBindingSuffix(*forward_layout, "frame", vve::v3::DescriptorBindingKind::uniform_buffer) ||
       !hasBindingSuffix(*forward_layout, "baseColorTexture", vve::v3::DescriptorBindingKind::sampled_image) ||
       !hasBindingSuffix(*forward_layout, "baseColorSampler", vve::v3::DescriptorBindingKind::sampler)) {
      return 15;
   }

   const auto resources_before_init = backend.createPipelineResources(*forward_layout, *shader_metadata);
   if (resources_before_init || resources_before_init.error() != vve::Error::not_initialized) {
      return 16;
   }

   const auto deferred_renderer = backend.createRenderer("deferred");
   if (deferred_renderer || deferred_renderer.error() != vve::Error::unsupported_version) {
      return 17;
   }

   const auto missing_shader = resource_system.loadShaderProgram(repository_root / "src/versions/v3/shaders/missing.slang",
                                                                 shader_system, vve::RendererKind::forward_renderer,
                                                                 vve::ShadowKind::none);
   if (missing_shader || missing_shader.error() != vve::Error::file_not_found) {
      return 18;
   }

   const auto backend_init = backend.init();
   if (!backend_init) {
      return 19;
   }

   const auto imported_scene =
       makeTriangleScene(repository_root / "tests/triangle_scene.gltf", repository_root / "assets/fox/Texture.png");
   const auto register_scene = resource_system.registerImportedScene(imported_scene, imported_scene.source_path);
   if (!register_scene) {
      return 20;
   }

   vve::v3::SceneSystem scene_system{};
   auto scene = scene_system.instantiate(imported_scene);
   if (!scene) {
      return 21;
   }

   const auto upload = resource_system.uploadResources(vve::v3::FrameContext{.frame_index = 1, .delta_seconds = 0.016},
                                                       *scene, backend);
   if (!upload) {
      return 22;
   }

   if (scene->gpu_textures.size() != 1 ||
       !scene->gpu_texture_indices.contains(imported_scene.textures.front().handle.value.value()) ||
       scene->gpu_meshes.size() != 1 ||
       !scene->gpu_mesh_indices.contains(imported_scene.meshes.front().handle.value.value()) ||
       scene->gpu_materials.size() != 1 ||
       !scene->gpu_material_indices.contains(imported_scene.materials.front().handle.value.value())) {
      return 23;
   }

   const auto &gpu_texture = scene->gpu_textures.front();
   if (gpu_texture.texture.value != imported_scene.textures.front().handle.value ||
       !gpu_texture.image.value.isValid() || !gpu_texture.sampler.value.isValid() ||
       gpu_texture.usage != vve::v3::GpuImageUsage::sampled ||
       gpu_texture.format != vve::v3::GpuImageFormat::rgba8_srgb ||
       gpu_texture.width == 0 || gpu_texture.height == 0 || gpu_texture.mip_levels <= 1 ||
       gpu_texture.array_layers != 1 || !gpu_texture.image_created ||
       !gpu_texture.image_view_created || !gpu_texture.sampler_created || !gpu_texture.resident) {
      return 25;
   }

   const auto &gpu_mesh = scene->gpu_meshes.front();
   if (gpu_mesh.mesh.value != imported_scene.meshes.front().handle.value || gpu_mesh.vertex_count != 3 ||
       gpu_mesh.index_count != 3 || gpu_mesh.submesh_count != 1 ||
       gpu_mesh.vertex_stride != sizeof(vve::v3::ImportedVertex) || !gpu_mesh.resident ||
       !gpu_mesh.vertex_buffer.value.isValid() || !gpu_mesh.index_buffer.value.isValid()) {
      return 26;
   }

   const auto &gpu_material = scene->gpu_materials.front();
   if (gpu_material.material.value != imported_scene.materials.front().handle.value ||
       !gpu_material.constants_uploaded || !gpu_material.textures_uploaded ||
       !gpu_material.constants_buffer.value.isValid() || gpu_material.textures.size() != 1 ||
       gpu_material.textures.front().texture.value != imported_scene.textures.front().handle.value ||
       gpu_material.textures.front().image.value != gpu_texture.image.value ||
       gpu_material.textures.front().sampler.value != gpu_texture.sampler.value ||
       gpu_material.textures.front().semantic != vve::v3::TextureSemantic::base_color ||
       gpu_material.textures.front().binding != 0 || gpu_material.textures.front().uv_set != 0) {
      return 27;
   }

   const auto texture_image = backend.imageResources(gpu_texture.image);
   const auto vertex_buffer = backend.bufferResources(gpu_mesh.vertex_buffer);
   const auto index_buffer = backend.bufferResources(gpu_mesh.index_buffer);
   const auto material_buffer = backend.bufferResources(gpu_material.constants_buffer);
   if (!texture_image || !texture_image->has_value() ||
       (*texture_image)->image.value != gpu_texture.image.value ||
       (*texture_image)->width != gpu_texture.width ||
       (*texture_image)->height != gpu_texture.height ||
       !vertex_buffer || !vertex_buffer->has_value() || !index_buffer || !index_buffer->has_value() ||
       !material_buffer || !material_buffer->has_value() || (*vertex_buffer)->byte_size != gpu_mesh.vertex_byte_size ||
       (*index_buffer)->byte_size != gpu_mesh.index_byte_size ||
       (*material_buffer)->usage != vve::v3::GpuBufferUsage::uniform ||
       (*material_buffer)->owner != imported_scene.materials.front().handle.value ||
       (*material_buffer)->byte_size != sizeof(float) * 16U) {
      return 28;
   }

   const auto uploaded_records = resource_system.enumerate();
   if (!uploaded_records) {
      return 29;
   }

   const auto texture_record = std::ranges::find_if(*uploaded_records, [&](const vve::v3::ResourceRecord &record) {
      return record.id == imported_scene.textures.front().handle.value &&
             record.kind == vve::v3::ResourceKind::texture &&
             record.location == vve::v3::ResourceLocation::gpu_memory &&
             record.generation == gpu_texture.generation;
   });
   const auto image_record = std::ranges::find_if(*uploaded_records, [&](const vve::v3::ResourceRecord &record) {
      return record.id == gpu_texture.image.value && record.kind == vve::v3::ResourceKind::image &&
             record.location == vve::v3::ResourceLocation::gpu_memory;
   });
   const auto mesh_record = std::ranges::find_if(*uploaded_records, [&](const vve::v3::ResourceRecord &record) {
      return record.id == imported_scene.meshes.front().handle.value && record.kind == vve::v3::ResourceKind::mesh &&
             record.location == vve::v3::ResourceLocation::gpu_memory && record.generation == gpu_mesh.generation;
   });
   const auto vertex_record = std::ranges::find_if(*uploaded_records, [&](const vve::v3::ResourceRecord &record) {
      return record.id == gpu_mesh.vertex_buffer.value && record.kind == vve::v3::ResourceKind::buffer &&
             record.location == vve::v3::ResourceLocation::gpu_memory;
   });
   const auto index_record = std::ranges::find_if(*uploaded_records, [&](const vve::v3::ResourceRecord &record) {
      return record.id == gpu_mesh.index_buffer.value && record.kind == vve::v3::ResourceKind::buffer &&
             record.location == vve::v3::ResourceLocation::gpu_memory;
   });
   const auto material_record = std::ranges::find_if(*uploaded_records, [&](const vve::v3::ResourceRecord &record) {
      return record.id == imported_scene.materials.front().handle.value &&
             record.kind == vve::v3::ResourceKind::material &&
             record.location == vve::v3::ResourceLocation::gpu_memory &&
             record.generation == gpu_material.generation;
   });
   const auto material_buffer_record =
       std::ranges::find_if(*uploaded_records, [&](const vve::v3::ResourceRecord &record) {
          return record.id == gpu_material.constants_buffer.value && record.kind == vve::v3::ResourceKind::buffer &&
                 record.location == vve::v3::ResourceLocation::gpu_memory;
       });
   if (texture_record == uploaded_records->end() || image_record == uploaded_records->end() ||
       mesh_record == uploaded_records->end() || vertex_record == uploaded_records->end() ||
       index_record == uploaded_records->end() || material_record == uploaded_records->end() ||
       material_buffer_record == uploaded_records->end()) {
      return 30;
   }

   const auto uploaded_texture_generation = gpu_texture.generation;
   const auto uploaded_texture_image = gpu_texture.image;
   const auto uploaded_texture_sampler = gpu_texture.sampler;
   const auto uploaded_generation = gpu_mesh.generation;
   const auto uploaded_vertex_buffer = gpu_mesh.vertex_buffer;
   const auto uploaded_index_buffer = gpu_mesh.index_buffer;
   const auto uploaded_material_generation = gpu_material.generation;
   const auto uploaded_material_buffer = gpu_material.constants_buffer;
   const auto upload_again = resource_system.uploadResources(vve::v3::FrameContext{.frame_index = 2, .delta_seconds = 0.016},
                                                             *scene, backend);
   if (!upload_again || scene->gpu_textures.size() != 1 ||
       scene->gpu_textures.front().generation != uploaded_texture_generation ||
       scene->gpu_textures.front().image.value != uploaded_texture_image.value ||
       scene->gpu_textures.front().sampler.value != uploaded_texture_sampler.value ||
       scene->gpu_meshes.size() != 1 ||
       scene->gpu_meshes.front().generation != uploaded_generation ||
       scene->gpu_meshes.front().vertex_buffer.value != uploaded_vertex_buffer.value ||
       scene->gpu_meshes.front().index_buffer.value != uploaded_index_buffer.value ||
       scene->gpu_materials.size() != 1 ||
       scene->gpu_materials.front().generation != uploaded_material_generation ||
       scene->gpu_materials.front().constants_buffer.value != uploaded_material_buffer.value ||
       scene->gpu_materials.front().textures.size() != 1 ||
       scene->gpu_materials.front().textures.front().image.value != uploaded_texture_image.value ||
       scene->gpu_materials.front().textures.front().sampler.value != uploaded_texture_sampler.value) {
      return 31;
   }

   return 0;
}
