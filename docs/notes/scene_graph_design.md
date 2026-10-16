# Scene tree in VVE: minimal design

Status: implemented on top of commit 4b54ba1a ("Added scene hierarchy"), steps 1 and 2 of section 8, not yet
committed. The engine change touches the five files of section 5 plus `tests/SceneTreeTests.cpp`. With real
dependencies (Clang 23, libc++, Linux, no GPU), 34 of the 35 non-GPU tests pass, including the new `SceneTreeTests`.
Before the change 33 of 34 passed. The one failure in both runs is `WindowSetupDefaultsTests`, because the machine
has no Vulkan surface. The `hierarchy_car` node sequence, run without a window for 600 frames, gives
byte-identical object transforms before and after the change. The prototype `scene_graph_proto.cpp` (next to this note) is the earlier stand-alone version
of the same tree.

## 1. Decisions

1. The world is a single root node. It always exists, never moves and cannot be removed.
2. `keep_hierarchy` decides whether a loaded model joins the tree. Without it (the default) a model's pieces stay
   loose objects in the world, exactly as today.
3. Updating is one simple downward walk. When something changes, the engine walks down from the changed node and
   places every object below it. There are no dirty flags, no update pass per frame and no sorting.
4. The implementation uses as little code as possible and changes the current code as little as possible. Current
   examples and tests keep their behavior.

## 2. The tree

The render system replaces its per-model maps (`HierarchyNode`, `scene_hierarchies_`) with one tree:

```cpp
struct SceneNode {
    SceneNodeHandle parent{};                                   // invalid only for the world root
    Vector<SceneNodeHandle> children{};                         // in creation order
    Transform local{};                                          // relative to the parent
    Vector<std::pair<RenderObjectHandle, Transform>> objects{}; // objects hung here, with their offset
};
std::unordered_map<SceneNodeHandle, SceneNode, HandleHash<SceneNodeHandle>> scene_nodes_;
SceneNodeHandle scene_root_;                                    // created with the render system
std::map<RenderSceneInstanceHandle, std::unordered_map<NodeHandle, SceneNodeHandle, HandleHash<NodeHandle>>>
    instance_nodes_;                                            // keep_hierarchy models: asset node -> tree node
std::unordered_map<RenderObjectHandle, SceneNodeHandle, HandleHash<RenderObjectHandle>> object_nodes_;
```

`SceneNodeHandle` is one new public handle type in `src/Types.ixx`. World matrices are not stored in the tree.
Each render object keeps its `world_transform`, as today.

## 3. The update rule

There is a single function, a generalized version of today's `updateHierarchy`:

```cpp
void update(SceneNodeHandle node) {
    auto parent_world = identityMat4();                         // multiply the ancestors once
    for (auto up = scene_nodes_.at(node).parent; up.valid(); up = scene_nodes_.at(up).parent)
        parent_world = multiply(modelMatrix(scene_nodes_.at(up).local), parent_world);
    down(node, parent_world);                                   // then walk down
}
void down(SceneNodeHandle node, const Mat4 &parent_world) {
    const auto world = multiply(parent_world, modelMatrix(scene_nodes_.at(node).local));
    for (const auto &[object, offset] : scene_nodes_.at(node).objects)
        place(object, multiply(world, modelMatrix(offset)));   // world_transform + decomposed transform
    for (const auto child : scene_nodes_.at(node).children) down(child, world);
}
```

Every call that changes the tree ends with `update(node)` for the node it changed: setting a transform,
reparenting, hanging an object on a node, or instantiating a model. Results are therefore correct immediately, so
reading `objectTransform` right after setting a node gives the new value, which is what `hierarchy_car` checks every
frame.

One update pass per frame from the root was considered and rejected for two reasons. First, values read between
the change and the next frame would be one frame old, so the car example's check would fail. Second, a direct
`setObjectTransform` on a part would be overwritten every frame. The walk from the changed node costs as much as
the subtree below it. That is 19 node visits per frame in the car example, which is fine for this engine. Dirty
flags can be added later without changing the API.

## 4. Public API (facade `RenderSystem`)

| Call | Meaning |
|---|---|
| `sceneRoot()` | The world root node. |
| `createNode(parent, local = {})` | A new empty node, for example a pivot, a socket or a group. |
| `setNodeTransform(node, local)` / `nodeTransform(node)` | Local transform relative to the parent. The root returns `Error::invalid_argument`. |
| `setNodeParent(node, parent)` | Moves a node, with everything below it, under another parent. The local transform is kept. Loops and the root return `Error::invalid_argument`. |
| `nodeParent(node)` / `nodeChildren(node)` | Walk the tree. Children come back in creation order. |
| `removeNode(node)` | Removes the node, everything below it, and the objects hung there. |
| `sceneInstanceRoot(instance)` | The node above a `keep_hierarchy` model; move it to place the whole model. |
| `sceneInstanceNode(instance, asset_node)` | The tree node copied from an asset node, so names from `AssetSystem::nodeName` can be used. |
| `setObjectParent(object, node)` | Hangs any render object on a node, including primitives. Its current transform becomes its offset. An empty `node` takes it out of the tree again. |

