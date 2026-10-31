export module VVEngine.Types;
import std;
export import VVEngine.Error;
export import VVEngine.Handle;
export import VVEngine.Math;
export import VVEngine.Vector;
export import VVEngine.Entity;

/**
	* @file
	* @brief Public data and descriptor contract declared by the facade layer.
	*/
export namespace vve {

	struct SoundHandleTag {}; ///< Loaded audio resource category.
	struct AudioPlaybackHandleTag {}; ///< Independent playing voice category.
	using SoundHandle = TypedHandle<SoundHandleTag>; ///< Audio owned until unloadSound or shutdown.
	using AudioPlaybackHandle = TypedHandle<AudioPlaybackHandleTag>; ///< Voice owned until stopped or collected.

	/// @brief Linear audio volume: zero is silent, one is full volume.
	struct AudioVolume {
		float value{1.0F}; ///< Finite gain in [0, 1]; independent of renderer math precision.
	};

	/// @brief Playback speed of one voice: one is unchanged, two plays an octave higher and twice as fast.
	struct AudioPitch {
		float value{1.0F}; ///< Finite speed ratio in [0.01, 100].
	};

	/// @brief Options for one independent playback of a loaded sound.
	struct AudioPlaybackOptions {
		bool loop{false}; ///< Repeat indefinitely until stopped.
		AudioVolume volume{}; ///< Per-voice gain multiplied by the master gain.
	};

	/// @brief Choose upfront decoding for short effects or deferred decoding for longer audio.
	enum class AudioLoadMode {
		decoded, ///< Decode into PCM while loading; reuse for frequent sound effects.
		on_demand ///< Retain encoded data and decode during playback; not disk streaming.
	};


	struct SceneHandleTag {};											///< Scene descriptor handle tag.
	struct WindowHandleTag {};											///< Runtime window handle tag.
	struct NodeHandleTag {};											///< Node descriptor handle tag.
	struct MeshHandleTag {};											///< Mesh descriptor handle tag.
	struct MaterialHandleTag {};										///< Material descriptor handle tag.
	struct RenderObjectHandleTag {};								///< Render object handle tag.
	struct RenderSceneInstanceHandleTag {};						///< Render scene instance handle tag.
	struct LightHandleTag {};											///< Light descriptor handle tag.
	struct CameraHandleTag {};											///< Camera descriptor handle tag.
	struct SceneNodeHandleTag {};										///< World scene tree node handle tag.
	struct GuiImageHandleTag {};										///< HUD image handle tag.

	using SceneHandle	= TypedHandle<SceneHandleTag>;		///< Scene descriptor handle.
	using WindowHandle	= TypedHandle<WindowHandleTag>;		///< Runtime window handle.
	using NodeHandle	= TypedHandle<NodeHandleTag>;			///< Node descriptor handle.
	using MeshHandle	= TypedHandle<MeshHandleTag>;			///< Mesh descriptor handle.
	using MaterialHandle = TypedHandle<MaterialHandleTag>;	///< Material descriptor handle.
	using RenderObjectHandle = TypedHandle<RenderObjectHandleTag>;	///< Render object handle.
	using RenderSceneInstanceHandle = TypedHandle<RenderSceneInstanceHandleTag>;	///< Render scene instance handle.
	using LightHandle	= TypedHandle<LightHandleTag>;		///< Light descriptor handle.
	using CameraHandle	= TypedHandle<CameraHandleTag>;		///< Camera descriptor handle.
	using SceneNodeHandle = TypedHandle<SceneNodeHandleTag>;	///< World scene tree node handle.
	using GuiImageHandle = TypedHandle<GuiImageHandleTag>;		///< HUD image loaded with GuiSystem::loadImage.

	/// @brief Strong wrapper for world or local position values.
	struct Position {
		Vec3 value{zeroVec3()};													///< Wrapped coordinate.
	};

	/// @brief Strong wrapper for vectors that should be interpreted as directions.
	struct Direction {
		Vec3 value{Vec3(zero(), zero(), -one())};							///< Wrapped direction.
	};

