/** @file @brief CPU regression checks for example asset lookup with partially staged asset directories. */
import std;
import VVE.ExampleSupport;

namespace {

int checks{};   ///< Number of named assertions emitted for automated review.
int failures{}; ///< Number of failed asset-discovery contracts.

/** @brief Reports each candidate-selection outcome and preserves all failed assertions. */
void check(bool passed, std::string_view name) {
   ++checks;
   if (!passed) { ++failures; }
   std::cout << "check=" << name << " passed=" << passed << '\n';
}

/** @brief Creates a small fixture file without depending on renderer assets or a GPU. */
void writeFixture(const std::filesystem::path &path) {
   std::filesystem::create_directories(path.parent_path());
   std::ofstream file{path};
   if (!file.put('\n')) { throw std::runtime_error{"Cannot write asset lookup fixture"}; }
}

/** @brief Executes a lookup from a fixture directory and restores the process working directory on every exit. */
template <typename Function>
std::filesystem::path fromDirectory(const std::filesystem::path &directory, Function lookup) {
   const auto previous = std::filesystem::current_path();
   std::filesystem::current_path(directory);
   try {
      const auto result = lookup();
      std::filesystem::current_path(previous);
      return result;
   } catch (...) {
      std::filesystem::current_path(previous);
      throw;
   }
}

/** @brief Exercises incomplete working directories and executable ancestors against caller-selected actual files. */
void discovery(const std::filesystem::path &temporary) {
   const auto source = temporary / "source";
   const auto executableDirectory = source / "build/release/exe";
   const auto partial = temporary / "partial-working-directory";
   const auto empty = temporary / "empty-working-directory";
   const std::filesystem::path grass{"assets/game/plane/grass.jpg"};
   const std::filesystem::path texture{"assets/game/crate0/diffuse.png"};
   const std::filesystem::path scene{"assets/sea_keep_lonely_watcher/scene.gltf"};
   writeFixture(executableDirectory / "example-fixture");
   writeFixture(executableDirectory / "assets/island_delivery/engine.wav");
   writeFixture(partial / "assets/island_delivery/engine.wav");
   writeFixture(source / grass);
   writeFixture(source / "build" / texture);
   writeFixture(source / "build/release" / scene);
   std::filesystem::create_directories(empty);
   auto executable = (executableDirectory / "example-fixture").string();
   const auto locate = [&](const std::filesystem::path &required) {
      return fromDirectory(partial, [&] { return vve::example::assetRoot(executable.data(), required); });
   };
   check(locate(grass) == source, "partial_cwd_and_executable_assets_skip_to_complete_ancestor");
   check(locate(texture) == source / "build", "caller_selects_texture_file_ancestor");
   check(locate(scene) == source / "build/release", "caller_selects_scene_file_ancestor");
   // A directory named like a required file is another incomplete package, never an acceptable asset.
   std::filesystem::create_directories(executableDirectory / grass);
   check(locate(grass) == source, "directory_at_required_filename_is_rejected");
   std::filesystem::remove_all(executableDirectory / grass);
   writeFixture(executableDirectory / grass);
   check(locate(grass) == executableDirectory, "nearest_valid_executable_ancestor_wins");
   // A valid caller working directory intentionally takes priority over a complete executable package.
   writeFixture(partial / grass);
   check(locate(grass) == partial, "complete_working_directory_has_priority");
   check(fromDirectory(partial, [&] { return vve::example::assetRoot(nullptr, grass); }) == partial,
         "null_argv_uses_complete_working_directory");
   const auto configured = fromDirectory(empty, [&] { return vve::example::assetRoot(nullptr, grass); });
   check(std::filesystem::is_regular_file(configured / grass) && configured != source && configured != partial,
         "null_argv_falls_back_to_configured_real_source_assets");
   const auto missing = fromDirectory(empty, [&] {
      return vve::example::assetRoot(executable.data(), "assets/fixture-asset-that-does-not-exist.bin");
   });
   check(missing == configured, "no_matching_executable_ancestor_uses_configured_source");
   const auto invalidExecutable = (temporary / "missing-executable-directory/example").string();
   auto missingArgv = invalidExecutable;
   check(fromDirectory(empty, [&] { return vve::example::assetRoot(missingArgv.data(), grass); }) == configured,
         "nonexistent_executable_path_still_finds_source_fallback");
   std::cout << "configured_asset_root=" << configured.generic_string() << '\n';
}

} // namespace

/** @brief Runs isolated filesystem fixtures, removes them and reports compact deterministic assertion totals. */
int main() {
   const auto previous = std::filesystem::current_path();
   const auto temporary = std::filesystem::absolute(VVE_TEST_TMP_DIR);
   try {
      std::filesystem::remove_all(temporary);
      std::filesystem::create_directories(temporary);
      discovery(temporary);
      check(std::filesystem::current_path() == previous, "lookup_scope_restores_working_directory");
      std::filesystem::remove_all(temporary);
   } catch (const std::exception &error) {
      std::filesystem::current_path(previous);
      std::filesystem::remove_all(temporary);
      std::cerr << "fixture_error=" << error.what() << '\n';
      ++failures;
   }
   std::cout << "checks=" << checks << " failures=" << failures << '\n';
   return failures == 0 ? 0 : 1;
}
