export module VEEngine.V3:Types;
// This module gathers the lightweight value types that define the contracts
// shared between the v3 subsystem facades.
export import :Vector;
import VEEngine;
import std;

/**
 * @file
 * @brief Public v3 data types for resources, tasks, rendering, and runtime state.
 *
 * These types form the public contract shared across v3 subsystem facades and
 * frame-graph/task-graph APIs.
 */
export namespace vve::v3 {

   /// @brief Coarse classification of engine-managed resource records.
   enum class ResourceKind {
      unknown,        ///< Resource kind is not yet known.
      mesh,           ///< Mesh resource.
      texture,        ///< Texture resource.
      material,       ///< Material resource.
      shader_program, ///< Shader program resource.
      buffer,         ///< Generic buffer resource.
      image           ///< Generic image resource.
   };

   /// @brief Current storage location or provenance of a resource record.
   enum class ResourceLocation {
      unknown,       ///< Resource location is not yet known.
      source_file,   ///< Resource originates directly from a source file.
      imported_blob, ///< Resource exists as imported intermediate data.
      cpu_memory,    ///< Resource resides in CPU-visible memory.
      gpu_memory,    ///< Resource resides in GPU memory.
      streaming      ///< Resource is managed through a streaming path.
   };

   /// @brief Built-in engine task kernels recognized by the v3 scheduler.
   enum class TaskKernelId : std::uint32_t {
      none = 0,             ///< No built-in kernel classification.
      begin_frame,          ///< Backend or runtime begin-frame work.
      poll_window_events,   ///< Platform window-event polling.
      update_transforms,    ///< Scene transform propagation.
      sample_animations,    ///< Animation sampling before scene evaluation.
      cull_visibility_cpu,  ///< CPU-side visibility determination.
      cull_visibility_gpu,  ///< GPU-side visibility determination.
      build_draw_packets,   ///< Draw-packet generation for rendering.
      upload_resources,     ///< Resource upload and residency work.
      record_render_graph,  ///< Render-graph command recording.
      consume_frame_output, ///< Presentation or final frame-output consumption.
      end_frame             ///< Backend or runtime end-frame work.
   };

   /// @brief High-level phase used to group task execution within a frame.
   enum class TaskPhase : std::uint32_t {
      automatic = 0, ///< Phase should be inferred from the task kernel.
      begin_frame,   ///< Earliest frame-boundary work.
      input,         ///< Platform input and window-event processing.
      user_update,   ///< User-system update work.
      scene,         ///< Scene and simulation preparation work.
      resources,     ///< Resource upload or residency work.
      render,        ///< Render preparation and submission work.
      end_frame,     ///< End-of-frame synchronization and teardown work.
      post_frame     ///< Work intentionally deferred until after frame completion.
   };

   /// @brief Window event kinds emitted by the window system.
   enum class WindowEventType : std::uint32_t {
      none = 0,          ///< No concrete window event.
      close_requested,   ///< Window close was requested.
      resized,           ///< Window size changed.
      moved,             ///< Window position changed.
      focus_gained,      ///< Window gained input focus.
      focus_lost,        ///< Window lost input focus.
      key_down,          ///< Keyboard key was pressed.
      key_held,          ///< Keyboard key is currently held.
      key_up,            ///< Keyboard key was released.
      mouse_move,        ///< Mouse cursor moved within the window.
      mouse_button_down, ///< Mouse button was pressed.
      mouse_button_up,   ///< Mouse button was released.
      mouse_wheel        ///< Mouse wheel delta was reported.
   };

   /// @brief Built-in render pass kernels recognized by the renderer.
   enum class RenderKernelId : std::uint32_t {
      none = 0,           ///< No built-in render kernel classification.
      depth_prepass,      ///< Depth-only prepass.
      forward_opaque,     ///< Forward opaque shading pass.
      deferred_gbuffer,   ///< Deferred G-buffer generation pass.
      deferred_lighting,  ///< Deferred lighting resolve pass.
      path_trace,         ///< Path-tracing pass.
      shadow_map,         ///< Shadow-map generation pass.
      ray_traced_shadows, ///< Ray-traced shadow pass.
      post_process,       ///< Main post-processing pass.
      post_post_process,  ///< Final post-processing pass after the main stack.
      imgui               ///< Dear ImGui overlay pass.
   };