	/// @brief World-space listener; audio distances use metres and velocities use metres per second.
	struct AudioListener {
		Position position{}; ///< Listening position, normally the active camera.
		Direction forward{}; ///< Nonzero viewing direction; normalized by the audio system.
		Direction up{.value = Vec3(zero(), one(), zero())}; ///< Up direction, independent of forward.
		Vec3 velocity{zeroVec3()}; ///< World-space velocity in metres per second for Doppler.
	};

	/// @brief Optional world-space voice placement; a voice remains ordinary 2D until setSource is called.
	struct AudioSource {
		Position position{}; ///< World-space source position in metres.
		Vec3 velocity{zeroVec3()}; ///< World-space velocity in metres per second for Doppler.
		float reference_distance{1.0F}; ///< Positive distance at which attenuation starts.
		float max_distance{10000.0F}; ///< Distance beyond which attenuation stops increasing; at least reference_distance.
	};

	/// @brief Strong wrapper for non-uniform scale factors.
	struct Scale {
		Vec3 value{oneVec3()};													///< Wrapped scale vector.
	};

	/// @brief Strong wrapper for quaternion rotations.
	struct Rotation {
		Quat value{identityQuat()};											///< Wrapped orientation.
	};

	/// @brief Strong wrapper for linear RGB color values.
	struct LinearColor {
		Vec3 value{oneVec3()};													///< Wrapped linear RGB color.
	};

	/// @brief Strong wrapper for relative light intensity.
	struct LightIntensity {
		Scalar value{one()};														///< Wrapped non-negative intensity scale.
	};

	/// @brief Strong wrapper for finite light influence distance.
	struct LightRange {
		Scalar value{static_cast<Scalar>(10)};								///< Wrapped range in world units.
	};

	/// @brief Spotlight cone half-angle in radians, measured from the spot axis to the cone edge.
	/// Uses the glTF KHR_lights_punctual convention for both inner and outer cones.
	struct SpotConeAngle {
		Scalar radians{static_cast<Scalar>(0.75)};						///< Wrapped axis-to-edge half-angle in radians.
	};

	/// @brief Public imported-light category understood by the asset facade.
	enum class LightKind {
		directional,																///< Infinite light with direction only.
		point,																		///< Positional light radiating in all directions.
		spot																			///< Positional light constrained by a cone.
	};

	/// @brief Facade descriptor for imported light data stored by the asset system.
	struct LightDescriptor {
		LightKind kind{LightKind::point};									///< Imported light category.
		LinearColor color{};													///< Linear RGB light color.
		LightIntensity intensity{};											///< Imported or derived intensity scale.
		Direction direction{};													///< World-space light direction (light toward scene) when available.
		Position position{};													///< World-space light position when available.
		LightRange range{};														///< Finite influence range for point and spot lights.
		SpotConeAngle cone{};													///< Outer half-angle in radians, from spot axis to cone edge (glTF convention).
		SpotConeAngle inner_cone{.radians = static_cast<Scalar>(0.35)};	///< Inner half-angle in radians, from spot axis to full-intensity edge (glTF convention).
	};

	/// @brief Facade descriptor for directional light setup.
	struct DirectionalLight {
		Direction direction{};													///< Direction in which the light travels (from the light toward the scene).
		LinearColor color{};													///< Linear RGB direct light color.
		LightIntensity intensity{};											///< Direct light intensity scale.
		LinearColor ambient{.value = Vec3(static_cast<Scalar>(0.04), static_cast<Scalar>(0.04),
													 static_cast<Scalar>(0.04))};	///< Linear RGB ambient contribution.
	};

	/// @brief Facade descriptor for point light setup.
	struct PointLight {
		Position position{};													///< World-space light position.
		LinearColor color{};													///< Linear RGB direct light color.
		LightIntensity intensity{};											///< Direct light intensity scale.
		LightRange range{};														///< Finite influence range.
		LinearColor ambient{.value = Vec3(static_cast<Scalar>(0.04), static_cast<Scalar>(0.04),
													 static_cast<Scalar>(0.04))};	///< Linear RGB ambient contribution.
	};

	/// @brief Facade descriptor for spotlight setup.
	struct SpotLight {
		Position position{};													///< World-space light position.
		Direction direction{};													///< World-space spotlight direction.
		LinearColor color{};													///< Linear RGB direct light color.
		LightIntensity intensity{};											///< Direct light intensity scale.
		LightRange range{};														///< Finite influence range.
		SpotConeAngle cone{};													///< Outer half-angle in radians, from spot axis to cone edge (glTF convention).
		LinearColor ambient{.value = Vec3(static_cast<Scalar>(0.04), static_cast<Scalar>(0.04),
													 static_cast<Scalar>(0.04))};	///< Linear RGB ambient contribution.
	};

