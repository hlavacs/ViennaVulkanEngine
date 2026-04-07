module;

#if defined(_WIN32)
#if defined(VVE_ENGINE_BUILD)
#define VVE_API __declspec(dllexport)
#else
#define VVE_API __declspec(dllimport)
#endif
#else
#define VVE_API
#endif

export module VEEngine.V3.Systems;
import VEEngine.V3.Types;
import VEEngine;
import std;

/**
 * @file
 * @brief Public v3 subsystem facades.
 *
 * Each facade defines the stable API contract exposed at a subsystem boundary
 * while the concrete implementation remains version-local.
 */
export namespace vve::v3 {

   class AssimpAssetSystemImplementation;
   class DefaultResourceSystemImplementation;
   class DefaultSceneSystemImplementation;
   class DefaultSceneLoaderImplementation;
   class DefaultTaskGraphSystemImplementation;
   class SDL3WindowSystemImplementation;
   class VulkanGraphicsBackendImplementation;
   class SlangShaderSystemImplementation;
   class DefaultRenderSystemImplementation;
   class ImGuiSystemImplementation;

   /// @brief Asset-import facade for source scene ingestion.
   template <typename TImplementation> class VVE_API AssetSystemFacade {
   public:
      AssetSystemFacade();
      ~AssetSystemFacade() = default;
      AssetSystemFacade(AssetSystemFacade &&other) noexcept = default;
      AssetSystemFacade &operator=(AssetSystemFacade &&other) noexcept = default;
      AssetSystemFacade(const AssetSystemFacade &) = delete;
      AssetSystemFacade &operator=(const AssetSystemFacade &) = delete;

      /// @brief Returns the implementation name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept; 
      /// @brief Imports a source scene and returns engine-neutral imported data.
      [[nodiscard]] std::expected<ImportedScene, vve::Error> importScene(const std::filesystem::path &source_path);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr}; ///< Owned subsystem implementation hidden behind the facade boundary.
   };

   /// @brief Resource-registration and upload facade.
   template <typename TImplementation> class VVE_API ResourceSystemFacade {
   public:
      ResourceSystemFacade();
      ~ResourceSystemFacade() = default;
      ResourceSystemFacade(ResourceSystemFacade &&other) noexcept = default;
      ResourceSystemFacade &operator=(ResourceSystemFacade &&other) noexcept = default;
      ResourceSystemFacade(const ResourceSystemFacade &) = delete;
      ResourceSystemFacade &operator=(const ResourceSystemFacade &) = delete;

      /// @brief Returns the implementation name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept; 
      /// @brief Registers resources referenced by an imported scene.
      [[nodiscard]] std::expected<void, vve::Error> registerImportedScene(const ImportedScene &scene,
                                                                          const std::filesystem::path &source_path);
      /// @brief Enumerates the currently known resource records.
      [[nodiscard]] std::expected<std::vector<ResourceRecord>, vve::Error> enumerate() const;
      /// @brief Uploads resources needed for the current frame.
      [[nodiscard]] std::expected<void, vve::Error> uploadResources(const FrameContext &frame_context,
                                                                    const SceneData &scene);
      /// @brief Registers resource-related tasks with the frame task graph.
      void registerTasks(TaskGraphBuilder &builder, const SceneData &scene);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr}; ///< Owned subsystem implementation hidden behind the facade boundary.
   };

   /// @brief Scene-instantiation and scene-update facade.
   template <typename TImplementation> class VVE_API SceneSystemFacade {
   public:
      SceneSystemFacade();
      ~SceneSystemFacade() = default;
      SceneSystemFacade(SceneSystemFacade &&other) noexcept = default;
      SceneSystemFacade &operator=(SceneSystemFacade &&other) noexcept = default;
      SceneSystemFacade(const SceneSystemFacade &) = delete;
      SceneSystemFacade &operator=(const SceneSystemFacade &) = delete;

      /// @brief Returns the implementation name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept; 
      /// @brief Instantiates runtime scene data from imported scene data.
      [[nodiscard]] std::expected<SceneData, vve::Error> instantiate(const ImportedScene &scene);
      /// @brief Updates runtime transforms for the current frame.
      [[nodiscard]] std::expected<void, vve::Error> updateTransforms(const FrameContext &frame_context,
                                                                     SceneData &scene);
      /// @brief Performs visibility determination for the current frame.
      [[nodiscard]] std::expected<void, vve::Error> cullVisibility(const FrameContext &frame_context,
                                                                   const SceneData &scene);
      /// @brief Registers scene-related tasks with the frame task graph.
      void registerTasks(TaskGraphBuilder &builder, const SceneData &scene);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr}; ///< Owned subsystem implementation hidden behind the facade boundary.
   };

   /// @brief Scene-loading orchestration facade.
   template <typename TImplementation> class VVE_API SceneLoaderFacade {
   public:
      /// @brief Creates the scene loader around the participating import, resource, and scene subsystems.
      SceneLoaderFacade(AssetSystemFacade<AssimpAssetSystemImplementation> &asset_system,
                        ResourceSystemFacade<DefaultResourceSystemImplementation> &resource_system,
                        SceneSystemFacade<DefaultSceneSystemImplementation> &scene_system);
      ~SceneLoaderFacade() = default;
      SceneLoaderFacade(SceneLoaderFacade &&other) noexcept = default;
      SceneLoaderFacade &operator=(SceneLoaderFacade &&other) noexcept = default;
      SceneLoaderFacade(const SceneLoaderFacade &) = delete;
      SceneLoaderFacade &operator=(const SceneLoaderFacade &) = delete;

      /// @brief Returns the implementation name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept;
      /// @brief Imports, registers, and instantiates a scene file into runtime scene data.
      [[nodiscard]] std::expected<SceneData, vve::Error> loadScene(const std::filesystem::path &file_path);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr}; ///< Owned subsystem implementation hidden behind the facade boundary.
   };

   /// @brief Window creation and event-polling facade.
   template <typename TImplementation> class VVE_API WindowSystemFacade {
   public:
      WindowSystemFacade();
      ~WindowSystemFacade() = default;
      WindowSystemFacade(WindowSystemFacade &&other) noexcept = default;
      WindowSystemFacade &operator=(WindowSystemFacade &&other) noexcept = default;
      WindowSystemFacade(const WindowSystemFacade &) = delete;
      WindowSystemFacade &operator=(const WindowSystemFacade &) = delete;

      /// @brief Returns the implementation name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept; 
      /// @brief Creates the configured runtime windows.
      [[nodiscard]] std::expected<void, vve::Error>
      init(VectorConstRange<vve::WindowDesc> windows);
      /// @brief Polls platform events for the current frame.
      [[nodiscard]] std::expected<void, vve::Error> pollEvents(const FrameContext &frame_context);
      /// @brief Returns the current frame's window and event snapshot.
      [[nodiscard]] WindowFrameData frameData() const;
      /// @brief Returns the current runtime window range.
      [[nodiscard]] VectorConstRange<WindowState> windows() const;
      /// @brief Installs the destination frame-data snapshot shared with other systems.
      void setFrameDataSink(std::shared_ptr<WindowFrameData> frame_data);
      /// @brief Registers window-related tasks with the frame task graph.
      void registerTasks(TaskGraphBuilder &builder);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr}; ///< Owned subsystem implementation hidden behind the facade boundary.
   };

   /// @brief Graphics-backend facade for frame boundary work.
   template <typename TImplementation> class VVE_API GraphicsBackendFacade {
   public:
      GraphicsBackendFacade();
      ~GraphicsBackendFacade() = default;
      GraphicsBackendFacade(GraphicsBackendFacade &&other) noexcept = default;
      GraphicsBackendFacade &operator=(GraphicsBackendFacade &&other) noexcept = default;
      GraphicsBackendFacade(const GraphicsBackendFacade &) = delete;
      GraphicsBackendFacade &operator=(const GraphicsBackendFacade &) = delete;

      /// @brief Returns the implementation name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept; 
      /// @brief Returns the graphics API implemented by this backend.
      [[nodiscard]] vve::GraphicsApi api() const noexcept; 
      /// @brief Initializes backend resources.
      [[nodiscard]] std::expected<void, vve::Error> init();
      /// @brief Performs backend begin-frame work.
      [[nodiscard]] std::expected<void, vve::Error> beginFrame(const FrameContext &frame_context);
      /// @brief Performs backend end-frame work.
      [[nodiscard]] std::expected<void, vve::Error> endFrame(const FrameContext &frame_context);
      /// @brief Registers backend-owned frame tasks.
      void registerTasks(TaskGraphBuilder &builder);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr}; ///< Owned subsystem implementation hidden behind the facade boundary.
   };

   /// @brief Shader reflection facade.
   template <typename TImplementation> class VVE_API ShaderSystemFacade {
   public:
      ShaderSystemFacade();
      ~ShaderSystemFacade() = default;
      ShaderSystemFacade(ShaderSystemFacade &&other) noexcept = default;
      ShaderSystemFacade &operator=(ShaderSystemFacade &&other) noexcept = default;
      ShaderSystemFacade(const ShaderSystemFacade &) = delete;
      ShaderSystemFacade &operator=(const ShaderSystemFacade &) = delete;

      /// @brief Returns the implementation name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept; 
      /// @brief Reflects a shader into engine-visible metadata for the chosen renderer mode.
      [[nodiscard]] std::expected<ShaderMetadata, vve::Error>
      reflect(const std::filesystem::path &shader_path, vve::RendererKind renderer, vve::ShadowKind shadow);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr}; ///< Owned subsystem implementation hidden behind the facade boundary.
   };

   /// @brief Render orchestration facade.
   template <typename TImplementation> class VVE_API RenderSystemFacade {
   public:
      /// @brief Creates the render system for the selected renderer and shadow mode.
      RenderSystemFacade(vve::RendererKind renderer, vve::ShadowKind shadow,
                         GraphicsBackendFacade<VulkanGraphicsBackendImplementation> &graphics_backend,
                         bool imgui_enabled);
      ~RenderSystemFacade() = default;
      RenderSystemFacade(RenderSystemFacade &&other) noexcept = default;
      RenderSystemFacade &operator=(RenderSystemFacade &&other) noexcept = default;
      RenderSystemFacade(const RenderSystemFacade &) = delete;
      RenderSystemFacade &operator=(const RenderSystemFacade &) = delete;

      /// @brief Returns the implementation name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept; 
      /// @brief Builds the static render graph for a window.
      [[nodiscard]] RenderGraph buildStaticGraph(WindowHandle window);
      /// @brief Performs GPU visibility work for a window render graph.
      [[nodiscard]] std::expected<void, vve::Error> cullVisibilityGpu(const FrameContext &frame_context,
                                                                      const SceneData &scene, WindowHandle window,
                                                                      const RenderGraph &render_graph);
      /// @brief Builds draw packets for a window render graph.
      [[nodiscard]] std::expected<void, vve::Error> buildDrawPackets(const FrameContext &frame_context,
                                                                     const SceneData &scene, WindowHandle window,
                                                                     const RenderGraph &render_graph);
      /// @brief Records render commands for a window render graph.
      [[nodiscard]] std::expected<void, vve::Error> record(const FrameContext &frame_context, const SceneData &scene,
                                                           WindowHandle window, const RenderGraph &render_graph);
      /// @brief Presents or otherwise consumes the produced frame output.
      [[nodiscard]] std::expected<void, vve::Error> consumeOutput(const FrameContext &frame_context,
                                                                  const SceneData &scene, WindowHandle window,
                                                                  const RenderGraph &render_graph);
      /// @brief Registers render tasks for all active window pipelines.
      void registerTasks(TaskGraphBuilder &builder, const SceneData &scene,
                         VectorConstRange<WindowRenderPipeline> render_pipelines);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr}; ///< Owned subsystem implementation hidden behind the facade boundary.
   };

   /// @brief Optional GUI integration facade.
   template <typename TImplementation> class VVE_API GuiSystemFacade {
   public:
      GuiSystemFacade();
      ~GuiSystemFacade() = default;
      GuiSystemFacade(GuiSystemFacade &&other) noexcept = default;
      GuiSystemFacade &operator=(GuiSystemFacade &&other) noexcept = default;
      GuiSystemFacade(const GuiSystemFacade &) = delete;
      GuiSystemFacade &operator=(const GuiSystemFacade &) = delete;

      /// @brief Returns the implementation name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept; 
      /// @brief Initializes GUI resources against the active graphics backend.
      [[nodiscard]] std::expected<void, vve::Error>
      init(GraphicsBackendFacade<VulkanGraphicsBackendImplementation> &graphics_backend);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr}; ///< Owned subsystem implementation hidden behind the facade boundary.
   };

   /// @brief Default asset-system facade alias for v3.
   using AssetSystem = AssetSystemFacade<AssimpAssetSystemImplementation>;
   /// @brief Default resource-system facade alias for v3.
   using ResourceSystem = ResourceSystemFacade<DefaultResourceSystemImplementation>;
   /// @brief Default scene-system facade alias for v3.
   using SceneSystem = SceneSystemFacade<DefaultSceneSystemImplementation>;
   /// @brief Default scene-loader facade alias for v3.
   using SceneLoader = SceneLoaderFacade<DefaultSceneLoaderImplementation>;
   /// @brief Default window-system facade alias for v3.
   using WindowSystem = WindowSystemFacade<SDL3WindowSystemImplementation>;
   /// @brief Default graphics-backend facade alias for v3.
   using GraphicsBackend = GraphicsBackendFacade<VulkanGraphicsBackendImplementation>;
   /// @brief Default shader-system facade alias for v3.
   using ShaderSystem = ShaderSystemFacade<SlangShaderSystemImplementation>;
   /// @brief Default render-system facade alias for v3.
   using RenderSystem = RenderSystemFacade<DefaultRenderSystemImplementation>;
   using GuiSystem = GuiSystemFacade<ImGuiSystemImplementation>; ///< Default GUI-system facade alias for v3.

   /**
    * @brief User extension seam for injecting custom tasks into the frame graph.
    *
    * Implementations are expected to describe work declaratively by registering
    * tasks rather than executing frame logic directly.
    */
   class ITaskSystem {
   public:
      virtual ~ITaskSystem() = default;
      /// @brief Returns a diagnostic name for the task-system instance.
      [[nodiscard]] virtual std::string_view name() const noexcept = 0;
      /// @brief Registers this task system's tasks into the shared task graph builder.
      virtual void registerTasks(TaskGraphBuilder &builder, const SceneData &scene) = 0;
   };

   /// @brief Task-graph assembly facade.
   template <typename TImplementation> class VVE_API TaskGraphSystemFacade {
   public:
      TaskGraphSystemFacade();
      ~TaskGraphSystemFacade() = default;
      TaskGraphSystemFacade(TaskGraphSystemFacade &&other) noexcept = default;
      TaskGraphSystemFacade &operator=(TaskGraphSystemFacade &&other) noexcept = default;
      TaskGraphSystemFacade(const TaskGraphSystemFacade &) = delete;
      TaskGraphSystemFacade &operator=(const TaskGraphSystemFacade &) = delete;

      /// @brief Returns the implementation name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept; 
      /**
       * @brief Builds the frame task graph from engine subsystems and external task systems.
       *
       * The builder is populated by each participating subsystem, then finalized
       * into an immutable `TaskGraph`.
       */
      [[nodiscard]] TaskGraph build(const SceneData &scene, VectorConstRange<ITaskSystem *> task_systems,
                                    WindowSystem &window_system, GraphicsBackend &graphics_backend,
                                    ResourceSystem &resource_system, SceneSystem &scene_system,
                                    RenderSystem &render_system, std::function<void(TaskGraphBuilder &, const SceneData &)> extra_tasks,
                                    VectorConstRange<WindowRenderPipeline> render_pipelines);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr}; ///< Owned subsystem implementation hidden behind the facade boundary.
   };

   /// @brief Default task-graph facade alias for v3.
   using TaskGraphSystem = TaskGraphSystemFacade<DefaultTaskGraphSystemImplementation>;

   /// @brief Runtime assembly descriptor used to create the concrete v3 engine runtime.
   struct EngineRuntimeDesc {
      vve::GraphicsApi graphics_api{vve::GraphicsApi::vulkan};         ///< Requested graphics API for runtime creation.
      vve::RendererKind renderer{vve::RendererKind::forward_renderer}; ///< Requested renderer kind.
      vve::ShadowKind shadow{vve::ShadowKind::none};                   ///< Requested shadow mode.
      bool imgui_enabled{true};                                        ///< Enables or disables Dear ImGui integration.
      Vector<vve::WindowDesc> windows{vve::WindowDesc{}};              ///< Windows created during runtime initialization.
      Vector<std::shared_ptr<ITaskSystem>> task_systems{};             ///< External task systems that extend the frame task graph.
   };

   /// @brief Wrapper used to pass runtime task systems through engine configuration.
   struct TaskSystems {
      Vector<std::shared_ptr<ITaskSystem>> value{}; ///< Task-system instances supplied by the caller.
   };

} // namespace vve::v3