   /// @brief Shader stage kinds reported by shader reflection.
   enum class ShaderStage : std::uint32_t {
      vertex = 0, ///< Vertex shader stage.
      fragment,   ///< Fragment shader stage.
      compute     ///< Compute shader stage.
   };

   /// @brief Strong type for mesh resource identifiers.
   struct MeshHandle final {
      vve::Handle value{}; ///< Underlying generic handle value.
   };

   /// @brief Strong type for texture resource identifiers.
   struct TextureHandle final {
      vve::Handle value{}; ///< Underlying generic handle value.
   };

   /// @brief Strong type for material resource identifiers.
   struct MaterialHandle final {
      vve::Handle value{}; ///< Underlying generic handle value.
   };

   /// @brief Strong type for shader resource identifiers.
   struct ShaderHandle final {
      vve::Handle value{}; ///< Underlying generic handle value.
   };

   /// @brief Strong type for scene identifiers.
   struct SceneHandle final {
      vve::Handle value{}; ///< Underlying generic handle value.
   };

   /// @brief Strong type for scene-node identifiers.
   struct SceneNodeHandle final {
      vve::Handle value{}; ///< Underlying generic handle value.
   };

   /// @brief Strong type for task-node identifiers.
   struct TaskNodeHandle final {
      vve::Handle value{}; ///< Underlying generic handle value.
   };

   /// @brief Strong type for render-pass identifiers.
   struct RenderPassHandle final {
      vve::Handle value{}; ///< Underlying generic handle value.
   };

   /// @brief Strong type for window identifiers inside v3 systems.
   struct WindowHandle final {
      vve::Handle value{}; ///< Underlying generic handle value.
   };

   /// @brief Per-frame timing data passed to runtime systems and tasks.
   struct FrameContext {
      std::uint64_t frame_index{0}; ///< Monotonically increasing frame index.
      double delta_seconds{0.0};    ///< Frame delta time in seconds.
   };

   /// @brief Declares a task's read or write access to a resource handle.
   struct ResourceAccess {
      vve::Handle resource{}; ///< Resource touched by the task.
      bool write{false};      ///< Whether the access writes to the resource.
   };

   /// @brief High-level meaning assigned to a referenced texture.
   enum class TextureSemantic : std::uint32_t {
      unknown = 0,        ///< Texture role is not known.
      base_color,         ///< Base-color or diffuse color texture.
      normal,             ///< Normal-map texture.
      metallic_roughness, ///< Combined metallic-roughness texture.
      roughness,          ///< Roughness-only texture.
      metallic,           ///< Metallic-only texture.
      specular,           ///< Specular response texture.
      emissive,           ///< Emissive color texture.
      opacity,            ///< Opacity or alpha texture.
      ambient_occlusion   ///< Ambient-occlusion texture.
   };

   /// @brief CPU-side vertex payload imported from a source scene.
   struct ImportedVertex {
      vve::math::Vec3 position{vve::math::zeroVec3()}; ///< Object-space vertex position.
      vve::math::Vec3 normal{vve::math::zeroVec3()};   ///< Object-space vertex normal.
      vve::math::Vec3 tangent{vve::math::zeroVec3()};  ///< Object-space tangent direction.
      vve::math::Vec3 bitangent{vve::math::zeroVec3()}; ///< Object-space bitangent direction.
      vve::math::Vec2 texcoord0{vve::math::Vec2(vve::math::zero(), vve::math::zero())}; ///< Primary texture coordinates.
      vve::math::Vec4 color0{vve::math::Vec4(vve::math::one(), vve::math::one(), vve::math::one(),
                                             vve::math::one())}; ///< Primary vertex color.
   };

   /// @brief One indexed primitive range inside an imported mesh.
   struct ImportedSubmesh {
      std::uint32_t index_offset{0}; ///< Offset into the mesh index buffer.
      std::uint32_t index_count{0};  ///< Number of indices in the primitive range.
      MaterialHandle material{};     ///< Material assigned to this primitive range.
   };

