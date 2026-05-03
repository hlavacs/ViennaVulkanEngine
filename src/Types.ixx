export module VEEngine:Types;
import std;
export import :Error;
export import :Math;

/**
 * @file
 * @brief Upper-layer semantic engine types built from the thin math facade.
 */
export namespace vve {

   template <typename T> using Vector = std::vector<T>; ///< Facade dynamic array type.

   using Scalar = math::Scalar; ///< Facade scalar type.
   using Vec2   = math::Vec2;   ///< Facade 2D vector type.
   using Vec3   = math::Vec3;   ///< Facade 3D vector type.
   using Vec4   = math::Vec4;   ///< Facade 4D vector type.
   using Quat   = math::Quat;   ///< Facade quaternion type.
   using Mat4   = math::Mat4;   ///< Facade 4x4 matrix type.

   [[nodiscard]] inline constexpr Scalar zero() noexcept { return math::zero(); } ///< Scalar zero.

   [[nodiscard]] inline constexpr Scalar one() noexcept { return math::one(); } ///< Scalar one.

   [[nodiscard]] inline Vec3 zeroVec3() noexcept { return math::zeroVec3(); } ///< Zero 3D vector.

   [[nodiscard]] inline Vec3 oneVec3() noexcept { return math::oneVec3(); } ///< One-filled 3D vector.

   [[nodiscard]] inline Quat identityQuat() noexcept { return math::identityQuat(); } ///< Identity rotation.

   [[nodiscard]] inline Mat4 identityMat4() noexcept { return math::identityMat4(); } ///< Identity matrix.

   /// @brief Strong wrapper for world or local position values.
   struct Position {
      math::Vec3 value{math::zeroVec3()}; ///< Wrapped coordinate.
   };

   /// @brief Strong wrapper for vectors that should be interpreted as directions.
   struct Direction {
      math::Vec3 value{math::Vec3(math::zero(), math::zero(), -math::one())}; ///< Wrapped direction.
   };

   /// @brief Strong wrapper for non-uniform scale factors.
   struct Scale {
      math::Vec3 value{math::oneVec3()}; ///< Wrapped scale vector.
   };

   /// @brief Strong wrapper for quaternion rotations.
   struct Rotation {
      math::Quat value{math::identityQuat()}; ///< Wrapped orientation.
   };

   /// @brief Strong wrapper for linear RGB color values.
   struct LinearColor {
      math::Vec3 value{math::oneVec3()}; ///< Wrapped linear RGB color.
   };

   /// @brief Strong wrapper for relative light intensity.
   struct LightIntensity {
      math::Scalar value{math::one()}; ///< Wrapped non-negative intensity scale.
   };

   /// @brief Strong wrapper for vertical field-of-view angles.
   struct FovY {
      math::Scalar radians{static_cast<math::Scalar>(1.0471975511965976)}; ///< Wrapped vertical FOV in radians.
   };

   /// @brief Strong wrapper for near and far clipping planes.
   struct ClipPlanes {
      math::Scalar near_plane{static_cast<math::Scalar>(0.1)};     ///< Near clip distance.
      math::Scalar far_plane{static_cast<math::Scalar>(10000.0)}; ///< Far clip distance.
   };

   /// @brief Strong wrapper for frame delta time.
   struct DeltaTime {
      double seconds{1.0 / 60.0}; ///< Elapsed seconds.
   };

   /// @brief Strong wrapper for pixel dimensions.
   struct PixelExtent {
      std::uint32_t width{0};  ///< Width in pixels.
      std::uint32_t height{0}; ///< Height in pixels.
   };

   /// @brief Strong wrapper for human-readable object names.
   struct ObjectName {
      std::string value{}; ///< Wrapped display or diagnostic name.
   };

   /// @brief Strong wrapper for renderer selection identifiers.
   struct RendererId {
      std::string value{}; ///< Wrapped renderer identifier.
   };

   /// @brief Strong wrapper for frame counts and frame indices.
   struct FrameCount {
      std::uint64_t value{0}; ///< Wrapped frame count.
   };

   /// @brief Strong wrapper for source vertex counts.
   struct VertexCount {
      std::uint64_t value{0}; ///< Wrapped vertex count.
   };

   /// @brief Strong wrapper for source index counts.
   struct IndexCount {
      std::uint64_t value{0}; ///< Wrapped index count.
   };

   /// @brief Strong wrapper for texture channel counts.
   struct TextureChannelCount {
      std::uint32_t value{0}; ///< Wrapped channel count.
   };