	/// @brief Strong wrapper for vertical field-of-view angles.
	struct FovY {
		Scalar radians{static_cast<Scalar>(1.0471975511965976)};		///< Wrapped vertical FOV in radians.
	};

	/// @brief Facade descriptor for imported camera data stored by the asset system.
	struct CameraDescriptor {
		Position position{};													///< World-space camera position when available.
		Direction direction{};													///< World-space camera look direction.
		Direction up{.value = Vec3(zero(), one(), zero())};			///< World-space camera up direction.
		FovY fov{};																///< Vertical field-of-view angle.
		Scalar aspect{one()};													///< Projection aspect ratio.
		Scalar near_clip{static_cast<Scalar>(0.1)};					///< Near clipping distance.
		Scalar far_clip{static_cast<Scalar>(10000.0)};				///< Far clipping distance.
	};

	/// @brief Strong wrapper for near and far clipping planes.
	struct ClipPlanes {
		Scalar near_plane{static_cast<Scalar>(0.1)};						///< Near clip distance.
		Scalar far_plane{static_cast<Scalar>(100.0)};					///< Far clip distance.
	};

	/// @brief Strong wrapper for frame delta time.
	struct DeltaTime {
		double seconds{1.0 / 60.0};											///< Elapsed seconds.
	};

	/// @brief Strong wrapper for pixel dimensions.
	struct PixelExtent {
		std::uint32_t width{0};													///< Width in pixels.
		std::uint32_t height{0};												///< Height in pixels.
	};

	/// @brief Strong wrapper for human-readable object names.
	struct ObjectName {
		std::string value{};														///< Wrapped display or diagnostic name.
	};

