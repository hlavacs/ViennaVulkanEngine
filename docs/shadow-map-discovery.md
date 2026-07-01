# Simple Shadow Map Discovery

Discovery scope: `src/versions/simple` and `examples/light_shadow_debug`, after reading `AGENTS.md` and `src/versions/simple/AGENTS.md`.

## Owning Files And Symbols

- Light data:
  - `src/versions/simple/Scene.ixx`: `vve::simple::PointLight`, `DirectionalLight`, `SpotLight`, and `Scene::{pointLight,directionalLight,spotLight}` are the concrete forward-renderer scene lights. `Scene` currently stores one light of each type, not arrays.
  - `src/versions/simple/RenderSystem.ixx`: `RenderDirectionalLight`, `RenderPointLight`, `RenderSpotLight`, `RenderScene::{light_,point_light_,spot_light_}`, and `RenderSystem::{setDirectionalLight,setPointLight,setSpotLight}` are the facade-facing resource/light setup path. The setters update both `RenderScene` optionals and `ForwardRenderer::scene`.

- Shadow/depth resources:
  - `src/versions/simple/Renderer.ixx`: `ForwardRenderer::{depthImage,shadowMap,dirShadowMap,spotShadowMap}` owns the swapchain depth image and three single-layer `ShadowMap` instances.
  - `src/versions/simple/Vulkan.ixx`: `VulkanDepthImage` owns the swapchain-sized depth attachment; `ShadowMap` owns a square sampled `VK_FORMAT_D32_SFLOAT` image, memory, view, sampler, render pass, framebuffer, pipeline layout, and pipeline.
  - `src/versions/simple/Vulkan.ixx`: `VulkanDescriptorSetLayout`, `VulkanDescriptorPool`, and `VulkanDescriptorSets::{writeShadowMap,writeDirShadowMap,writeSpotShadowMap}` own descriptor bindings for sampled shadow maps.

- Scene submission/render stages:
  - `src/versions/simple/RenderSystem.ixx`: `RenderScene` stores CPU render resources; `RenderSystem::{addPlane,addCuboid,addTexturedCuboid}` create facade-level resources and call `appendBackendObject`; `appendBackendObject` converts `RenderVertex` data into backend `Mesh` data and calls `ForwardRenderer::appendObject`.
  - `src/versions/simple/Renderer.ixx`: `ForwardRenderer::init` uploads `scene.objects` into `meshes`; `ForwardRenderer::drawFrame` updates `FrameUniforms`; `ForwardRenderer::recordCommandBuffer` records shadow passes and the forward color pass.
  - `src/versions/simple/Renderer.ixx`: `detail::forward_renderer_pass_contracts` currently exposes only `frame_begin -> forward.color_pass -> scene_color -> frame_finished`. It does not expose the shadow passes recorded in `recordCommandBuffer`.
  - `src/versions/simple/RenderPassContract.ixx`: `RenderMilestone::shadow_depth()` exists as vocabulary, but the current forward renderer pass list does not use it.
  - `src/versions/simple/SceneGraph.ixx`: `SceneTree = Tree<NodeHandle>` exists, but the observed forward renderer path submits from `RenderSystem`/`Scene` object vectors, not from `SceneTree`.

- Forward Slang shaders:
  - `src/versions/simple/shaders/simple_forward.slang`: `vertexMain`, `fragmentMain`, `shadowVertexMain`, `shadowVertexMainDir`, and `shadowVertexMainSpot`.
  - `src/versions/simple/CMakeLists.txt`: compiles `simple_forward.slang` into `simple_forward.vert.spv`, `simple_forward.frag.spv`, `simple_forward.shadow.vert.spv`, `simple_forward.dir_shadow.vert.spv`, and `simple_forward.spot_shadow.vert.spv`.

## Current Shadow Map Flow

There is not just one shadow resource today. The forward renderer creates three separate single-layer 2D shadow maps:

- `ForwardRenderer::init` calls `shadowMap.create`, `dirShadowMap.create`, and `spotShadowMap.create`, then creates pipelines with entries `shadowVertexMain`, `shadowVertexMainDir`, and `shadowVertexMainSpot`.
- `ShadowMap::create` creates a 1024x1024 `VK_FORMAT_D32_SFLOAT` image with `VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT`, one `VK_IMAGE_VIEW_TYPE_2D` view, a clamp-to-edge non-comparison sampler, a depth-only render pass whose final layout is `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`, and a one-layer framebuffer.
- `ShadowMap::createPipeline` creates a depth-only graphics pipeline with one vertex stage, no fragment stage, `VK_COMPARE_OP_LESS`, `depthWriteEnable = VK_TRUE`, no culling, and `depthBiasEnable = VK_TRUE`; both `depthBiasConstantFactor` and `depthBiasSlopeFactor` are currently `0.0F`.
- `ForwardRenderer::recordCommandBuffer` records three depth render passes before the color pass. It draws the same uploaded meshes through `drawUploadedObjects` into `shadowMap`, `dirShadowMap`, then `spotShadowMap`, and then begins the swapchain `renderPass` for `graphicsPipeline`.
- Descriptor binding layout in `VulkanDescriptorSetLayout::create`: binding 0 is `FrameUniforms`, binding 1 is `shadowMap`, binding 2 is `baseColorTexture`, binding 3 is `dirShadowMap`, and binding 4 is `spotShadowMap`. `ForwardRenderer::init` writes all three shadow descriptors for each frame.

