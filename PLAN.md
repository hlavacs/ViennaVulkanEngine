Read AGENTS.md and src/versions/simple/AGENTS.md

---

**Split `src/versions/simple/Vulkan.ixx` (3,505 lines) into module partitions under `src/versions/simple/Vulkan/`.**

**Goal:** `VEEngine.Simple.Vulkan` stays one module with an unchanged exported surface — `Renderer.ixx` and `RenderSystem.ixx` keep `import VEEngine.Simple.Vulkan;` untouched. The monolith becomes a thin primary interface `src/versions/simple/Vulkan/Vulkan.ixx` that re-exports partitions, each ≤ ~700 lines. No rendering behavior changes.

**Rules:** Smallest possible increments — move one partition per step. After every step: build + `ctest --output-on-failure`; never proceed on red; revert failed steps. One commit per step, prefixed `vulkan-split:`. Pure code motion — no renames, no signature changes, no new types unless listed below. Each new file starts with the AGENTS.md-mandated overview comment listing its functional objects. Don't touch the facade layer.

**Target layout (`src/versions/simple/Vulkan/`):**

- `Vulkan.ixx` — primary interface; `export module VEEngine.Simple.Vulkan;` + `export import :Device; ...` only (~40 lines).
- `Device.ixx` — partition `:Device`: `VulkanInstance`, `VulkanSurface`, `VulkanPhysicalDevice`, `VulkanDevice`, plus a new shared free function `findMemoryType(...)` (extracted from `VulkanDepthImage`; replaces the `friend struct ShadowMap` / `friend struct TextureImage` memory-selector hacks) (~450).
- `Presentation.ixx` — partition `:Presentation` (imports `:Device`): `VulkanSwapchain`, `VulkanImageViews`, `VulkanDepthImage`, `VulkanRenderPass`, `VulkanFramebuffers` (~650).
- `Pipeline.ixx` — partition `:Pipeline` (imports `:Device`): `VulkanDescriptorSetLayout`, `VulkanVertexInputDescription`, `VulkanPipelineLayout`, `VulkanShaderModule`, `VulkanGraphicsPipeline` (~700).
- `Commands.ixx` — partition `:Commands` (imports `:Device`): `VulkanCommandPool`, `VulkanCommandBuffers`, `VulkanFrameSync`, `VulkanBuffer` (~350).
- `Shadow.ixx` — partition `:Shadow` (imports `:Device`, `:Pipeline`): `ShadowMap` (~270).
- `Readback.ixx` — partition `:Readback` (imports `:Device`, `:Commands`): `VulkanReadback`, `VulkanDepthReadback`, `writeReadbackPng` (~530).
- `Resources.ixx` — partition `:Resources` (imports `:Device`, `:Commands`): `TextureImage`, `VulkanMesh`, `ObjectPushConstants`, `FrameUniforms`, `VulkanUniformBuffers`, `VulkanDescriptorPool`, `VulkanDescriptorSets` (~950; split `:Descriptors` out only if it exceeds 1,000 after the move).

Each partition repeats the global-module-fragment headers it actually needs (`<SDL3/SDL.h>`, `<vulkan/vulkan.h>`, `<SDL3/SDL_vulkan.h>`, stb headers) with the existing `SDL_MAIN_HANDLED` guard dance. `import VEEngine.Simple.Mesh/Math/Scene` only where used (`Mesh` → `:Resources`; `Scene`/`Math` → check actual users during the move).

**Phase 0 — Preparation:**
1. Create `src/versions/simple/Vulkan/` and move `Vulkan.ixx` into it unchanged; update the path in `src/versions/simple/CMakeLists.txt` `FILE_SET simple_modules`. Build green proves the folder move alone.
2. Move `STB_IMAGE_IMPLEMENTATION` out of the module's global fragment: the implementation define lives only in `stb_image_write_impl.cpp` (already a PRIVATE source); the module includes plain `stb_image.h`/`stb_image_write.h`. This must land before the split so no two partitions ever define the implementation (ODR).
3. Extract `findMemoryType` as a free function in the module and delete the two `friend struct` declarations in `VulkanDepthImage`; `ShadowMap` and `TextureImage` call the free function.

**Phase 1 — Carve out partitions,** one commit each, dependency order: `:Device` → `:Commands` → `:Presentation` → `:Pipeline` → `:Shadow` → `:Readback` → `:Resources`. Per step: create `Vulkan/<Name>.ixx` with `export module VEEngine.Simple.Vulkan:<Name>;`, move the listed structs verbatim, add `export import :<Name>;` to the primary interface, delete the moved code from it, add the file to CMake `FILE_SET`. Build + ctest green before the next.

**Phase 2 — Finish:**
1. Shrink the primary interface to re-exports plus the module-level doc comment (keep the per-object overview, now pointing at partition files).
2. Run the light_shadow_debug and simple_forward_demo examples; compare readback PNGs against pre-split captures — must be identical.
3. Grep check: no file outside `src/versions/simple/Vulkan/` mentions the partition names; importers still say only `import VEEngine.Simple.Vulkan;`.
4. Update `src/versions/simple/AGENTS.md` file inventory if it lists Vulkan.ixx, and FACADE_MIGRATION_NOTES.md if it references line numbers in Vulkan.ixx.

**Report:** steps completed, final line counts per partition, deviations and why.