   /// @brief Imported mesh payload produced by the asset system.
   struct ImportedMesh {
      MeshHandle handle{};                 ///< Stable imported mesh handle.
      std::string name{};                  ///< Human-readable mesh name.
      Vector<ImportedVertex> vertices{};   ///< CPU-side imported vertex data.
      Vector<std::uint32_t> indices{};     ///< CPU-side imported index data.
      Vector<ImportedSubmesh> submeshes{}; ///< Primitive/material partitioning for the mesh.
      vve::math::Vec3 bounds_min{vve::math::zeroVec3()}; ///< Object-space minimum bounds corner.
      vve::math::Vec3 bounds_max{vve::math::zeroVec3()}; ///< Object-space maximum bounds corner.
      std::filesystem::path source_path{}; ///< Source scene file that produced the mesh.
   };

   /// @brief One texture reference owned by an imported material.
   struct ImportedTextureRef {
      TextureHandle texture{};                            ///< Stable referenced texture handle.
      TextureSemantic semantic{TextureSemantic::unknown}; ///< Meaning of the texture inside the material.
      std::uint32_t uv_set{0};                            ///< UV channel used by the texture.
   };

   /// @brief Imported texture reference produced by the asset system.
   struct ImportedTexture {
      TextureHandle handle{};              ///< Stable imported texture handle.
      std::string name{};                  ///< Human-readable texture name.
      std::filesystem::path resolved_path{}; ///< Canonicalized external path when the texture is file-backed.
      std::filesystem::path original_path{}; ///< Original texture path string reported by the importer.
      bool embedded{false};                ///< Whether the texture payload is embedded in the scene file.
      std::string embedded_id{};           ///< Embedded texture identifier when `embedded` is true.
   };

   /// @brief Imported material payload produced by the asset system.
   struct ImportedMaterial {
      MaterialHandle handle{}; ///< Stable imported material handle.
      std::string name{};      ///< Human-readable material name.
      Vector<ImportedTextureRef> textures{}; ///< Referenced textures preserved from the source material.
      vve::math::Vec4 base_color_factor{
          vve::math::Vec4(vve::math::one(), vve::math::one(), vve::math::one(), vve::math::one())}; ///< Base-color multiplier.
      vve::math::Vec3 emissive_factor{vve::math::zeroVec3()}; ///< Emissive color multiplier.
      vve::math::Scalar roughness_factor{vve::math::one()};   ///< Roughness scalar factor.
      vve::math::Scalar metallic_factor{vve::math::zero()};   ///< Metallic scalar factor.
      vve::math::Scalar normal_scale{vve::math::one()};       ///< Normal-map scale factor.
      vve::math::Scalar alpha_cutoff{vve::math::zero()};      ///< Alpha cutoff used by masked materials.
      bool double_sided{false};                               ///< Whether the material should render both sides.
      bool alpha_blend{false};                                ///< Whether the material expects alpha blending.
   };

   /// @brief One imported mesh instance attached to a scene node.
   struct ImportedMeshInstance {
      vve::Handle handle{};                          ///< Stable imported mesh-instance handle.
      MeshHandle mesh{};                             ///< Referenced imported mesh.
      std::optional<MaterialHandle> material_override{}; ///< Optional per-instance material override.
   };

   /// @brief Imported scene-node description with local transform data.
   struct ImportedSceneNode {
      SceneNodeHandle handle{}; ///< Stable imported scene-node handle.
      SceneNodeHandle parent{}; ///< Parent node handle, or default for a root node.
      std::string name{};       ///< Human-readable node name.
      /// @brief Local transform relative to the parent node.
      vve::math::Mat4 local_transform{vve::math::identityMat4()};
      Vector<ImportedMeshInstance> mesh_instances{}; ///< Mesh instances attached to this node.
   };

   /// @brief Scene asset payload produced by import and consumed by scene instantiation.
   struct ImportedScene {
      SceneHandle handle{};                 ///< Stable scene handle.
      std::string name{};                   ///< Human-readable scene name.
      std::filesystem::path source_path{};  ///< Source file from which the scene was imported.
      Vector<ImportedTexture> textures{};   ///< Imported textures referenced by scene materials.
      Vector<ImportedMesh> meshes{};        ///< Imported meshes owned by the scene.
      Vector<ImportedMaterial> materials{}; ///< Imported materials owned by the scene.
      Vector<ImportedSceneNode> nodes{};    ///< Imported scene-node hierarchy.
   };