	/// @brief Scene rendering passes selected independently for each engine, shared by its windows.
	enum class RendererBackend {
		forward, ///< Opaque and transparent objects use forward lighting (default).
		deferred, ///< Opaque G-buffer and deferred lighting, followed by forward transparency.
		hybrid_gi ///< Deferred opaque lighting with bounded raster-probe approximate diffuse GI.
	};
	/// @brief Dynamic-rendering scope supplied to application-owned Vulkan geometry.
	enum class NativeDrawPass {
		forward_opaque, ///< Forward backend solids, before sky and transparency.
		gbuffer, ///< Deferred/hybrid opaque and cutout material attributes.
		shadow, ///< Depth-only light view, including the cached probe-sun map.
		probe_capture, ///< Direct + emission + sky radiance and signed radial metres, without GI feedback.
		forward_transparent ///< Forward-only composition over opaque depth, before post-processing and GUI.
	};
	/// @brief Borrowed native Vulkan handle bits and per-pass data; never owners. See docs/deferred-renderer.md.
	/// prepare runs once outside rendering after this window/slot fence; draw calls run after engine geometry.
	/// Bind your own pipeline/descriptors/vertices. Do not submit, end rendering, mutate VVE or retain this context.
	struct NativeDrawContext {
		NativeDrawPass pass{NativeDrawPass::forward_opaque}; ///< Scope; prepare identifies the upcoming main geometry pass.
		std::uintptr_t device{}, physicalDevice{}, commandBuffer{}; ///< Dispatchable Vulkan handle bits, as in the former native hook.
		WindowHandle window{}; ///< Independent per-window resource identity.
		std::uint32_t frameIndex{}, width{}, height{}; ///< Fence-completed slot and this view's pixel extent.
		std::uint32_t viewportX{}, viewportY{}; ///< Atlas viewport/scissor origin; zero outside probe capture.
		std::array<std::uint32_t,3> colorFormats{}; ///< VkFormat values: G-buffer targets are RGBA16F, capture/forward HDR RGBA16F.
		std::uint32_t colorAttachmentCount{}, depthFormat{}, samples{}; ///< Pipeline rendering formats and VkSampleCountFlagBits; shadow/capture are 1x.
		Mat4 view{identityMat4()}, projection{identityMat4()}, viewProjection{identityMat4()}; ///< Actual pass matrices; Vulkan Y flip and depth [0,1], clear 1, LESS.
		std::array<Vec4,6> frustum{}; ///< Inward unnormalized world planes (left,right,bottom,top,near,far); dot(plane,world)>=0 is inside.
		Position eye{}; ///< This pass's eye, including a probe or light eye; never substituted with the main camera.
		std::uint32_t faceIndex{}, cascadeIndex{}, lightIndex{}, lightType{}; ///< Cube face +X,-X,+Y,-Y,+Z,-Z; volume/cascade; packed light; type 1 spot/2 point/3 sun.
		bool probeShadow{}; ///< Shadow scope is the cached local-volume sun map, rather than a main light layer.
		Scalar captureFar{}; ///< Capture alpha: min(radial metres,captureFar), negative on backfaces; sky uses positive captureFar.
		std::array<std::uint64_t,2> probeIrradianceViews{}; ///< Borrowed RGBA32F VkImageView bits; sampled-image descriptors, SHADER_READ_ONLY_OPTIMAL, forward draws only.
		Vec4 probeOrigin{}, probeControl{}; ///< Near origin/spacing; enabled/strength/visibility/AO. Use shaders/vve_irradiance.slang, E/pi times diffuse albedo once.
		Position probeEye{}; ///< Main window eye used for coarse origin and visibility bias, independent of this pass's eye.
		std::uint32_t probeCascades{}; ///< Zero without usable shared probes; otherwise near/coarse count for the public sampler.
		std::uint64_t sunShadowView{}, captureSunShadowView{}; ///< Borrowed sampled D32 array views, shader-read layout after shadows; never sample during prepare/shadow.
		std::array<Mat4,4> sunShadowMatrices{}; ///< First direct sun's world-to-shadow cascade transforms; zero sunShadowCount disables sampling.
		Mat4 captureSunShadowMatrix{identityMat4()}; ///< Cached world-to-shadow transform for direct-only probe radiance.
		Vec4 sunShadowSplits{}; ///< Positive main-view depth boundaries in metres; last cascade fades beyond its far plane.
		Vec4 sunShadowTexels{}; ///< Reciprocal main-shadow width/height in xy and capture-shadow width/height in zw; zero when absent.
		Mat4 mainView{identityMat4()}; ///< Main camera view for native cascade selection, independent of capture/light views.
		std::uint32_t sunShadowCount{}, sunShadowFirstLayer{}; ///< Available cascades (0 or 4) and first array layer for the selected sun.
		bool captureSunShadowEnabled{}; ///< Cached sun depth is usable by probe captures after its write/read dependency.
		bool samplerAnisotropy{}, tessellationShader{}; ///< Enabled optional device features for application pipelines.
		bool prepare{}; ///< Outside-rendering upload phase; do not sample engine attachments or draw to them here.
	};
	using NativeDrawCallback = std::function<std::expected<void,Error>(const NativeDrawContext &)>; ///< Owned recorder; replacement retires its captures by submission fences.

	/// @brief Bounded probe refresh cost; both presets use the same coverage and storage.
	enum class GiQuality { low, ///< One 16-square cubemap per window frame.
		medium ///< Two 16-square cubemaps per window frame.
	};
	/// @brief Runtime independent probe-light and spatial contact-AO controls for hybrid_gi.
	struct GiSettings {
		bool enabled{true}; ///< Probe lighting is used only by hybrid_gi.
		bool ao_enabled{false}; ///< Depth/normal-aware 0.5 m contact AO on indirect diffuse only; min with material AO, no history.
		Scalar indirect_strength{1}; ///< Finite nonnegative probe multiplier; zero with AO off restores deferred lighting exactly.
		GiQuality quality{GiQuality::medium}; ///< Maximum probe captures per window frame.
	};
	/// @brief Geometry edge smoothing; selected before the renderer's first frame.
	enum class Antialiasing {
		none, ///< One sample per pixel; preserves the default rendering path.
		msaa4 ///< Four coverage samples; initialization returns platform_error if unsupported.
	};
	/// @brief Material response; classic preserves existing scenes, physically_based uses GGX and the sky panorama.
	enum class MaterialLighting {
		classic, ///< Existing Blinn-Phong response.
		physically_based ///< GGX direct response and approximate sky illumination.
	};
	/// @brief Untextured surface factors; imported roughness/metalness maps still multiply these values.
	struct SurfaceMaterial {
		Scalar roughness{0.5F}; ///< Perceptual roughness in [0,1]; zero gives the sharpest supported reflection.
		Scalar metalness{}; ///< Metal fraction in [0,1]; metals reflect their base colour and have no diffuse term.
	};

