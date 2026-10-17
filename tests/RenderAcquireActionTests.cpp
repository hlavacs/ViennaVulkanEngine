/**
 * @file
 * @brief CPU-only regression coverage of acquisition, presentation and swapchain recovery decisions.
 *
 * Functional objects:
 * - main: checks synthetic Vulkan results without creating a window, instance or device.
 */
#include <vulkan/vulkan_core.h>

import std;
import VVEngine.Error;
import VVEngine.Simple.Renderer;

/// @brief Reports every acquisition decision; returns the failing case number, or zero on success.
int main() {
   using vve::simple::AcquireAction;
   constexpr std::array cases{
      std::pair{VK_TIMEOUT, AcquireAction::skip},
      std::pair{VK_NOT_READY, AcquireAction::skip},
      std::pair{VK_SUCCESS, AcquireAction::render},
      std::pair{VK_SUBOPTIMAL_KHR, AcquireAction::render},
      std::pair{VK_ERROR_OUT_OF_DATE_KHR, AcquireAction::recreate},
      std::pair{VK_ERROR_DEVICE_LOST, AcquireAction::fail},
      std::pair{VK_ERROR_SURFACE_LOST_KHR, AcquireAction::fail},
   }; ///< Timeout/readiness results must not take the error path or consume an unsignaled semaphore.

   // Exercise the renderer's policy directly; no Vulkan entry point is called.
   int case_number{}; ///< Stable nonzero exit code identifies a failed mapping.
   for (const auto &[result, expected] : cases) {
      ++case_number;
      const auto action = vve::simple::acquireAction(result);
      std::println("[RenderAcquireActionTests] case={} result={} action={} expected={}",
         case_number, static_cast<int>(result), static_cast<int>(action), static_cast<int>(expected));
      if (action != expected) { return case_number; }
   }

   using vve::simple::FrameOutcome;
   constexpr std::array outcomes{
      std::tuple{VK_SUCCESS, FrameOutcome::presented, std::optional{FrameOutcome::presented}},
      std::tuple{VK_SUBOPTIMAL_KHR, FrameOutcome::presented, std::optional{FrameOutcome::presented}},
      std::tuple{VK_TIMEOUT, FrameOutcome::presented, std::optional{FrameOutcome::skipped}},
      std::tuple{VK_NOT_READY, FrameOutcome::presented, std::optional{FrameOutcome::skipped}},
      std::tuple{VK_ERROR_OUT_OF_DATE_KHR, FrameOutcome::presented, std::optional{FrameOutcome::skipped}},
      std::tuple{VK_SUCCESS, FrameOutcome::skipped, std::optional{FrameOutcome::skipped}},
      std::tuple{VK_NOT_READY, FrameOutcome::skipped, std::optional{FrameOutcome::skipped}},
      std::tuple{VK_ERROR_OUT_OF_DATE_KHR, FrameOutcome::skipped, std::optional{FrameOutcome::skipped}},
      std::tuple{VK_ERROR_DEVICE_LOST, FrameOutcome::presented, std::optional<FrameOutcome>{}},
      std::tuple{VK_ERROR_SURFACE_LOST_KHR, FrameOutcome::presented, std::optional<FrameOutcome>{}},
      std::tuple{VK_ERROR_OUT_OF_DEVICE_MEMORY, FrameOutcome::skipped, std::optional<FrameOutcome>{}},
      std::tuple{VK_ERROR_INITIALIZATION_FAILED, FrameOutcome::skipped, std::optional<FrameOutcome>{}},
   }; ///< Rebuilding successfully still skips the current frame; a fatal rebuild must never be swallowed.
   // Exercise the same classification used by drawFrame, including driver failure codes without a GPU.
   for (const auto &[result, completed, expected] : outcomes) {
      ++case_number;
      const auto outcome = vve::simple::frameOutcome(result, completed);
      std::println("[RenderAcquireActionTests] case={} result={} outcome={} error={}", case_number,
         static_cast<int>(result), outcome ? static_cast<int>(*outcome) : -1,
         outcome ? "none" : vve::errorName(outcome.error()));
      if (outcome.has_value() != expected.has_value() || (outcome && *outcome != *expected) ||
         (!outcome && outcome.error() != vve::Error::platform_error)) { return case_number; }
   }
   return 0;
}
