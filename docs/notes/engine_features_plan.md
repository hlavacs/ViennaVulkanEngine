# Engine features plan: transparency, sky, input, shapes, picking and more

Hand-off document for implementing ten small engine features in VVE V3. It is self-contained: read it, then
`AGENTS.md`, `src/versions/simple/AGENTS.md` and `CLAUDE.md`, and work through the steps in section 4 one at a
time. Line numbers refer to commit 4b54ba1a ("Added scene hierarchy") and may have moved; search for the named
symbols.

## 1. Goal and rules

Add the features a simple game needs, with the least code that does the job (about 700 engine lines and 600 test
lines in total). Fancy lighting is out of scope.

Rules for every step:

1. Only add to the public facade. Never change existing signatures, defaults or return types. New struct fields go
   at the end with defaults. No new overload that could make an existing call ambiguous (tests call
   `addTriangleMesh` with brace lists). All existing examples and tests must compile and pass unchanged.
2. Defaults keep today's behavior: the background stays the current light blue, and there is no skybox until set.
3. Follow `CLAUDE.md`: simplest design, surgical changes, match the existing style (`/// @brief` comments,
   `std::expected<..., Error>` returns, facade forwarders in `src/*.ixx`, implementation in `src/versions/simple`).
4. One commit per step, each with its tests, README and AGENTS updates. Keep the "left out" items out.
5. Build and test on Linux after every step:
   `bash build_linux.sh debug`, then `ctest --test-dir build/debug-linux -LE gpu --output-on-failure`, and the GPU
   tests too when a GPU is available. The script uses SDL's offscreen video driver when there is no display.
6. Do not count the new blended pipeline in `forwardPipelineCreateCount()`; `MultiWindowRenderTests` and
   `RenderFrameCountTests` expect 1.

## 2. Spare room the engine already has (reuse it)

- `ObjectPushConstants` (shader `src/versions/simple/shaders/simple_forward.slang:51`, C++ mirror
  `src/versions/simple/Vulkan/Pipeline.ixx:54`) has `uint unlit` (only 0/1 used) and an unused `uint padding`.
  Turn `unlit` into `flags` (bit 0 unlit, bit 1 sky, bit 2 sprite) and `padding` into `float opacity`. The block
  size stays the same. The value is filled in `RendererDraw.cpp` (`items.push_back`, about line 263).
- `GpuMaterial` (shader line 60, C++ `Pipeline.ixx:39`, filled in `RendererResources.cpp` about line 435) leaves
  `baseColorFactor.w` (always 1) and `emissiveFactor.w` (always 0) unused. Use them for material opacity and
  alpha mode (0 opaque, 1 cut-out, 2 blended).
- `VulkanGraphicsPipeline::create` (`Pipeline.ixx` about line 252) is one parameterized function with blending off
  and depth writes on. Add a `bool blended = false` parameter (blend src-alpha / one-minus-src-alpha, depth write
  off) and a second pipeline member `transparentPipeline` next to `graphicsPipeline` (`Render/Renderer.ixx:127`),
  created and cleaned up in the same places.
- The renderer uploads every live entry of the texture table, so a panorama or a HUD image can go through
  `RenderScene::acquireTexture`.
- Mouse motion already uses SDL's relative values (`event.motion.xrel`, `src/versions/simple/Window.ixx` about
  line 409), so capturing the mouse needs one SDL call. Keys are stored as SDL key codes in `InputState`, so mouse
  buttons can use reserved negative codes and the existing `isKeyDown` / `wasKeyPressed` work unchanged.
- The background is the constant `skyBackgroundColor` in `Render/RendererDraw.cpp:368`.

## 3. Shared groundwork (do it in step 1)

1. Push constants: `flags` and `opacity` as above; `RenderInstance` (`Render/RenderResources.ixx:54`) gets
   `Scalar opacity{1}` and `bool sky{}`, `bool sprite{}` next to `unlit`.
2. Blended pipeline as above.
3. Draw order in `RendererDraw.cpp`: the `DrawItem` list (built about line 255) gets a `transparent` flag
   (material mode 2 or `opacity < 1` or sprite) and the distance to the camera. Draw solid items as today, then the
   sky item, then bind `transparentPipeline` and draw transparent items sorted far to near. The shadow pass skips
   transparent, sky and sprite items. Sky items skip frustum culling.