	/// @brief Strong wrapper for renderer selection identifiers.
	struct RendererId {
		std::string value{};														///< Wrapped renderer identifier.
	};

	/// @brief Window creation descriptor shared by the facade and platform layer.
	struct WindowDesc {
		std::string id{"main"};									///< Stable application-local window id.
		std::string title{"VVE simple"};							///< Platform window title.
		PixelExtent extent{.width = 960, .height = 540};			///< Initial size in window coordinates.
		std::optional<int> x{};									///< Optional initial screen x coordinate.
		std::optional<int> y{};									///< Optional initial screen y coordinate.
		RendererId renderer_id{};								///< Empty or "forward" renders; "none" opts out; any other id makes Engine::init return invalid_argument.
		bool resizable{true};									///< Enables platform resizing.
		bool visible{true};										///< Shows the window; a hidden window still renders (tests, captures).
		bool fullscreen{false};									///< Starts in fullscreen at the desktop resolution.
	};

	/// @brief Runtime window state exposed through the facade window wrapper.
	struct WindowInfo {
		WindowHandle handle{};									///< 64-bit runtime window handle.
		std::string id{};										///< Stable id copied from WindowDesc.
		std::string title{};										///< Current platform title.
		PixelExtent extent{};									///< Current drawable size in pixels (the swapchain size).
		RendererId renderer_id{};								///< Renderer id selected for this window.
		bool focused{false};										///< True while the window has keyboard focus.
		bool minimized{false};									///< True while the platform reports a minimized window.
		bool should_close{false};								///< True after a close request.
	};

	/// @brief Snapshot passed to user systems that want window data for the current frame.
	struct WindowFrameData {
		Vector<WindowInfo> windows{};							///< Window states after event polling.
	};

	/// @brief One CPU/GPU shadow-depth comparison point; the world point is the origin for every light.
	struct RenderShadowDepthSample {
		std::uint32_t light_type{};								///< Light type: 1 spot, 2 point, 3 directional.
		std::uint32_t light_index{};								///< Dense index of the light inside its packed shadow slots.
		std::uint32_t face_index{};								///< Point-light cube face (+X,-X,+Y,-Y,+Z,-Z), 0 for other lights.
		std::uint32_t layer{};									///< Layer of the light type's shadow array that this sample reads.
		Vec3 world{zeroVec3()};									///< World-space sample point.
		Vec3 light_ndc{zeroVec3()};								///< Sample point in light normalized device coordinates.
		std::uint32_t pixel_x{};									///< Shadow-map texel x, valid when has_gpu is true.
		std::uint32_t pixel_y{};									///< Shadow-map texel y, valid when has_gpu is true.
		float expected_depth{};									///< CPU light-space depth (light_ndc.z).
		float bias{};											///< Shader-side compare bias mirrored on the CPU.
		float shadow_factor{1.0F};								///< Occluded factor when the GPU texel occludes the point, otherwise 1.
		float gpu_depth{-1.0F};									///< Depth read back from the shadow map, valid when has_gpu is true.
		float error{-1.0F};										///< Absolute difference between expected_depth and gpu_depth.
		bool has_gpu{};											///< True once the GPU texel was read back.
	};

	/// @brief Strong wrapper for frame counts and frame indices.
	struct FrameCount {
		std::uint64_t value{0};													///< Wrapped frame count.
	};

	/// @brief Human-readable application name selected by the user program.
	struct ApplicationName {
		std::string value{"simple"};												///< Name shown in diagnostics and default window titles.
	};

	/// @brief Optional frame cap; zero lets the engine run until a close request.
	struct MaxFrames {
		FrameCount value{};														///< Maximum number of step() calls.
	};

	/// @brief Per-frame timing context passed to user systems.
	struct FrameContext {
		FrameCount frame_index{};												///< Zero-based frame index.
		DeltaTime delta_time{};													///< Time elapsed since the previous frame.
	};