   /// @brief Persistent record describing a registered engine resource.
   struct ResourceRecord {
      vve::Handle id{};                                     ///< Stable resource identifier.
      ResourceKind kind{ResourceKind::unknown};             ///< Resource category.
      ResourceLocation location{ResourceLocation::unknown}; ///< Current storage location or provenance.
      std::uint32_t generation{0};                          ///< Monotonic version incremented on significant transitions.
      std::filesystem::path source_path{};                  ///< Source file associated with the resource.
   };

   /// @brief Instantiated scene-node description used at runtime.
   struct SceneNodeDesc {
      SceneNodeHandle handle{};       ///< Stable runtime scene-node handle.
      SceneNodeHandle parent{};       ///< Parent node handle, or default for a root node.
      SceneNodeHandle first_child{};  ///< First child node handle, or default when the node has no children.
      SceneNodeHandle next_sibling{}; ///< Next sibling node handle in the parent's child list, or default when none exists.
      std::string name{};             ///< Human-readable node name.
      /// @brief Local transform relative to the parent node.
      vve::math::Mat4 local_transform{vve::math::identityMat4()};
      /// @brief Cached world transform updated during the scene phase.
      vve::math::Mat4 world_transform{vve::math::identityMat4()};
      std::uint64_t last_updated_frame{std::numeric_limits<std::uint64_t>::max()}; ///< Frame index of the last world-transform update.
   };

   /// @brief Runtime mesh-instance description attached to one scene node.
   struct SceneMeshInstanceDesc {
      vve::Handle handle{};                          ///< Stable runtime mesh-instance handle.
      SceneNodeHandle node{};                        ///< Owning runtime scene node.
      MeshHandle mesh{};                             ///< Referenced mesh handle.
      std::optional<MaterialHandle> material_override{}; ///< Optional per-instance material override.
   };

   /// @brief Runtime scene data shared across scene, resource, and render systems.
   struct SceneData {
      SceneHandle handle{}; ///< Stable runtime scene handle.
      std::string name{};   ///< Human-readable runtime scene name.
      std::filesystem::path source_path{}; ///< Source file from which the scene was loaded.
      Vector<ImportedTexture> textures{};  ///< Imported textures preserved in runtime scene storage.
      std::unordered_map<vve::Handle::value_type, std::size_t> texture_indices{}; ///< Handle-to-index lookup cache for textures.
      Vector<ImportedMesh> meshes{};       ///< Imported meshes preserved in runtime scene storage.
      std::unordered_map<vve::Handle::value_type, std::size_t> mesh_indices{}; ///< Handle-to-index lookup cache for meshes.
      Vector<ImportedMaterial> materials{}; ///< Imported materials preserved in runtime scene storage.
      std::unordered_map<vve::Handle::value_type, std::size_t> material_indices{}; ///< Handle-to-index lookup cache for materials.
      Vector<SceneNodeDesc> nodes{};        ///< Runtime scene-node list.
      std::unordered_map<vve::Handle::value_type, std::size_t> node_indices{}; ///< Handle-to-index lookup cache for runtime scene nodes.
      Vector<SceneMeshInstanceDesc> mesh_instances{}; ///< Runtime mesh instances attached to scene nodes.
      std::unordered_map<vve::Handle::value_type, std::size_t> mesh_instance_indices{}; ///< Handle-to-index lookup cache for runtime mesh instances.
   };

   /// @brief Runtime window snapshot used by the frame graph and world facade.
   struct WindowState {
      WindowHandle handle{};    ///< Stable runtime window handle.
      std::string id{};         ///< Stable string identifier.
      std::string title{};      ///< Human-readable title.
      std::uint32_t width{0};   ///< Current window width in pixels.
      std::uint32_t height{0};  ///< Current window height in pixels.
      bool focused{false};      ///< Whether the window is focused.
      bool minimized{false};    ///< Whether the window is minimized.
      bool should_close{false}; ///< Whether the window has requested closure.
   };