## 4. Steps

Each step lists the public calls, the design, what is left out, and a size budget (engine lines / test lines).

### Step 1: transparency (with the shared groundwork) — 120 / 120

- Calls: `RenderSystem::setObjectOpacity(RenderObjectHandle, Scalar) -> std::expected<void, Error>` and
  `objectOpacity(RenderObjectHandle) const -> std::expected<Scalar, Error>`.
- Import: read `AI_MATKEY_GLTF_ALPHAMODE` ("OPAQUE", "MASK", "BLEND") and `AI_MATKEY_OPACITY` /
  the base colour alpha (read in `src/versions/simple/Assets.ixx`) into new optional fields of `MaterialFactors`
  (`src/versions/simple/Types.ixx:60`), copy
  them into new `RenderMaterial` fields `opacity` and `alpha_mode` in `RenderSystem::acquireRenderMaterial`, and
  write them to the two spare `GpuMaterial` slots.
- Shader: `alpha = baseColorFactor.a * object.opacity * texture alpha` (texture alpha only when a base-colour
  texture exists). Cut-out discards below 0.5. Write `alpha` to the output in both the lit and the unlit path.
- Left out: cut-out objects casting leaf-shaped shadows (they cast solid shadows); custom cut-off values.
- Tests: CPU round trip and errors for opacity; a glTF fixture with one OPAQUE, one MASK and one BLEND material;
  a GPU capture where a 50 % blue plane in front of a red plane gives a mixed colour.
- Note: `assets/sea_keep_lonely_watcher/scene.gltf` uses alpha modes, so `testscene` will now show holes and
  see-through parts. That is intended.

### Step 2: background colour — 20 / 30

- Calls: `setBackgroundColor(LinearColor)`, `backgroundColor() const`.
- Replace the constant with a renderer member that has the same default.
- Tests: GPU capture of an empty scene returns the set colour.

### Step 3: skybox from one panorama — 90 / 80

- Calls: `setSkybox(const std::filesystem::path &panorama) -> std::expected<void, Error>` (a 2:1
  equirectangular image) and `clearSkybox()`.
- Design: acquire the texture, create one plain material with it, an inward-facing unit cube mesh, and one internal
  `RenderInstance` with `sky = true` and no shadow. It is not a public render object.
- Vertex shader for sky items: rotate the position by the view rotation only (no translation), project, and set
  `z = 0.999999 * w` so the sky lies just inside the far plane. Pass the object-space position on.
- Fragment shader for sky items: `d = normalize(position)`, `u = atan2(d.x, -d.z) / (2 pi) + 0.5`,
  `v = acos(clamp(d.y, -1, 1)) / pi`, sample with `SampleLevel(uv, 0)` (no mips avoids a seam), output unlit.
- `clearScene` also removes the sky. Each window shows its own camera's view of it.
- Left out: six-image input (can be added later by stitching six images into one panorama on load).
- Tests: CPU: missing file gives `io_error`; set and clear change `sceneTextureCount`. GPU: a panorama with a red
  top half and a green bottom half shows red when looking up and green when looking down.

### Step 4: sprites — 50 / 40 (needs step 1)

- Call: `addSprite(Vec2 size, std::filesystem::path texture, LinearColor color = white, Transform transform = {})`.
- Design: a textured quad in the XY plane, blended, with `sprite = true`. The vertex shader keeps the object's
  world position and builds the quad from the camera's right and up vectors (first two rows of the view
  rotation), so it faces each window's camera. Frustum culling uses a box that covers the quad in every direction.
- Particles are many sprites moved by game code.
- Left out: a particle emitter, GPU instancing.
- Tests: CPU creation and bounds; GPU capture from two camera angles shows the same coverage.

### Step 5: input — 55 / 40

- Named keys added to `vve::Key` (`src/Window.ixx:74`), SDL key codes: `space = 32`, `enter = 13`, `tab = 9`,
  `backspace = 8`, `delete_key = 127`, `left_ctrl = 1073742048`, `right_ctrl = 1073742052`,
  `left_alt = 1073742050`, `right_alt = 1073742054`, `f1 = 1073741882` up to `f12 = 1073741893`,
  `insert = 1073741897`, `home = 1073741898`, `page_up = 1073741899`, `end = 1073741901`, `page_down = 1073741902`.
