# Task: Fix model loading in the simple engine — full material/texture import, single-copy mesh & texture storage

Work only inside this repository. The public facade API in `src/*.ixx` (namespace `vve`) must not change; all work happens in `src/versions/simple/` (including `shaders/simple_forward.slang`) plus tests and, if needed, small additions to the facade that are purely additive. Follow the repository CLAUDE.md guidelines: surgical changes, no speculative features, `std::expected<T, Error>` error handling, match the existing module and comment style.

## Current behavior (verified, with locations)

1. `AssetSystem::loadScene` (src/versions/simple/Assets.ixx:566) imports scenes via Assimp with `aiProcess_Triangulate | JoinIdenticalVertices | GenSmoothNormals | CalcTangentSpace | ImproveCacheLocality`. The catalog already records per-material texture paths for many semantics (`texture_types` at Assets.ixx:194: diffuse, base color, normals, height, roughness, metalness, emissive, AO, lightmap), plus per-mesh positions, normals, texcoords, indices.
2. The render side throws almost all of that away:
   - `RenderSystem::acquireRenderMaterial` (src/versions/simple/Render/RenderSceneImport.cpp:152) always creates a default opaque-white material and never queries `imported_assets_.material_textures`. Imported models render untextured.
   - `RenderSystem::appendBackendObject` (src/versions/simple/Render/RenderSystemScene.cpp:244) copies every `RenderMesh` (`RenderVertex` = position/normal/uv) into a per-object backend `Object.mesh` (`Vertex` = position/color/texCoord, Mesh.ixx:16), dropping normals and baking the material color into every vertex. Mesh sharing achieved by `imported_render_meshes_` caching (RenderSceneImport.cpp:114) is lost here: N instances of one mesh become N full CPU copies.
   - The GPU side (`RendererResources.cpp:159` and `syncSceneResources` at :218) creates one `VulkanMesh` (own vertex+index buffer) per `scene.objects` entry — N instances = N GPU buffer pairs. `drawUploadedObjects` (RendererDraw.cpp:156) binds buffers per object and draws with instanceCount 1.
   - Textures exist only for the `addTexturedCuboid` path: base color only, deduplicated by path into `Scene::textures`, capped at `kMaxSceneTextures = 8` (Scene.ixx:21), bound as a fixed sampler array (slang binding 1), selected by `ObjectPushConstants.baseColorTextureIndex`.
3. The shader (src/versions/simple/shaders/simple_forward.slang) has no normal input — it shades from interpolated vertex color and world position. Constants (`kMaxSceneTextures`, light caps, shadow layout) are hand-mirrored between Scene.ixx, Vulkan/Pipeline.ixx, and the shader.

## Goal

After this task:

A. **Full texture import**: every texture/map Assimp reports for a material (base color/diffuse, normal, metalness, roughness, emissive, ambient occlusion — the semantics already listed in `texture_types`) is loaded from disk, associated with the render material of each object created by `instantiateScene`, and consumed by the forward shader when present. Objects whose material lacks a given map must render correctly with sensible fallbacks (white base color, flat normal, roughness 1 / metalness 0, no emissive, AO 1).

B. **Single-copy storage with indexing**:
   - Each unique mesh exists exactly once on the CPU (`RenderMesh` in `RenderScene`) and exactly once on the GPU (one `VulkanMesh` per unique `RenderMesh`, not per object/instance). Instances reference their mesh by index/handle; per-instance data (model matrix, material index, flags) travels via push constants or a per-instance buffer. Rendering the same mesh K times must not create K vertex/index buffers.
   - Each unique texture file (identified by canonical absolute path) is decoded once and uploaded to the GPU once, whatever its semantic and however many materials reference it. Materials store indices into one shared texture table.
   - Remove the backend `Object`/`Mesh`/`Vertex` duplication: the renderer must draw directly from `RenderScene` data (meshes, materials, instances). Delete `appendBackendObject`'s vertex copying and the `Vertex` format, or reduce them to a thin upload view. The GPU vertex format must carry position, normal, and uv (add tangent only if you implement normal mapping, which is required — Assimp already computes tangents via `CalcTangentSpace`, so extend `AssetMesh`/`RenderVertex` to carry them through).