   /// @brief Window event payload emitted during event polling.
   struct WindowEvent {
      WindowHandle window{};                        ///< Window associated with the event.
      WindowEventType type{WindowEventType::none}; ///< Event type.
      std::int32_t a{0};                           ///< First event payload integer.
      std::int32_t b{0};                           ///< Second event payload integer.
   };

   /// @brief Frame-local window snapshot and event range bundle.
   struct WindowFrameData {
      VectorConstRange<WindowState> windows{}; ///< Current window-state range for the frame.
      VectorConstRange<WindowEvent> events{};  ///< Current event range for the frame.
   };

   /// @brief Scope used to distinguish global tasks from per-window tasks.
   enum class TaskScope : std::uint32_t {
      global = 0, ///< Task is global to the frame.
      window      ///< Task is scoped to one window.
   };

   /// @brief Runtime data visible to task callbacks during execution.
   struct TaskExecutionContext {
      const FrameContext *frame_context{nullptr};             ///< Frame timing data, or `nullptr` when unavailable.
      SceneData *scene{nullptr};                              ///< Mutable scene data, or `nullptr` when unavailable.
      vve::World *world{nullptr};                             ///< World facade, or `nullptr` when unavailable.
      std::shared_ptr<const WindowFrameData> window_frame{}; ///< Shared window snapshot for the frame.
      std::optional<WindowHandle> window{};                  ///< Optional window scope when the task is window-specific.
   };

   /**
    * @brief Callback signature used by task nodes.
    *
    * The callback receives frame-local execution context and returns either
    * success or a concrete engine error.
    */
   using TaskCallback = std::function<std::expected<void, vve::Error>(const TaskExecutionContext &)>;

   /**
    * @brief Declarative task-node description.
    *
    * Dependencies are expressed explicitly through `depends_on`. `accesses`
    * records coarse resource hazards used by higher-level graph tooling.
    */
   struct TaskNodeDesc {
      TaskNodeHandle handle{};                 ///< Stable task handle.
      TaskKernelId kernel{TaskKernelId::none}; ///< Built-in kernel classification.
      TaskPhase phase{TaskPhase::automatic};   ///< Declared execution phase.
      TaskScope scope{TaskScope::global};      ///< Global or per-window task scope.
      std::optional<WindowHandle> window{};    ///< Optional associated window for per-window tasks.
      Vector<TaskNodeHandle> depends_on{};     ///< Explicit predecessor task handles.
      Vector<ResourceAccess> accesses{};       ///< Coarse resource access declarations.
      std::string debug_name{};                ///< Human-readable task name for diagnostics.
      TaskCallback callback{};                 ///< Optional callback executed when the task runs.
   };

   /// @brief Immutable task graph assembled from task-node descriptions.
   struct TaskGraph {
      Vector<TaskNodeDesc> nodes{}; ///< Immutable task-node list in builder insertion order.
   };

   /**
    * @brief Helper for constructing task graphs incrementally.
    *
    * Stable names are hashed into task handles so independently built subsystems
    * can refer to the same task identity without exchanging raw indices.
    */
   class TaskGraphBuilder {
   public:
      [[nodiscard]] TaskNodeHandle addTask(std::string_view stable_name, TaskKernelId kernel,
                                           TaskCallback callback = {}, Vector<TaskNodeHandle> depends_on = {},
                                           Vector<ResourceAccess> accesses = {}, std::string debug_name = {},
                                           TaskPhase phase = TaskPhase::automatic,
                                           TaskScope scope = TaskScope::global,
                                           std::optional<WindowHandle> window = std::nullopt);

      void addTask(TaskNodeDesc node);
      [[nodiscard]] bool setTaskCallback(TaskNodeHandle handle, TaskCallback callback);
      [[nodiscard]] static TaskNodeHandle makeTaskHandle(std::string_view stable_name);
      [[nodiscard]] static TaskNodeHandle taskHandleFor(std::string_view stable_name);
      [[nodiscard]] std::optional<TaskNodeHandle> findTask(std::string_view stable_name) const;
      [[nodiscard]] bool containsTask(std::string_view stable_name) const;
      [[nodiscard]] bool addDependency(TaskNodeHandle task, TaskNodeHandle dependency);
      [[nodiscard]] bool addDependency(std::string_view task_name, std::string_view dependency_name);
      [[nodiscard]] static TaskPhase inferPhase(TaskKernelId kernel);