   /// @brief Type-safe handle wrapper; categories share 64-bit storage but not the same C++ type.
   template <typename TTag> struct TypedHandle {
      static constexpr std::uint32_t generation_bits{16};                 ///< Future generation bit count.
      static constexpr std::uint32_t id_bits{64 - generation_bits - 1};    ///< Counter/id bit count.
      static constexpr std::uint64_t counter_bit{1ULL << 63U};             ///< High bit marks counter handles.
      static constexpr std::uint64_t id_mask{(1ULL << id_bits) - 1ULL};    ///< Low id/index bits.
      static constexpr std::uint64_t generation_mask{~counter_bit & ~id_mask}; ///< Middle generation bits.

      std::uint64_t value{0}; ///< Raw 64-bit handle value; zero is invalid.

      /// @brief Returns true when this handle is not the invalid zero value.
      [[nodiscard]] constexpr bool valid() const noexcept { return value != 0; }

      /// @brief Returns true when the handle stores an upward-counted id.
      [[nodiscard]] constexpr bool isCounter() const noexcept { return (value & counter_bit) != 0; }

      /// @brief Returns true when the handle is shaped as a future slot-map index.
      [[nodiscard]] constexpr bool isSlotMapIndex() const noexcept { return valid() && !isCounter(); }

      /// @brief Extracts the future slot-map generation counter.
      [[nodiscard]] constexpr std::uint64_t generation() const noexcept { return (value & generation_mask) >> id_bits; }

      /// @brief Extracts the low id bits used by both counter and slot-map handles.
      [[nodiscard]] constexpr std::uint64_t id() const noexcept { return value & id_mask; }

      /// @brief Names the id bits as a slot index for future slot-map users.
      [[nodiscard]] constexpr std::uint64_t slotIndex() const noexcept { return id(); }

      [[nodiscard]] friend constexpr bool operator==(TypedHandle, TypedHandle) noexcept = default;
      [[nodiscard]] friend constexpr auto operator<=>(TypedHandle, TypedHandle) noexcept = default;
   };

   static_assert(sizeof(TypedHandle<decltype([] {})>) == sizeof(std::uint64_t));

   namespace detail {

      /// @brief Returns the next process-local id used by all typed counter handles.
      [[nodiscard]] inline std::uint64_t nextCounterHandleId() {
         static std::atomic_uint64_t next_id{1};
         return next_id.fetch_add(1, std::memory_order_relaxed);
      }

   } // namespace detail

   /// @brief Builds a typed upward-counted non-slot-map handle from the module-global counter.
   template <typename THandle> [[nodiscard]] inline THandle makeCounterHandle() {
      const auto id = detail::nextCounterHandleId();
      return THandle{.value = THandle::counter_bit | (id & THandle::id_mask)};
   }

   /// @brief Builds a deterministic typed counter handle for tests and examples that need stable ids.
   template <typename THandle>
   [[nodiscard]] constexpr THandle makeHandleForTest(std::uint64_t id) noexcept {
      return THandle{.value = THandle::counter_bit | (id & THandle::id_mask)};
   }

   /// @brief Builds a deterministic future slot-map handle for tests of the prepared bit layout.
   template <typename THandle>
   [[nodiscard]] constexpr THandle makeSlotMapHandleForTest(std::uint32_t slot_index,
                                                            std::uint32_t generation) noexcept {
      const auto generation_bits = (static_cast<std::uint64_t>(generation) << THandle::id_bits) &
                                   THandle::generation_mask;
      const auto index_bits = static_cast<std::uint64_t>(slot_index) & THandle::id_mask;
      return THandle{.value = generation_bits | index_bits};
   }

   using Entity           = TypedHandle<decltype([] {})>; ///< Strong handle for ECS entities.
   using SceneHandle      = TypedHandle<decltype([] {})>; ///< Strong handle for scene descriptors.
   using NodeHandle       = TypedHandle<decltype([] {})>; ///< Strong handle for scene-node descriptors.
   using MeshHandle       = TypedHandle<decltype([] {})>; ///< Strong handle for mesh descriptors.
   using MaterialHandle   = TypedHandle<decltype([] {})>; ///< Strong handle for material descriptors.
   using TextureHandle    = TypedHandle<decltype([] {})>; ///< Strong handle for texture descriptors.
   using LightHandle      = TypedHandle<decltype([] {})>; ///< Strong handle for light descriptors.
   using CameraHandle     = TypedHandle<decltype([] {})>; ///< Strong handle for camera descriptors.
   using WindowHandle     = TypedHandle<decltype([] {})>; ///< Strong handle for runtime windows.

   /// @brief Standard transform component shared by all active engine layers.
   struct Transform {
      Position translation{}; ///< Local or world-space translation.
      Rotation rotation{};    ///< Local or world-space orientation.
      Scale scale{};          ///< Local or world-space non-uniform scale.
   };

