# Teaching with the Simple engine

Complex is the default and implements the full rendering feature set. Simple builds
the same game sources against the same VVEngine facade. On Windows, use
`build_windows.cmd --simple`. Add `--no-tests` to build without running tests.

## Follow one frame

1. Start with [game.cpp](../examples/game/game.cpp): the game supplies objects,
   a camera and lights through the facade. It owns no Vulkan objects.
2. [RendererShadowPrep.cpp](../src/versions/simple/Render/RendererShadowPrep.cpp)
   computes the light views shared by both backends. There are up to **10
   directional, 10 point, and 10 spot lights**. A shadowed directional light uses
   four cascades; a point light uses six cube faces; a spot uses one projection.
3. [RendererDraw.cpp](../src/versions/simple/Render/RendererDraw.cpp) records
   shadows, then the colour passes for each window. Forward evaluates lighting as
   it draws each mesh, with Blinn-Phong as its default material model. Deferred
   writes opaque material attributes and depth, then lights the covered pixels
   with a fullscreen triangle. Its default is metallic-roughness PBR. Transparent
   objects still need a forward blending pass.
4. Read [simple_forward.slang](../src/versions/simple/shaders/simple_forward.slang)
   for material sampling, direct light and shadow lookup, and
   [simple_deferred.slang](../src/versions/simple/shaders/simple_deferred.slang)
   for reconstructing a surface from the G-buffer. The two paths share light and
   shadow calculations. A game can explicitly select the same material model
   on both paths, for example to compare the images.
5. The resolved colour is presented after any game-supplied post-processing;
   the HUD is drawn on its owning window. Input, scene loading, audio, terrain,
   sky, and optional 4x MSAA remain available to the games.

## Run the examples

After a Simple Release build on Windows, run these from the repository root:

```cmd
bin\simple\release\exe\game.exe --renderer forward
bin\simple\release\exe\racer.exe --renderer deferred
bin\simple\release\exe\island_delivery.exe --renderer deferred
```

Both backends run all three examples. Hybrid GI, probe lighting, screen-space
contact AO, SMAA, and built-in Neutral tone mapping belong to Complex. Simple
returns `Error::unsupported_feature` for those selections rather than silently
rendering a different result. Ordinary material occlusion and shadow maps do
not require GI.
