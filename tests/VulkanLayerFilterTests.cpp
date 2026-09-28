/**
 * @file
 * @brief CPU-only checks for preserving Vulkan loader filters when disabling a compatibility layer.
 * Functional objects: main exercises exact comma-separated entries without Vulkan or environment changes.
 */
import std;
import VEEngine.Simple.Vulkan;

/// @brief Reports each filter result and returns a distinct failure code for an incorrect append or duplicate.
int main() {
   constexpr std::string_view layer{"VK_LAYER_AMD_switchable_graphics"};
   // Print the actual and expected filters so a failure can be diagnosed without a GPU.
   const auto check = [layer](std::string_view name, std::string current, std::string_view expected) {
      const auto actual = vve::simple::detail::appendLayerFilter(std::move(current), layer);
      std::println("[VulkanLayerFilterTests] case={} actual={} expected={} passed={}",
         name, actual, expected, actual == expected);
      return actual == expected;
   };
   if (!check("empty", "", layer)) { return 1; }
   if (!check("append", "A", std::string{"A,"} + std::string{layer})) { return 2; }
   if (!check("present_last", std::string{"A,"} + std::string{layer}, std::string{"A,"} + std::string{layer})) { return 3; }

   // Existing entries are recognized at every position, with unrelated filters left unchanged.
   if (!check("present_only", std::string{layer}, layer)) { return 4; }
   if (!check("present_first", std::string{layer} + ",A", std::string{layer} + ",A")) { return 5; }
   const auto middle = std::string{"A,"} + std::string{layer} + ",B";
   if (!check("present_middle", middle, middle)) { return 6; }

   // A substring of another layer name is not an existing comma-separated entry.
   const auto suffix = std::string{layer} + "_extra";
   if (!check("partial_suffix", suffix, suffix + ',' + std::string{layer})) { return 7; }
   const auto prefix = std::string{"prefix_"} + std::string{layer};
   if (!check("partial_prefix", prefix, prefix + ',' + std::string{layer})) { return 8; }
   return 0;
}