   /// @brief Axis-aligned bounds described by minimum and maximum positions.
   struct Bounds {
      Position minimum{}; ///< Minimum corner.
      Position maximum{}; ///< Maximum corner.
      bool valid{false};  ///< False until at least one point has been included.
   };

   /// @brief Public camera description used by game code and renderers.
   struct Camera {
      /// @brief World-space camera position.
      Position position{.value = math::Vec3(math::zero(), static_cast<math::Scalar>(1.5),
                                            static_cast<math::Scalar>(6.0))};
      /// @brief View direction.
      Direction forward{.value = math::Vec3(math::zero(), math::zero(), -math::one())};
      /// @brief World-to-view transform.
      math::Mat4 view_transform{math::translate(
          math::identityMat4(),
          math::Vec3(math::zero(), static_cast<math::Scalar>(-1.5),
                     static_cast<math::Scalar>(-6.0)))};
      FovY fov_y{};      ///< Vertical field of view.
      ClipPlanes clip{}; ///< Near/far clip planes.

      /// @brief Builds a camera from an eye position and target point.
      [[nodiscard]] static Camera lookAt(Position position, Position target,
                                         Direction up = Direction{
                                            .value = math::Vec3(math::zero(), math::one(), math::zero())},
                                         FovY fov_y = {}, ClipPlanes clip = {}) {
         Camera camera{};
         camera.position = position;
         camera.forward = Direction{.value = math::subtract(target.value, position.value)};
         camera.view_transform = math::lookAt(position.value, target.value, up.value);
         camera.fov_y = fov_y;
         camera.clip = clip;
         return camera;
      }

   };

   /// @brief Material texture slot meaning visible to apps inspecting imported materials.
   enum class TextureSemantic {
      unknown,    ///< Unclassified texture use.
      base_color, ///< Color/albedo texture.
      normal,     ///< Tangent-space normal texture.
      roughness,  ///< Roughness texture.
      metallic,   ///< Metallic texture.
      emissive,   ///< Emissive texture.
      occlusion   ///< Ambient-occlusion texture.
   };

   /// @brief High-level light shape visible to apps creating or inspecting lights.
   enum class LightKind {
      unknown,     ///< Unclassified light.
      directional, ///< Direction-only light such as the sun.
      point,       ///< Point light with position.
      spot         ///< Spot light with position and direction.
   };

   /// @brief A material reference to one texture descriptor.
   struct TextureBinding {
      TextureHandle texture{};                            ///< Referenced texture handle.
      TextureSemantic semantic{TextureSemantic::unknown}; ///< Intended material slot.
      std::uint32_t uv_set{0};                            ///< UV channel used by the texture.
   };

   /// @brief A scene node reference to renderable geometry and material.
   struct MeshUse {
      MeshHandle mesh{};         ///< Referenced mesh handle.
      MaterialHandle material{}; ///< Referenced material handle.
   };

   /// @brief Tree topology: one root plus parent-to-child handle edges.
   template <typename THandle> struct BasicTree {
      THandle root{};                             ///< Root node handle.
      std::multimap<THandle, THandle> children{}; ///< Parent node handle mapped to child node handles.

      /// @brief Adds one parent-to-child tree edge.
      void addChild(THandle parent, THandle child) { children.emplace(parent, child); }

      /// @brief Returns all children for a parent handle.
      [[nodiscard]] auto childRange(THandle parent) const { return children.equal_range(parent); }

   };

   using Tree = BasicTree<NodeHandle>; ///< Scene-tree topology uses node handles.

   /// @brief Scene graph node descriptor stored by handle in ObjectCatalog.
   struct NodeDescriptor {
      using HandleType = NodeHandle; ///< Descriptor handle type.
      NodeHandle handle{};           ///< Stable node handle.
      ObjectName name{};             ///< Human-readable node name.
      Transform transform{};         ///< Local transform.
      Vector<MeshUse> meshes{};      ///< Mesh/material pairs attached to this node.
   };

   /// @brief Mesh geometry descriptor; actual vertex buffers are added later.
   struct MeshDescriptor {
      using HandleType = MeshHandle; ///< Descriptor handle type.
      MeshHandle handle{};           ///< Stable mesh handle.
      ObjectName name{};             ///< Human-readable mesh name.
      VertexCount vertex_count{};    ///< Number of vertices in source geometry.
      IndexCount index_count{};      ///< Number of indices in source geometry.
      MaterialHandle material{};     ///< Default material handle.
      Bounds bounds{};               ///< Object-space bounds.
   };