	/// @brief Result of one engine frame.
	enum class FrameStatus {
		running,																		///< Engine can continue stepping.
		stopped,																		///< Engine stopped because a close request or frame cap was reached.
	};

	/// @brief Strong wrapper for source vertex counts.
	struct VertexCount {
		std::uint64_t value{0};													///< Wrapped vertex count.
	};

	/// @brief Strong wrapper for source index counts.
	struct IndexCount {
		std::uint64_t value{0};													///< Wrapped index count.
	};

	/// @brief Standard transform component shared by all active engine layers.
	struct Transform {
		Position translation{};													///< Local or world-space translation.
		Rotation rotation{};														///< Local or world-space orientation.
		Scale scale{};																///< Local or world-space non-uniform scale.
	};

	/// @brief Facade descriptor for plane scene-object setup.
	struct PlaneDescriptor {
		Vec2 half_extent{};														///< Half-size along the local plane axes.
		LinearColor color{};													///< Linear RGB surface color.
		Transform transform{};													///< Local or world-space placement.
	};

	/// @brief Facade descriptor for cuboid scene-object setup.
	struct CuboidDescriptor {
		Vec3 minimum{};															///< Minimum local-space corner.
		Vec3 maximum{};															///< Maximum local-space corner.
		LinearColor color{};													///< Linear RGB surface color.
		Transform transform{};													///< Local or world-space placement.
	};

	/// @brief Facade descriptor for textured cuboid scene-object setup.
	struct TexturedCuboidDescriptor {
		Vec3 minimum{};															///< Minimum local-space corner.
		Vec3 maximum{};															///< Maximum local-space corner.
		std::filesystem::path base_color_texture{};					///< Base-color texture source path.
		Transform transform{};													///< Local or world-space placement.
	};

	/// @brief Facade descriptor for a mesh built by game code; per-vertex arrays are either empty or as long as positions.
	struct MeshDescriptor {
		Vector<Vec3> positions{};												///< Object-space vertex positions.
		Vector<std::uint32_t> indices{};										///< Triangle list into positions, counter-clockwise front faces.
		Vector<Vec3> normals{};													///< Vertex normals; empty gives flat face normals.
		Vector<Vec2> uvs{};														///< Texture coordinates, v pointing up; empty gives zero.
		LinearColor color{.value = oneVec3()};								///< Linear RGB surface color.
		std::filesystem::path base_color_texture{};					///< Optional base-color texture; empty means none.
		Transform transform{};													///< Local or world-space placement.
	};

	/// @brief Height-dependent haze; zero density disables it. Distances use world units, colours are linear.
	struct AtmosphereSettings {
		LinearColor color{.value = Vec3{0.65F, 0.75F, 0.85F}}; ///< Colour approached by distant surfaces; the enabled SkySettings light the haze instead.
		Scalar density{}; ///< Extinction per world unit; nonnegative.
		Scalar height_falloff{}; ///< Density decreases exponentially with height; zero gives uniform haze.
		Scalar base_height{}; ///< Height at which density is specified.
	};

	/// @brief Physically based sky with volumetric clouds; the first directional light is its sun.
	///
	/// Heights are world units (metres) above y = 0. When enabled it replaces a skybox panorama and the haze colour:
	/// distant surfaces fade into the sky, physically based materials reflect it, clouds shade the sun, and
	/// AtmosphereSettings adds weather haze or fog lit by sun and sky. Clouds drift with the animation time.
	struct SkySettings {
		bool enabled{false}; ///< False keeps the skybox panorama or the background colour.
		Scalar intensity{1}; ///< Brightness of sky, clouds and sun disk; one suits a sun light of intensity about 1.5.
		Scalar haze{1}; ///< Aerosols: 1 clear, 4 hazy, 12 milky; whitens the horizon and widens the glow around the sun.
		Scalar sun_size{2}; ///< Sun disk radius in multiples of the real sun; zero hides it.
		Scalar cloud_coverage{}; ///< 0 no clouds, 0.4 scattered, 0.7 broken, 1 overcast.
		Scalar cloud_base{1200}; ///< Height of the cloud base.
		Scalar cloud_thickness{1500}; ///< Vertical extent of the cloud layer.
		Scalar cloud_density{1}; ///< Below one, clouds are thin and translucent.
		Vec2 wind{10.0F, 3.0F}; ///< Cloud drift along x and z per animation second.
	};

