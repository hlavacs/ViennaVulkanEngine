#include <algorithm>
#include <filesystem>
#include <iostream>

import VEEngine;
import VEEngine.V3;

/**
 * @file
 * @brief Regression tests for Slang shader compilation and reflection.
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

   /// @brief Returns whether reflected metadata contains a stage.
   [[nodiscard]] bool hasStage(const vve::v3::ShaderMetadata &metadata, vve::v3::ShaderStage stage) {
      return std::ranges::contains(metadata.stages, stage);
   }

   /// @brief Returns whether reflected metadata contains a named parameter suffix.
   [[nodiscard]] bool hasParameterSuffix(const vve::v3::ShaderMetadata &metadata, std::string_view suffix) {
      return std::ranges::any_of(metadata.parameters, [suffix](const vve::v3::ShaderParameter &parameter) {
         return parameter.name.ends_with(suffix);
      });
   }

} // namespace

/**
 * @brief Executes the shader-system regression tests.
 * @return Process exit code expected by the lightweight test runner.
 */
int main() {
   const auto repository_root = findRepositoryRoot();
   if (repository_root.empty()) {
      return 1;
   }

   vve::v3::ShaderSystem shader_system{};
   const auto metadata = shader_system.reflect(repository_root / "src/versions/v3/shaders/rasterizer.slang",
                                               vve::RendererKind::forward_renderer, vve::ShadowKind::shadow_map);
   if (!metadata) {
      return 2;
   }

   if (!hasStage(*metadata, vve::v3::ShaderStage::vertex) ||
       !hasStage(*metadata, vve::v3::ShaderStage::fragment)) {
      return 3;
   }

   if (metadata->binaries.size() != 2) {
      return 4;
   }

   for (const auto &binary : metadata->binaries) {
      if (binary.spirv_words.empty() || binary.spirv_words.front() != 0x07230203U) {
         return 5;
      }
   }

   if (!hasParameterSuffix(*metadata, "frame") || !hasParameterSuffix(*metadata, "baseColorTexture") ||
       !hasParameterSuffix(*metadata, "baseColorSampler")) {
      for (const auto &parameter : metadata->parameters) {
         std::cerr << parameter.name << ' ' << parameter.type_name << ' ' << parameter.binding_kind << " set="
                   << parameter.set << " binding=" << parameter.binding << '\n';
      }
      return 6;
   }

   return 0;
}