   /// @brief Material descriptor referencing textures by handle.
   struct MaterialDescriptor {
      using HandleType = MaterialHandle; ///< Descriptor handle type.
      MaterialHandle handle{};           ///< Stable material handle.
      ObjectName name{};                 ///< Human-readable material name.
      Vector<TextureBinding> textures{}; ///< Texture slots used by this material.
   };

   /// @brief Texture descriptor; pixel storage and GPU upload are engine implementation work.
   struct TextureDescriptor {
      using HandleType = TextureHandle;    ///< Descriptor handle type.
      TextureHandle handle{};              ///< Stable texture handle.
      ObjectName name{};                   ///< Human-readable texture name.
      std::filesystem::path source{};      ///< Source file path or logical asset path.
      PixelExtent extent{};                ///< Source dimensions in pixels.
      TextureChannelCount channels{};      ///< Source channel count.
   };

   /// @brief Light descriptor used by apps and renderers.
   struct LightDescriptor {
      using HandleType = LightHandle;     ///< Descriptor handle type.
      LightHandle handle{};               ///< Stable light handle.
      ObjectName name{};                  ///< Human-readable light name.
      LightKind kind{LightKind::unknown}; ///< Light shape.
      Position position{};                ///< Light position for point/spot lights.
      Direction direction{};              ///< Light direction for directional/spot lights.
      LinearColor color{};                ///< Linear light color.
      LightIntensity intensity{};         ///< Relative light intensity.
   };

   /// @brief Camera descriptor used to create runtime cameras.
   struct CameraDescriptor {
      using HandleType = CameraHandle; ///< Descriptor handle type.
      CameraHandle handle{};           ///< Stable camera handle.
      ObjectName name{};               ///< Human-readable camera name.
      Position position{};             ///< Camera position.
      Direction forward{};             ///< Camera forward direction.
      FovY fov_y{};                    ///< Vertical field of view.
      ClipPlanes clip{};               ///< Near and far clipping planes.
   };

   /// @brief Scene descriptor stores only handles to objects kept in descriptor maps.
   struct SceneDescriptor {
      using HandleType = SceneHandle;     ///< Descriptor handle type.
      SceneHandle handle{};               ///< Stable scene handle.
      ObjectName name{};                  ///< Human-readable scene name.
      Tree tree{};                        ///< Scene hierarchy; nodes do not store child vectors.
      Vector<NodeHandle> nodes{};         ///< All node handles in the scene.
      Vector<MeshHandle> meshes{};        ///< Mesh handles used by the scene.
      Vector<MaterialHandle> materials{}; ///< Material handles used by the scene.
      Vector<TextureHandle> textures{};   ///< Texture handles used by the scene.
      Vector<LightHandle> lights{};       ///< Light handles used by the scene.
      Vector<CameraHandle> cameras{};     ///< Camera handles used by the scene.
   };

   /// @brief Simple descriptor table keyed by each descriptor's typed handle.
   template <typename TDescriptor> class DescriptorMap {
   public:
      using HandleType = typename TDescriptor::HandleType; ///< Strong handle accepted by this map.

      /// @brief Inserts a descriptor; descriptors must expose a valid `handle` member.
      [[nodiscard]] std::expected<void, Error> add(TDescriptor descriptor) {
         if (!descriptor.handle.valid()) { return std::unexpected(Error::invalid_handle); }
         const auto [_, inserted] = descriptors_.emplace(descriptor.handle, std::move(descriptor));
         if (!inserted) { return std::unexpected(Error::duplicate_object); }
         return {};
      }

      /// @brief Finds a descriptor by handle, or returns null.
      [[nodiscard]] const TDescriptor *find(HandleType handle) const {
         const auto it = descriptors_.find(handle);
         return it == descriptors_.end() ? nullptr : std::addressof(it->second);
      }

      /// @brief Finds a mutable descriptor by handle, or returns null.
      [[nodiscard]] TDescriptor *find(HandleType handle) {
         const auto it = descriptors_.find(handle);
         return it == descriptors_.end() ? nullptr : std::addressof(it->second);
      }

      /// @brief Returns true when the map contains the handle.
      [[nodiscard]] bool contains(HandleType handle) const { return descriptors_.contains(handle); }

      /// @brief Returns descriptor count.
      [[nodiscard]] std::size_t size() const { return descriptors_.size(); }

      /// @brief Exposes read-only descriptor storage for tests and iteration.
      [[nodiscard]] const std::map<HandleType, TDescriptor> &all() const { return descriptors_; }

   private:
      std::map<HandleType, TDescriptor> descriptors_{}; ///< Ordered descriptor storage.
   };

