# Discovery Report

## Scope

Read only:
- `AGENTS.md`
- `src/versions/simple/AGENTS.md`

No source files were copied, created, or modified.

## Files Marked For Copying From v5

The two allowed instruction files mark v5 items by subsystem name rather than by exact filename. I verified the actual `src/versions/v5` tree before resolving the entries below. Resolved source paths are real files on disk under `src/versions/v5`; unresolved entries are explicitly marked and must not be copied from an invented path.

| Marked v5 item | Source path under v5 | Intended destination path |
| --- | --- | --- |
| `Handle` helper class | `src/versions/v5/Handle.ixx` | `src/versions/simple/Handle.ixx` |
| `Vector` helper class | `src/versions/v5/Vector.ixx` | `src/versions/simple/Vector.ixx` |
| `Graph` helper class | `src/versions/v5/Graph.ixx` | `src/versions/simple/Graph.ixx` |
| `ECS` helper class | `src/versions/v5/ECS.ixx` | `src/versions/simple/ECS.ixx` |
| `Math` helper class | `src/versions/v5/Math.ixx` | `src/versions/simple/Math.ixx` |
| Scene graph material similar to v5 | `src/versions/v5/Assets.ixx` defines `SceneTree = Tree<NodeHandle>`, `Node`, and `Scene`; `src/versions/v5/Graph.ixx` defines the backing `Tree<THandle>` and `Graph<THandle>` used for parent/child node topology. | `src/versions/simple/SceneGraph.ixx` should be derived from those scene-tree pieces; reuse `src/versions/simple/Graph.ixx` for `Tree<THandle>`/`Graph<THandle>`. |
| Light shadow debug example from v5 | No dedicated light-shadow-debug example source exists under `src/versions/v5`: searched all `src/versions/v5/*.ixx` and found no v5-local example/test/demo directory. The concrete v5 light/shadow/debug source is `src/versions/v5/RenderSystem.ixx`, which defines `RenderDirectionalLight`, `RenderPointLight`, `RenderSpotLight`, `RenderDebugSample`, `RenderShadowDepthSample`, `RenderScene::setDirectionalLight`, `RenderScene::setPointLight`, `RenderScene::setSpotLight`, `RenderSystem::setDirectionalLight`, `RenderSystem::setPointLight`, `RenderSystem::setSpotLight`, `RenderSystem::sceneCpuDebugSample`, `RenderSystem::sceneGpuDebugSample`, `RenderSystem::sceneDebug*Error`, `RenderSystem::sceneShadowDepthSample`, `RenderSystem::sceneSpotShadowDepthSample`, and `RenderSystem::scenePointShadowDepthSample`; `RendererDescriptor::shadow_maps` is present but v5 sets it to `false`, and these debug/shadow functions are stubs returning zero counts or empty optionals. | No direct copy source; if needed later, derive `src/versions/simple/LightShadowDebug.ixx` from the `RenderSystem.ixx` symbols above. |

## Verified v5 Directory Listing

```text
src/versions/v5
src/versions/v5/AGENTS.md
src/versions/v5/Assets.ixx
src/versions/v5/CMakeLists.txt
src/versions/v5/ECS.ixx
src/versions/v5/Engine.ixx
src/versions/v5/Error.ixx
src/versions/v5/Graph.ixx
src/versions/v5/Gui.ixx
src/versions/v5/Handle.ixx
src/versions/v5/Math.ixx
src/versions/v5/RenderPass.ixx
src/versions/v5/RenderSystem.ixx
src/versions/v5/Resources.ixx
src/versions/v5/Shaders.ixx
src/versions/v5/Types.ixx
src/versions/v5/Vector.ixx
src/versions/v5/Window.ixx
```

## Notes

- `src/versions/simple/AGENTS.md` says to use v5 helper classes like `Handle`, `Vector`, `Graph`, `ECS`, and `Math` instead of reimplementing them.
- It also says to use a scene graph similar to v5 and the light shadow debug example from v5.
- The helper-class entries map cleanly to same-named `.ixx` files in the verified v5 listing.
- The scene graph and light shadow debug example entries do not have concrete matching filenames in the verified v5 listing, so they are unresolved for this tasklet.
