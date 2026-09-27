# Simple Engine Asset-To-Render Boundary Map

Discovery date: 2026-07-02. Guidance re-read first: `AGENTS.md` and `src/versions/simple/AGENTS.md`.

> **Status (current code):** This is the discovery record written before scene instantiation existed. It
> has since been built: the facade `vve::RenderSystem` has `instantiateScene`, `removeSceneInstance`,
> `sceneInstanceObjects`, `objectSourceScene` and `objectSourceNode`, and `RenderSceneInstanceHandle` is
> declared in `src/Types.ixx`. The simple render code now lives in `src/versions/simple/Render/`
> (`RenderSystem.ixx`, `RenderResources.ixx`, `RenderSceneImport.cpp`, `RenderSystemObjects.cpp`,
> `RenderSystemScene.cpp`). The notes below mark the other passages that no longer match the code.

## Public Facade Surface

`src/Assets.ixx` declares `vve::AssetSystem`. Current public asset methods are:

```cpp
[[nodiscard]] auto addScene(ObjectName name)									-> std::expected<SceneHandle, Error>;
[[nodiscard]] auto loadScene(const std::filesystem::path &source)		-> std::expected<SceneHandle, Error>;
[[nodiscard]] inline std::expected<SceneHandle, Error> loadScene(const std::filesystem::path &source,
																												 const SceneLoadOptions &options);
[[nodiscard]] bool containsScene(SceneHandle scene) const;
[[nodiscard]] auto sceneName(SceneHandle scene) const						-> std::expected<ObjectName, Error>;
[[nodiscard]] auto sceneNodeCount(SceneHandle scene) const				-> std::expected<std::size_t, Error>;
[[nodiscard]] auto sceneMeshCount(SceneHandle scene) const				-> std::expected<std::size_t, Error>;
[[nodiscard]] auto sceneMaterialCount(SceneHandle scene) const			-> std::expected<std::size_t, Error>;
[[nodiscard]] auto sceneTextureCount(SceneHandle scene) const			-> std::expected<std::size_t, Error>;
[[nodiscard]] auto sceneLightCount(SceneHandle scene) const				-> std::expected<std::size_t, Error>;
[[nodiscard]] auto sceneCameraCount(SceneHandle scene) const			-> std::expected<std::size_t, Error>;
[[nodiscard]] auto sceneRootNode(SceneHandle scene) const				-> std::expected<NodeHandle, Error>;
[[nodiscard]] auto sceneNodes(SceneHandle scene) const					-> std::expected<Vector<NodeHandle>, Error>;
[[nodiscard]] auto sceneMeshes(SceneHandle scene) const					-> std::expected<Vector<MeshHandle>, Error>;
[[nodiscard]] auto sceneMaterials(SceneHandle scene) const				-> std::expected<Vector<MaterialHandle>, Error>;
[[nodiscard]] auto sceneTextures(SceneHandle scene) const				-> std::expected<Vector<TextureHandle>, Error>;
[[nodiscard]] auto sceneLights(SceneHandle scene) const					-> std::expected<Vector<LightHandle>, Error>;
[[nodiscard]] auto sceneCameras(SceneHandle scene) const					-> std::expected<Vector<CameraHandle>, Error>;
[[nodiscard]] std::expected<Vector<NodeHandle>, Error> sceneNodeChildren(SceneHandle scene,
																										NodeHandle node) const;
[[nodiscard]] std::expected<std::optional<NodeHandle>, Error> sceneNodeParent(SceneHandle scene,
																											NodeHandle node) const;
[[nodiscard]] std::expected<ObjectName, Error> nodeName(NodeHandle node) const;
[[nodiscard]] auto nodeTransform(NodeHandle node) const					-> std::expected<Transform, Error>;
[[nodiscard]] auto nodeMeshes(NodeHandle node) const						-> std::expected<Vector<MeshHandle>, Error>;
[[nodiscard]] auto nodeMaterials(NodeHandle node) const					-> std::expected<Vector<MaterialHandle>, Error>;
[[nodiscard]] std::expected<ObjectName, Error> meshName(MeshHandle mesh) const;
[[nodiscard]] auto meshVertexCount(MeshHandle mesh) const				-> std::expected<VertexCount, Error>;
[[nodiscard]] auto meshIndexCount(MeshHandle mesh) const					-> std::expected<IndexCount, Error>;
[[nodiscard]] auto meshMaterial(MeshHandle mesh) const					-> std::expected<MaterialHandle, Error>;
[[nodiscard]] std::expected<Bounds, Error> meshBounds(MeshHandle mesh) const;
[[nodiscard]] auto meshPositions(MeshHandle mesh) const					-> std::expected<Vector<Vec3>, Error>;
[[nodiscard]] auto meshNormals(MeshHandle mesh) const						-> std::expected<Vector<Vec3>, Error>;
[[nodiscard]] auto meshTexcoords(MeshHandle mesh) const					-> std::expected<Vector<Vec2>, Error>;
[[nodiscard]] auto meshIndices(MeshHandle mesh) const						-> std::expected<Vector<std::uint32_t>, Error>;
[[nodiscard]] auto materialName(MaterialHandle material) const			-> std::expected<ObjectName, Error>;
[[nodiscard]] auto materialTextures(MaterialHandle material) const	-> std::expected<Vector<TextureHandle>, Error>;
```