   /// @brief Central imported-object catalog; every loaded object is found by 64-bit handle.
   struct ObjectCatalog {
      DescriptorMap<SceneDescriptor> scenes{};       ///< Scenes by handle.
      DescriptorMap<NodeDescriptor> nodes{};         ///< Nodes by handle.
      DescriptorMap<MeshDescriptor> meshes{};        ///< Meshes by handle.
      DescriptorMap<MaterialDescriptor> materials{}; ///< Materials by handle.
      DescriptorMap<TextureDescriptor> textures{};   ///< Textures by handle.
      DescriptorMap<LightDescriptor> lights{};       ///< Lights by handle.
      DescriptorMap<CameraDescriptor> cameras{};     ///< Cameras by handle.
   };

   /// @brief Human-readable application name used by the platform layer.
   struct ApplicationName {
      std::string value{"v4"}; ///< Name shown in diagnostics and default window titles.
   };

   /// @brief Optional frame cap; zero means the engine runs until a window asks to close.
   struct MaxFrames {
      FrameCount value{}; ///< Maximum number of step() calls before the engine stops.
   };

   /// @brief Window creation descriptor kept deliberately close to v3's public shape.
   struct WindowDesc {
      std::string id{"main"};       ///< Stable application-local window id.
      std::string title{"VVE v4"};  ///< Platform window title.
      PixelExtent extent{.width = 960, .height = 540}; ///< Initial pixel dimensions.
      std::optional<int> x{};       ///< Optional initial screen x coordinate.
      std::optional<int> y{};       ///< Optional initial screen y coordinate.
      RendererId renderer_id{};     ///< Renderer id selected for this window.
      bool resizable{true};         ///< Enables platform resizing.
      bool visible{true};           ///< Shows the window after creation.
   };

   /// @brief Collection wrapper for all windows created during engine init().
   struct Windows {
      Vector<WindowDesc> value{WindowDesc{}}; ///< Startup windows; defaults to one main window.
   };

   /// @brief Runtime window state exposed through World.
   struct WindowInfo {
      WindowHandle handle{};    ///< 64-bit runtime window handle.
      std::string id{};         ///< Stable id copied from WindowDesc.
      std::string title{};      ///< Current platform title.
      PixelExtent extent{};     ///< Current pixel dimensions.
      RendererId renderer_id{}; ///< Renderer id selected for this window.
      bool focused{false};      ///< True while the window has keyboard focus.
      bool minimized{false};    ///< True while the platform reports a minimized window.
      bool should_close{false}; ///< True after a close request.
   };

   /// @brief Per-step timing and frame index passed to user systems.
   struct FrameContext {
      FrameCount frame_index{}; ///< Zero-based frame index.
      DeltaTime delta_time{};   ///< Elapsed wall-clock time since the previous frame.
   };

   /// @brief Snapshot passed to user systems that want window data for the current frame.
   struct WindowFrameData {
      Vector<WindowInfo> windows{}; ///< Window states after event polling.
   };

   /// @brief Keyboard and mouse snapshot; held keys are independent of OS key-repeat speed.
   class InputState {
   public:
      /// @brief Starts a new input frame while preserving held-key state.
      void beginFrame() {
         keys_pressed_.clear();
         keys_released_.clear();
         mouse_delta_.clear();
         mouse_wheel_delta_.clear();
      }

      /// @brief Records a key as currently held without generating a fresh press edge.
      void holdKey(std::int32_t keycode) { keys_down_.insert(keycode); }

      /// @brief Records a key-down edge and held state.
      void pressKey(std::int32_t keycode) {
         if (!keys_down_.contains(keycode)) { keys_pressed_.insert(keycode); }
         keys_down_.insert(keycode);
      }

      /// @brief Records a key-up edge and clears held state.
      void releaseKey(std::int32_t keycode) {
         keys_down_.erase(keycode);
         keys_released_.insert(keycode);
      }

      /// @brief Stores the latest mouse position for one window.
      void setMousePosition(WindowHandle window, Vec2 position) { mouse_position_[window] = position; }

      /// @brief Accumulates mouse movement for the current frame.
      void addMouseDelta(WindowHandle window, Vec2 delta) {
         const auto [it, _] = mouse_delta_.try_emplace(window, Vec2{zero(), zero()});
         it->second = math::add(it->second, delta);
      }

      /// @brief Accumulates mouse-wheel movement for the current frame.
      void addMouseWheelDelta(WindowHandle window, Vec2 delta) {
         const auto [it, _] = mouse_wheel_delta_.try_emplace(window, Vec2{zero(), zero()});
         it->second = math::add(it->second, delta);
      }

      /// @brief Returns whether a key is currently held down.
      [[nodiscard]] bool isKeyDown(std::int32_t keycode) const { return keys_down_.contains(keycode); }