Current light-space matrices and sampling:

- `ForwardRenderer::drawFrame` computes `FrameUniforms::lightViewProj` from the point light using an orthographic projection and `lookAt(light.position, origin)`.
- `FrameUniforms::dirLightViewProj` uses an orthographic projection from `dirLightEye`, derived from `DirectionalLight::direction`.
- `FrameUniforms::spotLightViewProj` uses `perspectiveVulkan(spot.outerConeAngle * 2, 1, 0.1, spot.range)` and `lookAt(spot.position, spot.position + normalized spot.direction)`.
- In `simple_forward.slang`, the shadow vertex entries write clip-space positions from `frame.lightViewProj`, `frame.dirLightViewProj`, and `frame.spotLightViewProj`.
- `fragmentMain` samples `shadowMap`, `dirShadowMap`, and `spotShadowMap` with `Sampler2D.Sample(uv).r`. It computes `shadowFactor`, `dirShadowFactor`, and `spotShadowFactor` as `0.35` when inside the map and `lightNdc.z - 0.001 > storedDepth`; otherwise the factor is `1.0`. The shader-side compare bias is therefore the literal `0.001`.

## Forward Color Pass Data Layout

- Host uniform block: `src/versions/simple/Vulkan.ixx` defines `FrameUniforms` with `view`, `projection`, `lightViewProj`, `dirLightViewProj`, `spotLightViewProj`, and packed `Vec4` light fields: `lightPositionRange`, `lightColorIntensity`, `lightShadowAmbient`, `dirLightDirection`, `dirLightColorIntensity`, `dirLightShadowAmbient`, `spotLightPositionRange`, `spotLightColorIntensity`, `spotLightDirection`, and `spotLightConeAmbient`.
- Shader uniform block: `src/versions/simple/shaders/simple_forward.slang` defines matching `FrameUniforms`.
- Push constants: `src/versions/simple/Vulkan.ixx` and `simple_forward.slang` both define `ObjectPushConstants` with `model` and `useBaseColorTexture`. `ForwardRenderer::recordCommandBuffer` pushes this block per object before `vkCmdDrawIndexed`.
- Descriptor set 0: binding 0 is the uniform buffer; bindings 1, 3, and 4 are shadow samplers; binding 2 is the optional base-color texture.

## Light/Shadow Debug Example

- Location: `examples/light_shadow_debug/light_shadow_debug.cpp`.
- `loadShadowTestScene` creates one plane, one shadow-caster cuboid, one light-marker cuboid, then sets exactly three lights: one point light via `renderSystem.setPointLight`, one directional light via `renderSystem.setDirectionalLight`, and one spot light via `renderSystem.setSpotLight`.
- It currently sets one spot light, not two.
- The example writes `light_shadow_debug.txt` with `scene_instances`, `scene_meshes`, `directional_light`, `point_light`, `spot_light`, and `png_written`. It also captures `light_shadow_debug.png` through `RenderSystem::captureFrameToPng`.

## Existing CPU Readback Or Debug Data Path

- Image readback exists: `src/versions/simple/Vulkan.ixx` defines `VulkanReadback::{create,capture,pixelBytes}` and `writeReadbackPng`. `ForwardRenderer::captureFrameToPng` uses that path to copy the last swapchain image and write a PNG.
- In-frame optional image readback is partially wired: `ForwardRenderer::drawFrame(VulkanReadback *readback)` can call `readback->capture(...)` and stores the result in `lastReadbackCaptureResult`, but `RenderSystem::renderFrame` currently passes `nullptr`.
- Text/debug sample structs exist but are stubs: `src/versions/simple/Renderer.ixx` defines `RenderDebugSample` and `RenderShadowDepthSample`, including shadow-depth, bias, and factor fields, but `ForwardRenderer` and `StubRenderer` debug accessors all return zero counts or `std::nullopt`.
- No existing CPU-readback path currently carries per-light shadow debug values. The usable existing path is the color-image `VulkanReadback`/PNG path; per-light scalar debug values would need a new minimal buffer or text-output path in a later implementation task.
