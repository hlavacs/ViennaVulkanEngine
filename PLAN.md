Read AGENTS.md and src/versions/simple/AGENTS.md

File deleted. Here's the command, condensed:

---

**Enforce the VEEngine facade boundary.**

Read `FACADE_MIGRATION_NOTES.md` (repo root) first — it is the authoritative catalog of violations, symbol mappings, and the ordered lockdown plan. Also read `AGENTS.md`.

**Goal:** (1) No `src/*.ixx` facade module imports `VEEngine.Simple*` or names `VVE_ENGINE_IMPLEMENTATION_NAMESPACE` in its exported interface — implementation coupling only in non-exported module implementation units. (2) `src/versions/simple/*.ixx` become PRIVATE in CMake so apps cannot `import VEEngine.Simple.*`. (3) All five examples use only `import VEEngine;`, link only `ViennaVulkanEngine`, no SDL/Vulkan includes. (4) All tests pass.

**Rules:** Smallest possible increments. After every step: build + `ctest --output-on-failure`; never proceed on red; revert failed steps. One commit per step, prefixed `facade:`. Never expose `vve::simple::`, `SDL_*`, or `Vk*` types in exported facade declarations — use opaque handles/Pimpl with forwarding in implementation units. New APIs use facade types only (`std::expected<..., vve::Error>`, etc.), with Doxygen comments and `[[nodiscard]]`. Don't change rendering behavior. Don't touch v3/v4/v5.

**Phase 1 — Decouple facade (follow the notes' "Smallest-first tasklets" in order):** World.ixx unused import → facade-owned Error → Math → Handle → Vector → verify Types.ixx pure → facade-owned WindowSetup/WindowSetups → convert Gui/Input/Window/WindowSystem/Assets/RenderSystem wrappers from `Impl&` to opaque handles (one per commit) → decouple ECS → decouple Engine (opaque owning handle) → move simple modules to PRIVATE file set; verify `import VEEngine.Simple.Scene;` fails to compile.

**Phase 2 — Add missing facade APIs** (per the notes' gap tables): `loadSampleScene()`, `addTexturedCuboid/addMesh/addMaterial/addObject`, light getters + extended setters (ambient, inner cone), extent-aware `setCamera(Camera)`, `enum class vve::Key` + InputState overloads, and facade readback/debug access replacing `backend()`. One test per new API.

**Phase 3 — Migrate examples,** one commit each, easiest first: physics → sponza → simple_forward_demo → game → light_shadow_debug. Use the notes' mapping tables; remove SDL/Vulkan includes; link only `ViennaVulkanEngine`; verify readback PNGs match pre-migration captures.

**Phase 4 — Guard:** add a CI/CTest grep check failing on `VEEngine.Simple|vve::simple|<SDL3/|<vulkan/` in `examples/` and exported `src/*.ixx`; update the notes, README, AGENTS.md.

**Report:** steps completed, APIs added, deviations and why.