> **Status (current code):** the `loadScene(path, const SceneLoadOptions &)` overload and `SceneLoadOptions`
> were removed; `lightData(LightHandle)` and `cameraData(CameraHandle)` were added.

`src/RenderSystem.ixx` declares `vve::RenderSystem`. Current public render methods relevant to assets/render objects are:

```cpp
auto clearScene()																													-> void;
[[nodiscard]] auto loadSampleScene()																						-> std::expected<void, Error>;
auto setCamera(Camera camera, PixelExtent extent)																		-> void;
void setDirectionalLight(Direction direction_to_light, LinearColor color,
									LightIntensity intensity, LinearColor ambient);
void addDirectionalLight(Direction direction_to_light, LinearColor color,
									LightIntensity intensity, LinearColor ambient);
auto setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range)	-> void;
auto setPointLight(Position position, LinearColor color, LightIntensity intensity,
						 LightRange range, LinearColor ambient)															-> void;
void addPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range);
void addPointLight(Position position, LinearColor color, LightIntensity intensity,
						 LightRange range, LinearColor ambient);
void setSpotLight(Position position, Direction direction, LinearColor color,
						LightIntensity intensity, LightRange range, SpotConeAngle cone);
void setSpotLight(Position position, Direction direction, LinearColor color,
						LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient);
void addSpotLight(Position position, Direction direction, LinearColor color,
						LightIntensity intensity, LightRange range, SpotConeAngle cone);
void addSpotLight(Position position, Direction direction, LinearColor color,
						LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient);
[[nodiscard]] std::expected<RenderObjectHandle, Error> addPlane(Vec2 half_extent, LinearColor color,
																				 Transform transform = {});
[[nodiscard]] std::expected<RenderObjectHandle, Error> addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color,
																				 Transform transform = {});
[[nodiscard]] std::expected<RenderObjectHandle, Error> addTexturedCuboid(Vec3 minimum, Vec3 maximum,
																							  std::filesystem::path base_color_texture,
																							  Transform transform = {});
[[nodiscard]] auto removeObject(RenderObjectHandle handle)															-> std::expected<void, Error>;
[[nodiscard]] auto setObjectVisible(RenderObjectHandle handle, bool visible)							-> std::expected<void, Error>;
[[nodiscard]] auto objectVisible(RenderObjectHandle handle) const										-> std::expected<bool, Error>;
[[nodiscard]] auto setObjectTransform(RenderObjectHandle handle, Transform transform)			-> std::expected<void, Error>;
[[nodiscard]] auto objectTransform(RenderObjectHandle handle) const									-> std::expected<Transform, Error>;
[[nodiscard]] auto removeScene(SceneHandle handle)															-> std::expected<void, Error>;
[[nodiscard]] auto purgeUnusedAssets()																				-> std::size_t;
[[nodiscard]] auto sceneMeshCount() const																					-> std::size_t;
[[nodiscard]] auto sceneMaterialCount() const																			-> std::size_t;
[[nodiscard]] auto sceneInstanceCount() const																			-> std::size_t;
[[nodiscard]] auto sceneVertexCount() const																				-> std::size_t;
[[nodiscard]] auto sceneIndexCount() const																				-> std::size_t;
```

There is no public `RenderSystem::instantiateScene`, `addScene`, `sceneInstanceObjects`, `objectSourceScene`, or `objectSourceNode` facade operation yet.

