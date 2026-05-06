#include <algorithm>
#include <filesystem>
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

} // namespace

int main() {
   const auto root = findRepositoryRoot();
   if (!root) { return 1; }

   vve::v4::ShaderSystem shaders{};
   const auto source = *root / "src/versions/v4/shaders/Forward.slang";
   const auto shader = shaders.compileAndReflect(
      source, vve::v4::Vector<std::string>{"vveForwardVertexMain", "vveForwardFragmentMain"});
   if (!shader || !shaders.containsShader(*shader)) { return 2; }

   const auto vertex_words = shaders.spirvWordCount(*shader, vve::v4::ShaderStage::vertex);
   const auto fragment_words = shaders.spirvWordCount(*shader, vve::v4::ShaderStage::fragment);
   if (!vertex_words || !fragment_words || *vertex_words == 0 || *fragment_words == 0) { return 3; }

   const auto has_parameter_block = shaders.hasReflectedBinding(*shader, "gVveForward");
   const auto has_debug_samples = shaders.hasReflectedBinding(*shader, "gVveForward.debugSamples");
   const auto has_debug_type = shaders.hasReflectedType(*shader, "VveForwardDebugSample");
   if (!has_parameter_block || !has_debug_samples || !has_debug_type) { return 4; }
   if (!*has_parameter_block || !*has_debug_samples || !*has_debug_type) { return 5; }

   const auto missing = shaders.compileAndReflect(
      source, vve::v4::Vector<std::string>{"vveForwardMissingEntryPoint"});
   if (missing || missing.error() != vve::v4::Error::missing_object) { return 6; }

   return 0;
}
