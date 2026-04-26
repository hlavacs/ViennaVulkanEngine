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
   if (!deferred_renderer) {
      return 17;
   }

   const auto deferred_layout = backend.createPipelineLayout(*deferred_renderer, *deferred_shader);
   if (!deferred_layout || deferred_layout->renderer_id != "deferred" ||
       deferred_layout->shader_program.value != deferred_shader->handle.value ||
       deferred_layout->shader_program.value == forward_layout->shader_program.value) {
      return 18;
   }

   const auto missing_shader = resource_system.loadShaderProgram(repository_root / "src/versions/v3/shaders/missing.slang",
                                                                 shader_system, vve::RendererKind::forward_renderer,
                                                                 vve::ShadowKind::none);
   if (missing_shader || missing_shader.error() != vve::Error::file_not_found) {
      return 19;
   }

   return 0;
}