> **Status (current code):** all of these except `addScene` now exist on `vve::RenderSystem`, together with
> `removeSceneInstance`, `addTriangleMesh`, `setObjectMeshPositions`, `setObjectCastsShadow`, `setObjectUnlit`
> and per-kind light and camera counts. The first parameter of `setDirectionalLight` / `addDirectionalLight`
> is now named `direction`: the direction the light travels (light toward the scene).

## Public Handles

`src/Types.ixx` declares the public handle tags and aliases:

```cpp
struct SceneHandleTag {};
struct NodeHandleTag {};
struct MeshHandleTag {};
struct MaterialHandleTag {};
struct TextureHandleTag {};
struct RenderObjectHandleTag {};
struct LightHandleTag {};
struct CameraHandleTag {};

using SceneHandle	= TypedHandle<SceneHandleTag>;
using NodeHandle	= TypedHandle<NodeHandleTag>;
using MeshHandle	= TypedHandle<MeshHandleTag>;
using MaterialHandle = TypedHandle<MaterialHandleTag>;
using TextureHandle	= TypedHandle<TextureHandleTag>;
using RenderObjectHandle = TypedHandle<RenderObjectHandleTag>;
using LightHandle	= TypedHandle<LightHandleTag>;
using CameraHandle	= TypedHandle<CameraHandleTag>;
```

`src/versions/simple/Types.ixx` aliases these into `vve::simple`. No `RenderSceneInstanceHandle` or equivalent public facade handle currently exists.

> **Status (current code):** `src/Types.ixx` now also declares `RenderSceneInstanceHandle` (and `WindowHandle`).
> `src/versions/simple/Types.ixx` no longer aliases the handles: `vve::simple` is nested in `vve`, so they are
> found by ordinary lookup.

## Asset Implementation State

`src/versions/simple/Assets.ixx` owns imported descriptors in `vve::simple::AssetSystem::catalog_`. Internal types are:

- `Table<T>`: `std::map<Handle, T> data`.
- `Node`: `NodeHandle handle`, `ObjectName name`, local `Transform transform`, `Vector<MeshHandle> meshes`, `Vector<MaterialHandle> materials`.
- `Mesh`: `MeshHandle handle`, `ObjectName name`, counts, `MaterialHandle material`, `Bounds bounds`, `Vector<Vec3> positions`, `normals`, `Vector<Vec2> texcoords`, `Vector<std::uint32_t> indices`.
- `Material`: `MaterialHandle handle`, `ObjectName name`, `Vector<TextureHandle> textures`.
- `Scene`: `SceneHandle handle`, `ObjectName name`, `SceneTree tree`, and vectors of node, mesh, material, texture, light, and camera handles.
- `Catalog`: tables for scenes, nodes, meshes, materials only.

There are no texture, light, or camera descriptor tables. Textures are deduplicated by path during material import, but only `TextureHandle`s are retained; the normalized texture path is not stored after import. Lights and cameras are imported only as generated `LightHandle`/`CameraHandle` lists.

> **Status (current code):** the descriptors are now `AssetScene`, `AssetMesh` (with tangents), `Material`
> (base-color factor, `MaterialFactors`, and one `MaterialTextureSource` per semantic holding the canonical
> path or the embedded image data) and the new `Light` / `CameraAsset` tables with world-space
> `LightDescriptor` / `CameraDescriptor` data. There is still no texture table. The Assimp flags and the
> `import()` sequence below are unchanged.

`AssetSystem::loadScene(const std::filesystem::path &source)` normalizes the path, calls Assimp with `aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_ImproveCacheLocality`, rejects empty paths or missing root nodes, then calls `import(catalog_, *scene, path)`.

`import()` calls `materials()`, `meshes()`, `lights()`, `cameras()`, creates a `Scene`, recursively imports the root node via `node()`, stores each descriptor in the catalog tables, and finally adds the scene to `catalog_.scenes`.

## Render Implementation State

`src/versions/simple/RenderSystem.ixx` defines render-side resource and object types:

- `RenderMesh`, `RenderMaterial`, `RenderInstance`, `RenderDirectionalLight`, `RenderPointLight`, `RenderSpotLight`, `RenderCamera`, `RenderResource`, `RenderFunction`.
- Internal render handles: `RenderMeshHandle`, `RenderMaterialHandle`, `RenderInstanceHandle`, `RenderResourceHandle`, `RenderFunctionHandle`.
- `RenderScene` stores `Vector<RenderMesh> meshes_`, `Vector<RenderMaterial> materials_`, `Vector<RenderInstance> instances_`, optional camera and light lists.
- `RenderSystem` stores `RenderScene scene_`, selected renderer backend, `render_objects_` mapping public `RenderObjectHandle` to `(RenderInstanceHandle, backend_index)`, `std::map<SceneHandle, Scene> scenes_`, and `active_scene_`.