      [[nodiscard]] TaskGraph build() &&; 
      [[nodiscard]] std::vector<TaskNodeHandle> rootTasks() const;
      [[nodiscard]] std::vector<TaskNodeHandle> leafTasks() const;

   private:
      Vector<TaskNodeDesc> nodes_{}; ///< Mutable task-node list accumulated before final graph build.
   };

   /**
    * @brief Adds a task using the convenience parameter set.
    * @param stable_name Stable name hashed into the task handle.
    * @param kernel Built-in kernel classification.
    * @param callback Optional callback invoked during execution.
    * @param depends_on Explicit predecessor task handles.
    * @param accesses Coarse resource accesses associated with the task.
    * @param debug_name Human-readable task name for diagnostics.
    * @param phase Optional explicit phase override.
    * @param scope Task scope used by scheduling and diagnostics.
    * @param window Optional window scope for per-window tasks.
    * @return Handle of the added task.
    */
   inline TaskNodeHandle TaskGraphBuilder::addTask(std::string_view stable_name, TaskKernelId kernel,
                                                   TaskCallback callback, Vector<TaskNodeHandle> depends_on,
                                                   Vector<ResourceAccess> accesses, std::string debug_name, TaskPhase phase,
                                                   TaskScope scope, std::optional<WindowHandle> window) {
      // Stable names give independently constructed tasks a deterministic identity.
      const TaskNodeHandle handle = makeTaskHandle(stable_name);
      // Normalize convenience parameters into the full task-node representation.
      addTask(TaskNodeDesc{.handle = handle,
                           .kernel = kernel,
                           .phase = phase == TaskPhase::automatic ? inferPhase(kernel) : phase,
                           .scope = scope,
                           .window = window,
                           .depends_on = std::move(depends_on),
                           .accesses = std::move(accesses),
                           .debug_name = debug_name.empty() ? std::string(stable_name) : std::move(debug_name),
                           .callback = std::move(callback)});
      return handle;
   }

   /**
    * @brief Adds a fully described task node to the builder.
    * @param node Task-node description to append.
    */
   inline void TaskGraphBuilder::addTask(TaskNodeDesc node) {
      // Synthesize a debug label when the caller did not provide one so tools
      // still have something readable to display.
      if (node.debug_name.empty()) {
         node.debug_name = "task." + std::to_string(node.handle.value.value());
      }
      // Automatic phase selection keeps callers from restating obvious kernel defaults.
      if (node.phase == TaskPhase::automatic) {
         node.phase = inferPhase(node.kernel);
      }

      // Preserve insertion order because later compilation derives execution
      // structure from the accumulated node set.
      nodes_.push_back(std::move(node));
   }

   /**
    * @brief Replaces the callback of a previously registered task.
    * @param handle Task handle to update.
    * @param callback New callback to store.
    * @return `true` when the task was found, otherwise `false`.
    */
   inline bool TaskGraphBuilder::setTaskCallback(TaskNodeHandle handle, TaskCallback callback) {
      for (auto &node : nodes_) {
         // Task identity is based on the strong handle rather than the debug name.
         if (node.handle.value == handle.value) {
            node.callback = std::move(callback);
            return true;
         }
      }

      return false;
   }

   /**
    * @brief Hashes a stable task name into a deterministic handle.
    * @param stable_name Stable task name.
    * @return Deterministic task handle derived from the name.
    */
   inline TaskNodeHandle TaskGraphBuilder::makeTaskHandle(std::string_view stable_name) {
      // Hash-based handles let multiple subsystems refer to the same conceptual
      // task without coordinating integer indices.
      return TaskNodeHandle{vve::Handle::fromHash(stable_name)};
   }

   /**
    * @brief Convenience alias for `makeTaskHandle`.
    * @param stable_name Stable task name.
    * @return Deterministic task handle derived from the name.
    */
   inline TaskNodeHandle TaskGraphBuilder::taskHandleFor(std::string_view stable_name) {
      return makeTaskHandle(stable_name);
   }