      /// @brief Returns whether a key was pressed during the current frame.
      [[nodiscard]] bool wasKeyPressed(std::int32_t keycode) const { return keys_pressed_.contains(keycode); }

      /// @brief Returns whether a key was released during the current frame.
      [[nodiscard]] bool wasKeyReleased(std::int32_t keycode) const { return keys_released_.contains(keycode); }

      /// @brief Returns the latest mouse position for a window, if any motion event was seen.
      [[nodiscard]] std::optional<Vec2> mousePosition(WindowHandle window) const {
         const auto it = mouse_position_.find(window);
         return it == mouse_position_.end() ? std::optional<Vec2>{} : std::optional<Vec2>{it->second};
      }

      /// @brief Returns accumulated mouse delta for a window in the current frame.
      [[nodiscard]] Vec2 mouseDelta(WindowHandle window) const {
         const auto it = mouse_delta_.find(window);
         return it == mouse_delta_.end() ? Vec2{} : it->second;
      }

      /// @brief Returns accumulated mouse-wheel delta for a window in the current frame.
      [[nodiscard]] Vec2 mouseWheelDelta(WindowHandle window) const {
         const auto it = mouse_wheel_delta_.find(window);
         return it == mouse_wheel_delta_.end() ? Vec2{} : it->second;
      }

   private:
      std::set<std::int32_t> keys_down_{};               ///< Keys currently held down.
      std::set<std::int32_t> keys_pressed_{};            ///< Keys pressed this frame.
      std::set<std::int32_t> keys_released_{};           ///< Keys released this frame.
      std::map<WindowHandle, Vec2> mouse_position_{};    ///< Last mouse position by window.
      std::map<WindowHandle, Vec2> mouse_delta_{};       ///< Frame-local mouse delta by window.
      std::map<WindowHandle, Vec2> mouse_wheel_delta_{}; ///< Frame-local wheel delta by window.
   };

   /// @brief Default ECS trait reserved for future slot-map policy knobs.
   struct DefaultECSTraits {
      static constexpr bool use_slot_map_handles{false}; ///< Slot maps are not implemented in this skeleton yet.
   };