Renderable public objects are currently created only by `addPlane`, `addCuboid`, and `addTexturedCuboid`. Each call creates or appends a render material and mesh in `scene_`, creates a `RenderInstance`, mirrors it to the backend with `appendBackendObject()`, then registers a public `RenderObjectHandle`. Visibility and transforms update both `scene_` and `forward().scene.objects[backend_index]`.

`RenderSystem::loadScene(vve::simple::Scene scene) -> SceneHandle` is implementation-only and takes the renderer CPU scene type, not an asset scene handle. It stores the scene in `scenes_`, sets `active_scene_`, and forwards it to the backend. It does not create public `RenderObjectHandle`s and is not exposed by `vve::RenderSystem`.

`removeObject()` erases one public object mapping and its corresponding `RenderScene` instance/backend object. `removeScene(SceneHandle)` removes a backend-loaded simple `Scene` only when `render_objects_` is empty. `purgeUnusedAssets()` removes `RenderScene` mesh/material resources no remaining live instance references.

> **Status (current code):** the render types are in `src/versions/simple/Render/RenderResources.ixx` (module
> `VEEngine.Simple.RenderResources`) and `RenderSystem` is in `Render/RenderSystem.ixx`; `RenderResource`,
> `RenderFunction` and their handles are gone. `RenderScene` also owns the texture table
> (`std::deque<RenderTexture>`). `render_objects_` maps to a `RenderInstanceHandle` only: there is no backend
> object list and no `appendBackendObject`, because the renderer reads the `RenderScene` directly.
> Public objects also come from `addTriangleMesh` and `instantiateScene`. `scenes_` is a `std::set<SceneHandle>`:
> `loadScene(Scene)` replaces only the lights, and `removeScene` on such a handle removes exactly those lights;
> for an asset scene it fails with `invalid_argument` while an instance of it exists, otherwise it drops the
> scene's cached render meshes/materials and purges. `purgeUnusedAssets()` also releases unreferenced textures.

## Existing Tests And Build

Tests are enabled from the root `CMakeLists.txt` with `include(CTest)`, added by `tests/CMakeLists.txt`, and built by the normal presets. The simple engine is forced in `src/CMakeLists.txt` via `VVE_ENGINE_IMPLEMENTATION_NAMESPACE=simple`. `src/versions/simple/CMakeLists.txt` adds the simple modules and compiles Slang shaders through the `vve_simple_shaders` custom target.

Relevant current tests:

- `tests/SceneSystemTests.cpp`: writes a temporary OBJ, calls facade `AssetSystem::loadScene`, and checks scene/node/mesh/material counts, root/parent data, mesh bounds, positions, normals, texcoords, and indices.
- `tests/ImplementationTests.cpp`: `testAssimpSceneImport()` covers the same facade asset import path more broadly, including empty texture/light/camera lists for the generated OBJ.
- `tests/SimpleForwardRendererTests.cpp`: covers facade render object lifetime, backend visibility/transform mirroring, purge of unused render mesh/material resources, and implementation-only `RenderSystem::loadScene(vve::simple::Scene)` removal behavior.
- `tests/RenderSystemTests.cpp`: primarily targets v4/v5-style implementation APIs and render scene primitives; it is not the main simple facade asset-to-render bridge test.

> **Status (current code):** `tests/ImplementationTests.cpp` (never registered in CMake) was deleted, and
> `tests/RenderSystemTests.cpp` was removed with the retired v3/v4/v5 engines. Scene instantiation is covered by
> `SceneInstantiationTests`, `SceneRemovalTests`, `SceneAssetRemovalBlockedTests`, `ScenePurgeUnusedAssetsTests`,
> `SceneTransformCompositionTests`, `RenderSceneLightApplyTests` and `RenderSceneCameraApplyTests`; imported
> light and camera data by `AssetLightDataTests` and `AssetCameraDataTests`.

Configured build/test command required by the task:

```sh
cmake --preset 'debug-macos-arm64-llvm' && cmake --build --preset 'build-debug-macos-arm64-llvm'
```
