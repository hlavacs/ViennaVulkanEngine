#include <algorithm>
#include <filesystem>

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

   const auto missing_shader = resource_system.loadShaderProgram(repository_root / "src/versions/v3/shaders/missing.slang",
                                                                 shader_system, vve::RendererKind::forward_renderer,
                                                                 vve::ShadowKind::none);
   if (missing_shader || missing_shader.error() != vve::Error::file_not_found) {
      return 12;
   }

   return 0;
}