   /// @brief Small component store keyed by 64-bit entity handles.
   template <typename TTraits = DefaultECSTraits> class BasicECS {
      /// @brief Type-erased base so destruction can erase components from every pool.
      struct PoolBase {
         virtual ~PoolBase() = default;
         virtual void erase(Entity entity) = 0;
      };

      /// @brief Concrete component pool for one component type.
      template <typename T> struct Pool final : PoolBase {
         std::map<Entity, T> data{}; ///< Components keyed by owning entity handle.
         void erase(Entity entity) override { data.erase(entity); }

      };

   public:
      /// @brief Creates a live entity with a fresh 64-bit handle.
      [[nodiscard]] Entity create() {
         static_assert(!TTraits::use_slot_map_handles, "facade prepares slot-map handles but has no slot map yet");
         const auto entity = makeCounterHandle<Entity>();
         alive_.insert(entity);
         return entity;
      }

      /// @brief Returns whether an entity handle is live in this ECS.
      [[nodiscard]] bool exists(Entity entity) const { return alive_.contains(entity); }

      /// @brief Erases a live entity and all of its attached components.
      [[nodiscard]] std::expected<void, Error> erase(Entity entity) {
         if (!alive_.erase(entity)) { return std::unexpected(Error::invalid_handle); }
         for (auto &[_, pool] : pools_) { pool->erase(entity); }
         return {};
      }

      /// @brief Adds a component; fails if the entity is invalid or already has the component.
      template <typename T>
      [[nodiscard]] std::expected<void, Error> add(Entity entity, T component) {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         auto &data = pool<T>().data;
         const auto [_, inserted] = data.emplace(entity, std::move(component));
         if (!inserted) { return std::unexpected(Error::duplicate_component); }
         return {};
      }

      /// @brief Reads a required component by value.
      template <typename T>
      [[nodiscard]] std::expected<T, Error> get(Entity entity) const {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         const auto *typed_pool = findPool<T>();
         if (typed_pool == nullptr) { return std::unexpected(Error::missing_component); }
         const auto it = typed_pool->data.find(entity);
         if (it == typed_pool->data.end()) { return std::unexpected(Error::missing_component); }
         return it->second;
      }

      /// @brief Reads an optional component; invalid entities still produce an error.
      template <typename T>
      [[nodiscard]] std::expected<std::optional<T>, Error> tryGet(Entity entity) const {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         const auto *typed_pool = findPool<T>();
         if (typed_pool == nullptr) { return std::optional<T>{}; }
         const auto it = typed_pool->data.find(entity);
         return it == typed_pool->data.end() ? std::optional<T>{} : std::optional<T>{it->second};
      }

      /// @brief Inserts or replaces a component on a live entity.
      template <typename T>
      [[nodiscard]] std::expected<void, Error> put(Entity entity, T component) {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         pool<T>().data.insert_or_assign(entity, std::move(component));
         return {};
      }

      /// @brief Tests whether a live entity owns a component type.
      template <typename T>
      [[nodiscard]] std::expected<bool, Error> has(Entity entity) const {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         const auto *typed_pool = findPool<T>();
         return typed_pool != nullptr && typed_pool->data.contains(entity);
      }

      /// @brief Removes a component if present; missing components are ignored.
      template <typename T>
      [[nodiscard]] std::expected<void, Error> remove(Entity entity) {
         if (!exists(entity)) { return std::unexpected(Error::invalid_handle); }
         if (auto *typed_pool = findPool<T>()) { typed_pool->data.erase(entity); }
         return {};
      }

      /// @brief Returns every live entity that owns all requested component types.
      template <typename... T>
      [[nodiscard]] Vector<Entity> view() const {
         Vector<Entity> result{};
         for (const auto entity : alive_) {
            if ((contains<T>(entity) && ...)) { result.push_back(entity); }
         }
         return result;
      }

   private:
      /// @brief Creates or returns the pool for component type T.
      template <typename T> [[nodiscard]] Pool<T> &pool() {
         const std::type_index key{typeid(T)};
         auto &slot = pools_[key];
         if (!slot) { slot = std::make_unique<Pool<T>>(); }
         return static_cast<Pool<T> &>(*slot);
      }

      /// @brief Finds the const pool for component type T, or null if unused.
      template <typename T> [[nodiscard]] const Pool<T> *findPool() const {
         const auto it = pools_.find(std::type_index(typeid(T)));
         return it == pools_.end() ? nullptr : static_cast<const Pool<T> *>(it->second.get());
      }

      /// @brief Finds the mutable pool for component type T, or null if unused.
      template <typename T> [[nodiscard]] Pool<T> *findPool() {
         const auto it = pools_.find(std::type_index(typeid(T)));
         return it == pools_.end() ? nullptr : static_cast<Pool<T> *>(it->second.get());
      }

      /// @brief Checks raw component-pool membership without validating liveness.
      template <typename T> [[nodiscard]] bool contains(Entity entity) const {
         const auto *typed_pool = findPool<T>();
         return typed_pool != nullptr && typed_pool->data.contains(entity);
      }

      std::set<Entity> alive_{};                                    ///< Live entity handles.
      std::map<std::type_index, std::unique_ptr<PoolBase>> pools_{}; ///< Component pools by type.
   };

   using ECS = BasicECS<>; ///< Default ECS type owned by the active engine implementation.

   /// @brief User-visible state facade used by examples and systems.
   class World {
   public:
      /// @brief Creates a facade over ECS storage owned by a higher-level runtime.
      explicit World(ECS &ecs) noexcept : ecs_(ecs) {}

      /// @brief Returns the runtime ECS.
      [[nodiscard]] ECS &ecs() { return ecs_; }

      /// @brief Returns the runtime ECS.
      [[nodiscard]] const ECS &ecs() const { return ecs_; }

      /// @brief Returns the current input snapshot.
      [[nodiscard]] InputState &input() { return input_; }

      /// @brief Returns the current input snapshot.
      [[nodiscard]] const InputState &input() const { return input_; }

      /// @brief Returns mutable runtime window states for engine synchronization.
      [[nodiscard]] Vector<WindowInfo> &windows() { return windows_; }

      /// @brief Returns read-only runtime window states.
      [[nodiscard]] const Vector<WindowInfo> &windows() const { return windows_; }

      /// @brief Finds a mutable window by string id.
      [[nodiscard]] WindowInfo *findWindow(std::string_view id) {
         const auto it = std::ranges::find_if(windows_, [id](const WindowInfo &window) { return window.id == id; });
         return it == windows_.end() ? nullptr : std::addressof(*it);
      }

      /// @brief Finds a read-only window by string id.
      [[nodiscard]] const WindowInfo *findWindow(std::string_view id) const {
         const auto it = std::ranges::find_if(windows_, [id](const WindowInfo &window) { return window.id == id; });
         return it == windows_.end() ? nullptr : std::addressof(*it);
      }

