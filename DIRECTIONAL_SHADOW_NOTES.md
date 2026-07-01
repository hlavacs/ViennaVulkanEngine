# Directional Shadow Discovery Notes

Scope: only `src/versions/simple` and `src/versions/simple/shaders`.

## Spot-Light Shadow Pipeline

- CPU light model: `src/versions/simple/Scene.ixx:20`, `:47-57`, `:64-67` define `kMaxShadowedSpotLights`, `SpotLight`, `Scene::spotLights`, and the first-light mirror `Scene::spotLight`.
- Render API storage: `src/versions/simple/RenderSystem.ixx:115-124`, `:401-410`, `:739-808`, `:810-815` define `RenderSpotLight`, `RenderScene::setSpotLight/addSpotLight`, public `RenderSystem::setSpotLight/addSpotLight`, and load-time clamping to `kMaxShadowedSpotLights`.
- Shadow map allocation: `src/versions/simple/Renderer.ixx:128-131` owns `ShadowMap spotShadowMap` and `ShadowMap spotShadowArray`; `:310-317` creates the single spot map and the `kMaxShadowedSpotLights` array map/pipeline; `src/versions/simple/Vulkan.ixx:823-1038` implements `ShadowMap::create` with D32 image, sampled image usage, sampler, depth-only render pass, and per-layer framebuffers for arrays.
- Spot shadow pipeline creation: `src/versions/simple/Renderer.ixx:260-262`, `:316-317`, `:334-335` load `simple_forward.spot_shadow.vert.spv` and use entry `shadowVertexMainSpot`; `src/versions/simple/Vulkan.ixx:1496-1608` implements `ShadowMap::createPipeline`.
- GPU light data upload: `src/versions/simple/Renderer.ixx:491-511` builds per-spot view-projection matrices and packed position/color/direction/cone arrays; `:551-575` writes them into `FrameUniforms`; `src/versions/simple/Vulkan.ixx:3362-3388` defines the matching CPU uniform layout.
- Descriptor bindings: `src/versions/simple/Vulkan.ixx:1309-1322` declares binding 4 `spotShadowMap` and binding 5 `spotShadowArray`; `:3173-3223` writes those descriptors; `src/versions/simple/Renderer.ixx:396-399` binds both maps per frame.
- Shadow render/draw pass: `src/versions/simple/Renderer.ixx:974-987` renders the single spot shadow map; `:989-1005` loops active spot lights and renders each `spotShadowArray` layer with `ObjectPushConstants::spotLightIndex`.
- Shader stages: `src/versions/simple/shaders/simple_forward.slang:22`, `:33-36`, `:48`, `:90-94`, `:124-128`, `:177-202` define spot uniform arrays, push-constant light index, `Sampler2D spotShadowMap`, `Sampler2DArray spotShadowArray`, vertex entry `shadowVertexMainSpot`, and fragment sampling/compare from `spotShadowArray`.
- Shader compilation: `src/versions/simple/CMakeLists.txt:35-37`, `:111-119` define and compile `simple_forward.spot_shadow.vert.spv` from entry `shadowVertexMainSpot`.

## Directional-Light Handling And Gaps

- CPU light model: `src/versions/simple/Scene.ixx:21`, `:39-45`, `:64-65` define `kMaxDirectionalLights`, `DirectionalLight`, `Scene::directionalLights`, and first-light mirror `Scene::directionalLight`.
- Render API storage: `src/versions/simple/RenderSystem.ixx:97-104`, `:385-396`, `:691-717`, `:810-812` define `RenderDirectionalLight`, setter/add paths, and load-time clamping. `RenderDirectionalLight::light_view_projection` exists at `:103` but is not propagated into `ForwardRenderer` uniforms.
- Directional shadow resources exist: `src/versions/simple/Renderer.ixx:129`, `:307-308`, `:331-332`, `:394-395` own/create/bind `dirShadowMap` using `simple_forward.dir_shadow.vert.spv` entry `shadowVertexMainDir`.
- Directional light data upload exists only for lighting arrays and one scalar matrix: `src/versions/simple/Renderer.ixx:512-523` packs directional light arrays; `:554-562`, `:571-575` upload one `dirLightViewProj`, scalar directional data, and active count.
- Directional depth pass exists for one map only: `src/versions/simple/Renderer.ixx:959-972` renders `dirShadowMap` once using `dirShadowMap.pipeline`; there is no directional shadow array, no per-directional matrix array, and no loop equivalent to the spot array loop at `:989-1005`.
- Directional shader sampling is only slot zero: `src/versions/simple/shaders/simple_forward.slang:21`, `:37-41`, `:87-88`, `:118-122`, `:156-174` define one `dirLightViewProj`, directional light arrays, `Sampler2D dirShadowMap`, vertex entry `shadowVertexMainDir`, and apply `dirShadowFactor` only when `i == 0`.
- Shader compilation exists for the single directional vertex stage: `src/versions/simple/CMakeLists.txt:36`, `:100-108` compile `simple_forward.dir_shadow.vert.spv` from entry `shadowVertexMainDir`.

Plain-language gap: spot lights have a multi-light path: CPU stores up to four spots, builds one view-projection per spot, renders one depth layer per active spot, uploads per-spot light data, and samples `spotShadowArray` per spot in the fragment shader. Directional lights are lit as an array, but shadowing is only a single `dirShadowMap`/`dirLightViewProj` path for slot zero; additional directional lights have color/direction data but no per-light shadow map allocation, per-light shadow render loop, per-light light-space matrix array, or shader sampling for their own shadows.
