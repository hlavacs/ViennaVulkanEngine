/**
 * @file
 * @brief CPU-only regression coverage of swapchain acquisition decisions.
 *
 * Functional objects:
 * - main: checks synthetic Vulkan results without creating a window, instance or device.
 */
#include <vulkan/vulkan_core.h>

import std;
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
   return 0;
}