      /// @brief Creates an entity and attaches all provided components.
      template <typename... TComponents>
      [[nodiscard]] std::expected<Entity, Error> spawn(TComponents &&...components) {
         const Entity entity = ecs_.create();
         if constexpr (sizeof...(TComponents) > 0) {
            if ((!ecs_.add(entity, std::forward<TComponents>(components)) || ...)) {
               (void)ecs_.erase(entity);
               return std::unexpected(Error::duplicate_component);
            }
         }
         return entity;
      }

      /// @brief Adds one component to an existing entity.
      template <typename T> [[nodiscard]] std::expected<void, Error> addComponent(Entity entity, T component) {
         return ecs_.add(entity, std::move(component));
      }

      /// @brief Inserts or replaces one component on an existing entity.
      template <typename T> [[nodiscard]] std::expected<void, Error> setComponent(Entity entity, T component) {
         return ecs_.put(entity, std::move(component));
      }

      /// @brief Reads an optional component by value.
      template <typename T> [[nodiscard]] std::expected<std::optional<T>, Error> getComponent(Entity entity) const {
         return ecs_.tryGet<T>(entity);
      }

      /// @brief Destroys an entity and its components.
      [[nodiscard]] std::expected<void, Error> destroy(Entity entity) { return ecs_.erase(entity); }

      /// @brief Convenience wrapper for setting the Transform component.
      [[nodiscard]] std::expected<void, Error> setTransform(Entity entity, Transform transform) {
         return setComponent(entity, std::move(transform));
      }

      /// @brief Convenience wrapper for reading the Transform component.
      [[nodiscard]] std::expected<std::optional<Transform>, Error> getTransform(Entity entity) const {
         return getComponent<Transform>(entity);
      }

      /// @brief Stores the active camera handle; renderers can interpret it later.
      [[nodiscard]] std::expected<void, Error> setActiveCamera(Entity camera) {
         if (!ecs_.exists(camera)) { return std::unexpected(Error::invalid_handle); }
         active_camera_ = camera;
         return {};
      }

      /// @brief Returns the current active camera handle when one has been selected.
      [[nodiscard]] std::optional<Entity> activeCamera() const { return active_camera_; }

      /// @brief Installs the runtime scene loader used by loadScene().
      void setSceneLoader(std::function<std::expected<SceneHandle, Error>(const std::filesystem::path &)> loader) {
         scene_loader_ = std::move(loader);
      }

      /// @brief Installs a read-only object-catalog provider for examples and diagnostics.
      void setCatalogProvider(std::function<const ObjectCatalog *()> provider) {
         catalog_provider_ = std::move(provider);
      }

      /// @brief Imports a scene through the runtime loader and returns the scene handle.
      [[nodiscard]] std::expected<SceneHandle, Error> loadScene(const std::filesystem::path &path) {
         if (!scene_loader_) { return std::unexpected(Error::missing_object); }
         return scene_loader_(path);
      }

      /// @brief Returns the runtime object catalog when an engine has connected one.
      [[nodiscard]] const ObjectCatalog *objectCatalog() const {
         return catalog_provider_ ? catalog_provider_() : nullptr;
      }

   private:
      ECS &ecs_;                          ///< Non-owning runtime entity/component storage.
      InputState input_{};                ///< Current input snapshot.
      Vector<WindowInfo> windows_{};      ///< Current platform windows.
      std::optional<Entity> active_camera_{}; ///< Optional camera selected by the application.
      /// @brief Runtime callback that imports a scene into the asset catalog.
      std::function<std::expected<SceneHandle, Error>(const std::filesystem::path &)> scene_loader_{};
      std::function<const ObjectCatalog *()> catalog_provider_{}; ///< Runtime catalog access hook.
   };

   /// @brief Backward-compatible compact config; typed options are preferred for new examples.
   struct EngineConfig {
      std::string application_name{"v4"}; ///< Human-readable application name.
      FrameCount max_frames{};            ///< Zero means no frame limit.
   };

   /// @brief Result of one engine frame.
   enum class FrameStatus {
      running,                    ///< Engine can continue stepping.
      stopped,                    ///< Engine stopped because a close request or frame cap was reached.
      continue_running = running, ///< Alias kept close to v3-style wording.
      should_close     = stopped  ///< Alias kept close to v3-style wording.
   };

   /// @brief Heterogeneous user-system storage used by makeEngine().
   template <typename... TSystems> struct UserSystems {
      std::tuple<TSystems...> value{}; ///< User systems stored by value.
   };

   /// @brief Builds a user-system bundle while preserving concrete system types.
   template <typename... TSystems> [[nodiscard]] auto makeUserSystems(TSystems &&...systems) {
      return UserSystems<std::remove_cvref_t<TSystems>...>{
         .value = std::tuple<std::remove_cvref_t<TSystems>...>{std::forward<TSystems>(systems)...}};
   }

} // namespace vve