`instantiateScene(scene, {.keep_hierarchy = true})` creates an instance root under `sceneRoot()` and copies the
asset tree below it. Calling it again creates another copy. That covers cloning a model. To put a car on a trailer,
call `setNodeParent(sceneInstanceRoot(car), trailer)`. The two calls `sceneInstanceRoot` and `sceneInstanceNode`
return `Error::invalid_argument` for a model loaded without the flag.

`setSceneInstanceNodeTransform(instance, asset_node, local)` and `sceneInstanceNodeTransform` stay as two-line
wrappers over `sceneInstanceNode` plus `setNodeTransform` / `nodeTransform`, with their current errors.

## 5. What changes in the code

| File | Change |
|---|---|
| `src/Types.ixx` | Add `SceneNodeHandleTag` and `SceneNodeHandle` (2 lines). |
| `src/RenderSystem.ixx` | Forward the calls from section 4 (about 40 lines). |
| `src/versions/simple/Render/RenderSystem.ixx` | Replace `HierarchyNode`, `Hierarchy`, `scene_hierarchies_` and `updateHierarchy` with the members from section 2, and declare the calls (about 25 lines). Create the root node in the constructor. |
| `src/versions/simple/Render/RenderSceneImport.cpp` | With `keep_hierarchy`, `instantiateScene` creates the instance root and one tree node per asset node, and hangs the objects with an identity offset. It replaces about 12 lines. `updateHierarchy` becomes `update`/`down`, the two old calls become wrappers, and the new calls are added (about 90 lines). |
| `src/versions/simple/Render/RenderSystemObjects.cpp` | `removeObject` takes the object off its node (`object_nodes_`). `rollbackSceneInstance` removes the instance root's subtree. `clearScene` empties the tree and keeps the same root (about 10 changed lines). |
| `tests/SceneTreeTests.cpp` + `tests/CMakeLists.txt` | The prototype checks from section 6 (about 100 lines, 1 line). |

That is about 170 lines of engine code and 100 lines of tests. `Graph.ixx`, `Assets.ixx`, the renderer and all
examples stay unchanged.

## 6. Current behavior is kept

Models loaded without `keep_hierarchy` never touch the tree. `game`, `testscene`, `sponza`, `light_shadow_debug`,
`postprocessing` and `simple_forward_demo` work exactly as now.

For `keep_hierarchy` models, the update is today's algorithm. The only additions are two identity nodes on top,
the world root and the instance root, and multiplying by the identity is exact. The prototype ran the exact
`hierarchy_car` call sequence on both versions for 600 frames, and all 10 object matrices were bit-identical in
every frame. `hierarchy_car` keeps setting its "Car" node directly.

A direct `setObjectTransform` on a part still holds until its node or an ancestor is set again. The prototype checks
both halves: the placement survives a sibling move and is replaced when an ancestor is set. A model loaded without
the flag still answers `Error::invalid_argument` to the old calls, because it has no tree nodes.
`SceneHierarchyTests` and the `HierarchyCarExample` CTest must pass unchanged.

The prototype also checks the new parts. The world root cannot move or be removed. A car hung on a trailer follows
the trailer. Loops are rejected. A cube hung on a mount with a 1 m offset is placed by the node plus the offset.
Removing a node reports the objects that must be deleted.

## 7. Left out on purpose

None of these is needed now, and each can be added later without changing the calls above:

- copying a part of the tree at runtime (load the model again instead);
- hiding a whole branch at once;
- reparenting that keeps the world position;
- lights and cameras on nodes (a camera can already follow a part by reading `objectTransform` of an object hung
  on it each frame);
- dirty flags for large trees.

## 8. Steps

1. **Tree inside the render system, no new public calls.** Swap the per-model maps for the tree. Instantiation,
   the two old calls, removal and `clearScene` use it. All existing tests and `hierarchy_car` must pass unchanged.
2. **Public API.** Add `SceneNodeHandle` and the calls from section 4, plus `SceneTreeTests`.
3. **Docs.** Update the README, both AGENTS files, `docs/notes/architecture.md`, guide pages 16, 31, 61-63 and 75,
   and quickstart page 7.