   /**
    * @brief Searches for a task by stable name.
    * @param stable_name Stable task name to search for.
    * @return Matching task handle when present.
    */
   inline std::optional<TaskNodeHandle> TaskGraphBuilder::findTask(std::string_view stable_name) const {
      const auto handle = makeTaskHandle(stable_name);
      for (const auto &node : nodes_) {
         // Match on the deterministic handle so lookup remains stable even if
         // debug labels differ.
         if (node.handle.value == handle.value) {
            return handle;
         }
      }

      return std::nullopt;
   }

   /**
    * @brief Returns whether a task with the given stable name has been added.
    * @param stable_name Stable task name to search for.
    * @return `true` when the task exists, otherwise `false`.
    */
   inline bool TaskGraphBuilder::containsTask(std::string_view stable_name) const {
      return findTask(stable_name).has_value();
   }

   /**
    * @brief Adds a dependency edge from `task` to `dependency`.
    * @param task Task that depends on `dependency`.
    * @param dependency Task that must execute first.
    * @return `true` when the dependency was recorded, otherwise `false`.
    */
   inline bool TaskGraphBuilder::addDependency(TaskNodeHandle task, TaskNodeHandle dependency) {
      for (auto &node : nodes_) {
         // Only the dependent task is mutated; the prerequisite task remains unchanged.
         if (node.handle.value == task.value) {
            for (const auto &existing_dependency : node.depends_on) {
               // Treat duplicate dependency declarations as success to keep the
               // builder tolerant of additive subsystem registration.
               if (existing_dependency.value == dependency.value) {
                  return true;
               }
            }

            node.depends_on.push_back(dependency); // Store an explicit predecessor edge for later graph compilation.
            return true;
         }
      }

      return false;
   }

   /**
    * @brief Adds a dependency edge using stable task names.
    * @param task_name Stable name of the dependent task.
    * @param dependency_name Stable name of the prerequisite task.
    * @return `true` when the dependency was recorded, otherwise `false`.
    */
   inline bool TaskGraphBuilder::addDependency(std::string_view task_name, std::string_view dependency_name) {
      return addDependency(makeTaskHandle(task_name), makeTaskHandle(dependency_name));
   }

   /**
    * @brief Infers the default phase for a task kernel.
    * @param kernel Built-in task kernel classification.
    * @return Default task phase associated with the kernel.
    */
   inline TaskPhase TaskGraphBuilder::inferPhase(TaskKernelId kernel) {
      // Kernel-to-phase mapping gives built-in tasks a sensible default slot in
      // the frame without forcing every caller to specify one manually.
      switch (kernel) {
      case TaskKernelId::begin_frame:
         return TaskPhase::begin_frame;
      case TaskKernelId::poll_window_events:
         return TaskPhase::input;
      case TaskKernelId::update_transforms:
      case TaskKernelId::sample_animations:
      case TaskKernelId::cull_visibility_cpu:
         return TaskPhase::scene;
      case TaskKernelId::upload_resources:
         return TaskPhase::resources;
      case TaskKernelId::cull_visibility_gpu:
      case TaskKernelId::build_draw_packets:
      case TaskKernelId::record_render_graph:
      case TaskKernelId::consume_frame_output:
         return TaskPhase::render;
      case TaskKernelId::end_frame:
         return TaskPhase::end_frame;
      case TaskKernelId::none:
      default:
         return TaskPhase::user_update;
      }
   }

   /**
    * @brief Finalizes the builder into an immutable task graph.
    * @return Task graph built from the accumulated nodes.
    */
   inline TaskGraph TaskGraphBuilder::build() && { return TaskGraph{.nodes = std::move(nodes_)}; }

   /**
    * @brief Returns tasks that have no explicit predecessors.
    * @return Handles of all root tasks.
    */
   inline std::vector<TaskNodeHandle> TaskGraphBuilder::rootTasks() const {
      std::vector<TaskNodeHandle> root_handles{};
      root_handles.reserve(nodes_.size());
      for (const auto &node : nodes_) {
         if (node.depends_on.empty()) { // Root tasks are exactly the tasks with no declared predecessors.
            root_handles.push_back(node.handle);
         }
      }

      return root_handles;
   }