C. **Shader**: extend `simple_forward.slang` to sample the maps that exist. Per-material data (base color factor + texture indices per semantic, or a "no texture" sentinel) should live in a material array (uniform or storage buffer) indexed by a per-object/per-instance material index; keep the existing shadow/lighting pipeline working. Use the imported normals (and normal maps via TBN) for lighting instead of the current derivative/color-only shading where applicable. Raise or replace the `kMaxSceneTextures = 8` cap (a larger fixed cap, e.g. 64, with default-texture fill is acceptable; descriptor-indexing/bindless is not required). Any constant shared between C++ and Slang must be defined once and mirrored mechanically (generated header via CMake `configure_file`, or a clearly single-sourced include) — do not add new hand-mirrored constants.

## Constraints

- Do not change public facade signatures in `src/*.ixx`; additive facade accessors (e.g. a GPU-mesh count for tests) are allowed following the existing wrapper pattern (declare in facade, forward in the matching `src/*.cpp`).
- Keep `ImportedAssetReadAccess` as the only channel from renderer to assets; extend it (e.g. typed material-texture query returning semantic + path) rather than letting the renderer include Assimp.
- Keep all existing examples building and running: `testscene`, `sponza`, `game`, `postprocessing`, `simple_forward_demo`, `light_shadow_debug`, `physics`. The `addPlane`/`addCuboid`/`addTexturedCuboid`/`addTriangleMesh` and `setObjectMeshPositions`/`removeObject`/`setObjectTransform`/`setObjectVisible` paths must keep working on top of the deduplicated storage (removal must not require renumbering every live handle — key GPU meshes by `RenderMeshHandle`, not dense index).
- Keep the existing dirty-tracking idea (only upload what changed); a removed instance must not force re-upload of unrelated meshes.
- No new third-party dependencies. stb (already present) decodes images.
- Update every test that encodes the old behavior; keep the whole suite green.

## Suggested phase order (each phase must build and pass tests before the next)

1. **Texture pipeline groundwork**: texture table in `RenderScene` (unique canonical path → `RenderTexture` entry with semantic-agnostic pixel data reference), GPU texture cache keyed by that table, dedup test.
2. **Material import**: extend `RenderMaterial` with per-semantic texture indices + factors; make `acquireRenderMaterial` read the imported material via the asset callbacks; cache per imported `MaterialHandle`. Test: instantiating a scene twice creates each texture once.
3. **Mesh dedup / renderer reads RenderScene**: one `VulkanMesh` per `RenderMesh`, instances draw by reference; delete the backend copy layer; keep normals/tangents in the GPU vertex. Test: K instances of one mesh → 1 GPU mesh; vertex/index counts unchanged; existing scene tests pass.
4. **Shader**: material buffer + texture array sampling + normal-mapped lighting with fallbacks; regenerate SPIR-V through the existing Slang build step. Test: `captureFrameToPng` of the sponza example is not the uniform white/flat result (compare against a reference or assert pixel variance), shadow tests still pass.
5. **Cleanup**: remove now-dead code (`Vertex`, `makeCube`/`makePlane` if unused, `uploadedTextures_` style shadow copies), single-source shared constants.

## Verification (run after every phase)

- Configure/build the Linux debug preset exactly as `build_linux.sh` / `CMakePresets.json` does; fix warnings you introduce.
- Run the full ctest suite from the build directory; all tests green.
- Run `bin/debug/exe/sponza --frames 120` (asset root auto-discovery is built in) and `testscene --frames 120`; both must exit 0. Use the render-system statistics accessors (`sceneMeshCount`, `sceneVertexCount`, new GPU-mesh/texture counts) in tests to prove dedup rather than eyeballing.
- Report at the end: LOC delta, list of deleted duplicate structures, measured mesh/texture counts for sponza before vs. after.