	/// @brief Explicit animation clock; games supply elapsed seconds for repeatable rendering.
	struct AnimationTime {
		double seconds{}; ///< Nonnegative elapsed seconds; defaults to a paused scene.
	};

	/// @brief A regular height grid drawn with GPU tessellation; detail follows the distance to the camera.
	///
	/// Only the heights and a coarse patch grid are stored on the GPU (about 4 bytes per sample instead of a full
	/// triangle mesh), so square-kilometre terrains fit easily. Sample (x, z) lies at (x * cell_size, height, z * cell_size)
	/// in object space; the texture is stretched over the whole grid with u along +X and v along +Z.
	struct TerrainDescriptor {
		std::uint32_t columns{};											///< Samples along +X, 2 to 65535.
		std::uint32_t rows{};												///< Samples along +Z, 2 to 65535.
		Scalar cell_size{1.0F};											///< Positive distance between neighbouring samples.
		Vector<float> heights{};											///< columns * rows finite heights, row by row (row z, column x).
		LinearColor color{.value = oneVec3()};						///< Linear base colour; multiplies the texture.
		std::filesystem::path base_color_texture{};				///< Optional texture covering the whole grid.
		Transform transform{};												///< Places sample (0, 0) at the local origin.
	};

	/// @brief A horizontal water patch with two analytic waves; its handle supports ordinary object operations.
	struct OceanDescriptor {
		Vec2 half_extent{100.0F, 100.0F}; ///< Half-size of the finite patch along X and Z.
		LinearColor color{.value = Vec3{0.015F, 0.12F, 0.18F}}; ///< Linear water tint.
		Scalar wave_height{0.2F}; ///< Maximum vertical displacement in world units.
		Scalar wavelength{12.0F}; ///< Positive reference wavelength in world units.
		Scalar speed{1.5F}; ///< Angular wave speed in radians per second.
		Position position{}; ///< Centre of the undisturbed surface.
		std::uint32_t columns{64}; ///< Grid cells along X, in [1, 1024]; a few per wavelength suffice for the vertex waves.
		std::uint32_t rows{64}; ///< Grid cells along Z, in [1, 1024]; a thin strip needs only one.
	};

	/// @brief Nearest render object found by RenderSystem::raycast.
	struct RaycastHit {
		RenderObjectHandle object{};											///< Object whose world box the ray enters first.
		Scalar distance{};														///< Distance from the ray origin to the box, zero when the origin is inside.
	};

	/// @brief Public scene instantiation options for imported scene visibility.
	struct SceneInstantiationOptions {
		bool instantiate_geometry{true};									///< Creates render objects from imported geometry.
		bool apply_cameras{false};											///< Applies imported cameras to the render scene.
		bool apply_lights{false};											///< Applies imported lights to the render scene.
		bool keep_hierarchy{false};										///< Copies the node tree into the world scene tree; moving a node then moves all objects below it.
	};

	/// @brief Axis-aligned bounds described by minimum and maximum positions.
	struct Bounds {
		Position minimum{};														///< Minimum corner.
		Position maximum{};														///< Maximum corner.
		bool valid{false};														///< False until at least one point has been included.
	};

	/// @brief Public camera description used by game code and renderers.
	struct Camera {
		Position position{.value = Vec3(zero(), static_cast<Scalar>(1.5), static_cast<Scalar>(6.0))};
		Direction forward{.value = Vec3(zero(), zero(), -one())};	///< View direction.
		FovY fov_y{};																///< Vertical field of view.
		ClipPlanes clip{};														///< Near/far clip planes.

		/// @brief Describes a view by position and direction; up is retained for call compatibility.
		[[nodiscard]] static inline Camera lookAt(Position position, Position target,
																[[maybe_unused]] Direction up = Direction{.value = Vec3(zero(), one(), zero())},
																FovY fov_y = {}, ClipPlanes clip = {}) {
			Camera camera{};
			camera.position = position;
			camera.forward = Direction{.value = math::subtract(target.value, position.value)};
			camera.fov_y = fov_y;
			camera.clip = clip;
			return camera;
		}
	};

} // namespace vve