   /**
    * @brief Returns tasks that are not referenced as dependencies by other tasks.
    * @return Handles of all leaf tasks.
    */
   inline std::vector<TaskNodeHandle> TaskGraphBuilder::leafTasks() const {
      std::unordered_set<vve::Handle::value_type> dependency_handles{};
      dependency_handles.reserve(nodes_.size());

      for (const auto &node : nodes_) {
         for (const auto &dependency : node.depends_on) {
            // Track every task that appears as someone else's prerequisite.
            dependency_handles.insert(dependency.value.value());
         }
      }

      std::vector<TaskNodeHandle> leaf_handles{};
      leaf_handles.reserve(nodes_.size());
      for (const auto &node : nodes_) {
         // Leaf tasks are the ones never referenced as a predecessor.
         if (!dependency_handles.contains(node.handle.value.value())) {
            leaf_handles.push_back(node.handle);
         }
      }

      return leaf_handles;
   }

   /// @brief Reflected shader parameter binding description.
   struct ShaderParameter {
      std::string name{};       ///< Parameter name as reflected from the shader.
      std::string type_name{};  ///< Shader language type name.
      std::uint32_t binding{0}; ///< Resource binding index.
      std::uint32_t set{0};     ///< Descriptor set index.
   };

   /// @brief Reflected shader metadata used by higher-level systems.
   struct ShaderMetadata {
      ShaderHandle handle{};                ///< Stable shader handle.
      std::string shader_name{};            ///< Human-readable shader name.
      Vector<ShaderStage> stages{};         ///< Stages compiled into the shader.
      Vector<ShaderParameter> parameters{}; ///< Reflected resource parameters.
      std::string intended_renderer{};      ///< Intended renderer mode as metadata text.
      std::string intended_shadow{};        ///< Intended shadow mode as metadata text.
   };

   /// @brief Coarse render-graph resource access declaration.
   struct RenderResourceUse {
      vve::Handle resource{}; ///< Resource touched by the render pass.
      bool write{false};      ///< Whether the render pass writes to the resource.
   };

   /// @brief Declarative render-pass description.
   struct RenderPassDesc {
      RenderPassHandle handle{};                   ///< Stable render-pass handle.
      RenderKernelId kernel{RenderKernelId::none}; ///< Built-in render kernel classification.
      Vector<RenderPassHandle> depends_on{};       ///< Explicit predecessor render passes.
      Vector<RenderResourceUse> uses{};            ///< Coarse render-resource accesses.
      std::string debug_name{};                    ///< Human-readable pass name for diagnostics.
   };

   /// @brief Immutable render graph used by the render system.
   struct RenderGraph {
      Vector<RenderPassDesc> passes{}; ///< Immutable list of render passes in the graph.
   };

   /// @brief Render graph bundle bound to a specific window.
   struct WindowRenderPipeline {
      WindowHandle window{};   ///< Window owning the render pipeline.
      std::string window_id{}; ///< Stable window string id.
      RenderGraph graph{};     ///< Render graph executed for the window.
   };

   /// @brief Human-readable snapshot of the assembled engine runtime configuration.
   struct EngineRuntimeSnapshot {
      vve::GraphicsApi graphics_api{vve::GraphicsApi::vulkan};         ///< Active graphics API.
      vve::RendererKind renderer{vve::RendererKind::forward_renderer}; ///< Active renderer kind.
      vve::ShadowKind shadow{vve::ShadowKind::none};                   ///< Active shadow mode.
      bool imgui_enabled{true};                                        ///< Whether Dear ImGui is enabled.
      std::string asset_system{"AssimpAssetSystem"};                   ///< Asset-system implementation name.
      std::string resource_system{"ResourceSystem"};                   ///< Resource-system implementation name.
      std::string scene_system{"SceneSystem"};                         ///< Scene-system implementation name.
      std::string task_graph_system{"TaskGraphSystem"};                ///< Task-graph-system implementation name.
      std::string shader_system{"SlangShaderSystem"};                  ///< Shader-system implementation name.
      std::string render_system{"RenderSystem"};                       ///< Render-system implementation name.
      std::string window_system{"SDL3WindowSystem"};                   ///< Window-system implementation name.
      std::string gui_system{"ImGuiSystem"};                           ///< GUI-system implementation name.
      Vector<std::string> task_systems{};                              ///< Names of user-supplied task systems.
   };

} // namespace vve::v3