- `constexpr Key key(char c)` for letters (lower-cased), digits and punctuation, which are plain ASCII codes.
- Mouse buttons: `mouse_left = -1`, `mouse_middle = -2`, `mouse_right = -3`; handle `SDL_EVENT_MOUSE_BUTTON_DOWN`
  and `_UP` like keys (press only when the GUI does not claim the mouse, release always).
- `WindowSystem::setMouseCaptured(WindowHandle, bool)` calls `SDL_SetWindowRelativeMouseMode`, which also hides
  the pointer.
- Left out: gamepads.
- Tests: follow `WindowInputTests` / `WindowEventTests` (they inject SDL events).

### Step 6: change an object's look — 50 / 60

- Calls: `setObjectColor(RenderObjectHandle, LinearColor)`, `setObjectTexture(RenderObjectHandle,
  std::filesystem::path)`.
- Design: switch the instance to `acquirePrimitiveMaterial(color, texture slot)`; move the existing "release mesh,
  material and textures when unused" block of `removeObject` into one helper and call it for the old material.
- Left out: changing single PBR values of imported materials (an imported object becomes a plain material).
- Tests: CPU, in the style of `RenderPrimitiveSharingTests` (material counts and sharing).

### Step 7: shapes and meshes — 85 / 60

- Calls: `addSphere(Scalar radius, LinearColor color, Transform transform = {})` (cached like planes and cuboids)
  and `addMesh(MeshDescriptor)`. `MeshDescriptor` (new, `src/Types.ixx`) holds positions, indices, normals, UVs,
  colour, an optional base-colour texture path and a transform. Empty normals mean face normals, empty UVs mean
  zero; mismatched sizes return `Error::invalid_argument`.
- Left out: a cylinder (users can build one with `addMesh`).
- Tests: CPU vertex and index counts, errors, sharing.

### Step 8: picking — 100 / 70

- Calls: `objectBounds(RenderObjectHandle) const -> std::expected<Bounds, Error>` (world box from the mesh vertices
  and the world matrix); `raycast(Position origin, Direction direction, Scalar max_distance) const ->
  std::optional<RaycastHit>` with `RaycastHit{RenderObjectHandle object; Scalar distance;}` (nearest visible object,
  ray against world boxes); `screenRay(WindowHandle, Vec2 normalized_position) const` returning origin and
  direction from that window's camera.
- Left out: hits exact to the triangle.
- Tests: CPU: cubes at known places, nearest hit, misses, hidden objects ignored, the window centre gives the camera
  direction.

### Step 9: fullscreen — 25 / 20

- `WindowDesc::fullscreen` (appended at the end), `WindowSetup::fullscreen(bool)`, `SDL_WINDOW_FULLSCREEN` at window
  creation (`src/versions/simple/Window.ixx` about line 328), and `WindowSystem::setFullscreen(WindowHandle, bool)`.
  The existing resize path recreates the swapchain.
- Left out: choosing display modes.
- Tests: the flag is stored and the call succeeds with the offscreen driver.

### Step 10: images in the HUD — 100 / 60

- Calls: `GuiSystem::loadImage(std::filesystem::path) -> std::expected<GuiImageHandle, Error>` and
  `imageId(GuiImageHandle) const -> std::uint64_t` (an `ImTextureID` for `ImGui::Image`, 0 until the GUI and the
  texture are ready).
- Design: acquire the texture through the texture table and keep it from being released (exclude pinned slots in
  `RenderScene::releaseUnusedTextures`). After the GUI is initialized and the texture is uploaded, register it with
  `ImGui_ImplVulkan_AddTexture`.
- Left out: replacing or deleting images at runtime.
- Tests: CPU error path; GPU capture of an image drawn by ImGui.

## 5. Order and checks

Do the steps in the order above. Step 1 carries the shared groundwork, and steps 3 and 4 depend on it; steps 5 to
10 are independent. After each step: build, run all non-GPU tests (and GPU tests when possible), check that every
example still compiles, update the README and both AGENTS files, and commit. Stop and ask when a design here turns
out not to fit the code.
