#include <algorithm>
#include <expected>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

import VEEngine.V4;

namespace {

[[nodiscard]] std::optional<std::filesystem::path> findRepositoryRoot() {
   auto path = std::filesystem::current_path();
   while (!path.empty()) {
      if (std::filesystem::exists(path / "src/versions/v4/shaders/Forward.slang")) { return path; }
      const auto parent = path.parent_path();
      if (parent == path) { break; }
      path = parent;
   }
   return std::nullopt;
}

[[nodiscard]] bool hasStage(const vve::v4::Vector<vve::v4::ShaderEntryPointReflection> &entry_points,
                            vve::v4::ShaderStage stage) {
   return std::ranges::any_of(entry_points, [stage](const auto &entry_point) {
      return entry_point.stage == stage;
   });
}

void printBindings(const vve::v4::Vector<vve::v4::ShaderBindingReflection> &bindings) {
   for (const auto &binding : bindings) {
      std::cerr << binding.name << ' ' << binding.category << ' ' << binding.type << " set=" << binding.set
                << " binding=" << binding.binding << '\n';
   }
}

} // namespace

int main() {
   const auto root = findRepositoryRoot();
   if (!root) { return 1; }

   const auto shader_path = *root / "src/versions/v4/shaders/Forward.slang";
   vve::v4::ShaderSystem shader_system{};
   auto shader = shader_system.compileAndReflect(
      shader_path, vve::v4::Vector<std::string>{"vveForwardVertexMain", "vveForwardFragmentMain"});
   if (!shader) {
      std::cerr << "compileAndReflect failed: " << vve::v4::errorName(shader.error()) << '\n';
      return 2;
   }

   const auto stage_count = shader_system.shaderStageCount(*shader);
   if (!shader_system.containsShader(*shader) || !stage_count || *stage_count != 2) { return 3; }

   const auto vertex_words = shader_system.spirvWordCount(*shader, vve::v4::ShaderStage::vertex);
   const auto fragment_words = shader_system.spirvWordCount(*shader, vve::v4::ShaderStage::fragment);
   if (!vertex_words || !fragment_words || *vertex_words == 0 || *fragment_words == 0) { return 4; }

   const auto entry_points = shader_system.reflectedEntryPoints(*shader);
   if (!entry_points || entry_points->size() != 2 ||
       !hasStage(*entry_points, vve::v4::ShaderStage::vertex) ||
       !hasStage(*entry_points, vve::v4::ShaderStage::fragment)) {
      return 5;
   }

   const auto bindings = shader_system.reflectedBindings(*shader);
   if (!bindings || bindings->empty()) { return 6; }

   const auto has_forward_block = shader_system.hasReflectedBinding(*shader, "gVveForward");
   const auto has_debug_buffer = shader_system.hasReflectedBinding(*shader, "gVveForward.debugSamples");
   if (!has_forward_block || !has_debug_buffer || !*has_forward_block || !*has_debug_buffer) {
      printBindings(*bindings);
      return 7;
   }

   const auto has_debug_sample = shader_system.hasReflectedType(*shader, "VveForwardDebugSample");
   if (!has_debug_sample || !*has_debug_sample) { return 8; }

   const auto triangle_path = *root / "src/versions/v4/shaders/Triangle.slang";
   auto triangle = shader_system.compileAndReflect(
      triangle_path, vve::v4::Vector<std::string>{"vveTriangleVertexMain", "vveTriangleFragmentMain"});
   if (!triangle) { return 9; }

   const auto triangle_vertex_words = shader_system.stageSpirv(*triangle, vve::v4::ShaderStage::vertex);
   const auto triangle_fragment_words = shader_system.stageSpirv(*triangle, vve::v4::ShaderStage::fragment);
   if (!triangle_vertex_words || !triangle_fragment_words ||
       triangle_vertex_words->empty() || triangle_fragment_words->empty()) {
      return 10;
   }

   return 0;
}
