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

export namespace vve::v3 {

   class AssimpAssetSystemImplementation;
   class DefaultResourceSystemImplementation;
   class DefaultSceneSystemImplementation;
   class DefaultTaskGraphSystemImplementation;
   class SDL3WindowSystemImplementation;
   class VulkanGraphicsBackendImplementation;
   class SlangShaderSystemImplementation;
   class DefaultRenderSystemImplementation;
   class ImGuiSystemImplementation;

   template <typename TImplementation> class VVE_API AssetSystemFacade {
   public:
      AssetSystemFacade();
      ~AssetSystemFacade() = default;
      AssetSystemFacade(AssetSystemFacade &&other) noexcept = default;
      AssetSystemFacade &operator=(AssetSystemFacade &&other) noexcept = default;
      AssetSystemFacade(const AssetSystemFacade &) = delete;
      AssetSystemFacade &operator=(const AssetSystemFacade &) = delete;

      [[nodiscard]] std::string_view name() const noexcept;
      [[nodiscard]] std::expected<ImportedScene, vve::Error> importScene(const std::filesystem::path &source_path);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr};
   };

   template <typename TImplementation> class VVE_API ResourceSystemFacade {
   public:
      ResourceSystemFacade();
      ~ResourceSystemFacade() = default;
      ResourceSystemFacade(ResourceSystemFacade &&other) noexcept = default;
      ResourceSystemFacade &operator=(ResourceSystemFacade &&other) noexcept = default;
      ResourceSystemFacade(const ResourceSystemFacade &) = delete;
      ResourceSystemFacade &operator=(const ResourceSystemFacade &) = delete;

      [[nodiscard]] std::string_view name() const noexcept;
      [[nodiscard]] std::expected<void, vve::Error> registerImportedScene(const ImportedScene &scene,
                                                                          const std::filesystem::path &source_path);
      [[nodiscard]] std::expected<std::vector<ResourceRecord>, vve::Error> enumerate() const;
      [[nodiscard]] std::expected<void, vve::Error> uploadResources(const FrameContext &frame_context,
                                                                    const SceneData &scene);
      void registerTasks(TaskGraphBuilder &builder, const SceneData &scene);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr};
   };

   template <typename TImplementation> class VVE_API SceneSystemFacade {
   public:
      SceneSystemFacade();
      ~SceneSystemFacade() = default;
      SceneSystemFacade(SceneSystemFacade &&other) noexcept = default;
      SceneSystemFacade &operator=(SceneSystemFacade &&other) noexcept = default;
      SceneSystemFacade(const SceneSystemFacade &) = delete;
      SceneSystemFacade &operator=(const SceneSystemFacade &) = delete;

      [[nodiscard]] std::string_view name() const noexcept;
      [[nodiscard]] std::expected<SceneData, vve::Error> instantiate(const ImportedScene &scene);
      [[nodiscard]] std::expected<void, vve::Error> updateTransforms(const FrameContext &frame_context,
                                                                     SceneData &scene);
      [[nodiscard]] std::expected<void, vve::Error> cullVisibility(const FrameContext &frame_context,
                                                                   const SceneData &scene);
      void registerTasks(TaskGraphBuilder &builder, const SceneData &scene);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr};
   };

   template <typename TImplementation> class VVE_API WindowSystemFacade {
   public:
      WindowSystemFacade();
      ~WindowSystemFacade() = default;
      WindowSystemFacade(WindowSystemFacade &&other) noexcept = default;
      WindowSystemFacade &operator=(WindowSystemFacade &&other) noexcept = default;
      WindowSystemFacade(const WindowSystemFacade &) = delete;
      WindowSystemFacade &operator=(const WindowSystemFacade &) = delete;

      [[nodiscard]] std::string_view name() const noexcept;
      [[nodiscard]] std::expected<void, vve::Error>
      init(VectorConstRange<vve::WindowDesc> windows);
      [[nodiscard]] std::expected<void, vve::Error> pollEvents(const FrameContext &frame_context);
      [[nodiscard]] WindowFrameData frameData() const;
      [[nodiscard]] VectorConstRange<WindowState> windows() const;
      void setFrameDataSink(std::shared_ptr<WindowFrameData> frame_data);
      void registerTasks(TaskGraphBuilder &builder);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr};
   };

   template <typename TImplementation> class VVE_API GraphicsBackendFacade {
   public:
      GraphicsBackendFacade();
      ~GraphicsBackendFacade() = default;
      GraphicsBackendFacade(GraphicsBackendFacade &&other) noexcept = default;
      GraphicsBackendFacade &operator=(GraphicsBackendFacade &&other) noexcept = default;
      GraphicsBackendFacade(const GraphicsBackendFacade &) = delete;
      GraphicsBackendFacade &operator=(const GraphicsBackendFacade &) = delete;

      [[nodiscard]] std::string_view name() const noexcept;
      [[nodiscard]] vve::GraphicsApi api() const noexcept;
      [[nodiscard]] std::expected<void, vve::Error> init();
      [[nodiscard]] std::expected<void, vve::Error> beginFrame(const FrameContext &frame_context);
      [[nodiscard]] std::expected<void, vve::Error> endFrame(const FrameContext &frame_context);
      void registerTasks(TaskGraphBuilder &builder);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr};
   };

   template <typename TImplementation> class VVE_API ShaderSystemFacade {
   public:
      ShaderSystemFacade();
      ~ShaderSystemFacade() = default;
      ShaderSystemFacade(ShaderSystemFacade &&other) noexcept = default;
      ShaderSystemFacade &operator=(ShaderSystemFacade &&other) noexcept = default;
      ShaderSystemFacade(const ShaderSystemFacade &) = delete;
      ShaderSystemFacade &operator=(const ShaderSystemFacade &) = delete;

      [[nodiscard]] std::string_view name() const noexcept;
      [[nodiscard]] std::expected<ShaderMetadata, vve::Error>
      reflect(const std::filesystem::path &shader_path, vve::RendererKind renderer, vve::ShadowKind shadow);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr};
   };

   template <typename TImplementation> class VVE_API RenderSystemFacade {
   public:
      RenderSystemFacade(vve::RendererKind renderer, vve::ShadowKind shadow,
                         GraphicsBackendFacade<VulkanGraphicsBackendImplementation> &graphics_backend,
                         bool imgui_enabled);
      ~RenderSystemFacade() = default;
      RenderSystemFacade(RenderSystemFacade &&other) noexcept = default;
      RenderSystemFacade &operator=(RenderSystemFacade &&other) noexcept = default;
      RenderSystemFacade(const RenderSystemFacade &) = delete;
      RenderSystemFacade &operator=(const RenderSystemFacade &) = delete;

      [[nodiscard]] std::string_view name() const noexcept;
      [[nodiscard]] RenderGraph buildStaticGraph(WindowHandle window);
      [[nodiscard]] std::expected<void, vve::Error> cullVisibilityGpu(const FrameContext &frame_context,
                                                                      const SceneData &scene, WindowHandle window,
                                                                      const RenderGraph &render_graph);
      [[nodiscard]] std::expected<void, vve::Error> buildDrawPackets(const FrameContext &frame_context,
                                                                     const SceneData &scene, WindowHandle window,
                                                                     const RenderGraph &render_graph);
      [[nodiscard]] std::expected<void, vve::Error> record(const FrameContext &frame_context, const SceneData &scene,
                                                           WindowHandle window, const RenderGraph &render_graph);
      [[nodiscard]] std::expected<void, vve::Error> consumeOutput(const FrameContext &frame_context,
                                                                  const SceneData &scene, WindowHandle window,
                                                                  const RenderGraph &render_graph);
      void registerTasks(TaskGraphBuilder &builder, const SceneData &scene,
                         VectorConstRange<WindowRenderPipeline> render_pipelines);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr};
   };

   template <typename TImplementation> class VVE_API GuiSystemFacade {
   public:
      GuiSystemFacade();
      ~GuiSystemFacade() = default;
      GuiSystemFacade(GuiSystemFacade &&other) noexcept = default;
      GuiSystemFacade &operator=(GuiSystemFacade &&other) noexcept = default;
      GuiSystemFacade(const GuiSystemFacade &) = delete;
      GuiSystemFacade &operator=(const GuiSystemFacade &) = delete;

      [[nodiscard]] std::string_view name() const noexcept;
      [[nodiscard]] std::expected<void, vve::Error>
      init(GraphicsBackendFacade<VulkanGraphicsBackendImplementation> &graphics_backend);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr};
   };

   using AssetSystem = AssetSystemFacade<AssimpAssetSystemImplementation>;
   using ResourceSystem = ResourceSystemFacade<DefaultResourceSystemImplementation>;
   using SceneSystem = SceneSystemFacade<DefaultSceneSystemImplementation>;
   using WindowSystem = WindowSystemFacade<SDL3WindowSystemImplementation>;
   using GraphicsBackend = GraphicsBackendFacade<VulkanGraphicsBackendImplementation>;
   using ShaderSystem = ShaderSystemFacade<SlangShaderSystemImplementation>;
   using RenderSystem = RenderSystemFacade<DefaultRenderSystemImplementation>;
   using GuiSystem = GuiSystemFacade<ImGuiSystemImplementation>;

   class ITaskSystem {
   public:
      virtual ~ITaskSystem() = default;
      [[nodiscard]] virtual std::string_view name() const noexcept = 0;
      virtual void registerTasks(TaskGraphBuilder &builder, const SceneData &scene) = 0;
   };

   template <typename TImplementation> class VVE_API TaskGraphSystemFacade {
   public:
      TaskGraphSystemFacade();
      ~TaskGraphSystemFacade() = default;
      TaskGraphSystemFacade(TaskGraphSystemFacade &&other) noexcept = default;
      TaskGraphSystemFacade &operator=(TaskGraphSystemFacade &&other) noexcept = default;
      TaskGraphSystemFacade(const TaskGraphSystemFacade &) = delete;
      TaskGraphSystemFacade &operator=(const TaskGraphSystemFacade &) = delete;

      [[nodiscard]] std::string_view name() const noexcept;
      [[nodiscard]] TaskGraph build(const SceneData &scene, VectorConstRange<ITaskSystem *> task_systems,
                                    WindowSystem &window_system, GraphicsBackend &graphics_backend,
                                    ResourceSystem &resource_system, SceneSystem &scene_system,
                                    RenderSystem &render_system, std::function<void(TaskGraphBuilder &, const SceneData &)> extra_tasks,
                                    VectorConstRange<WindowRenderPipeline> render_pipelines);

   private:
      std::unique_ptr<TImplementation, void (*)(TImplementation *)> implementation_{nullptr, nullptr};
   };

   using TaskGraphSystem = TaskGraphSystemFacade<DefaultTaskGraphSystemImplementation>;

   struct EngineRuntimeDesc {
      vve::GraphicsApi graphics_api{vve::GraphicsApi::vulkan};
      vve::RendererKind renderer{vve::RendererKind::forward_renderer};
      vve::ShadowKind shadow{vve::ShadowKind::none};
      bool imgui_enabled{true};
      Vector<vve::WindowDesc> windows{vve::WindowDesc{}};
      Vector<std::shared_ptr<ITaskSystem>> task_systems{};
   };

   struct TaskSystems {
      Vector<std::shared_ptr<ITaskSystem>> value{};
   };

} // namespace vve::v3
