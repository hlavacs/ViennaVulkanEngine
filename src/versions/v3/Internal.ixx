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

export module VEEngine.V3:Internal;
import VEEngine.V3.Types;
import VEEngine.V3.Systems;
import VEEngine;
import std;

export namespace vve::v3::detail {

   struct Runtime final {
      AssetSystem asset_system{};
      ResourceSystem resource_system{};
      SceneSystem scene_system{};
      TaskGraphSystem task_graph_system{};
      std::vector<std::shared_ptr<ITaskSystem>> task_systems{};
      WindowSystem window_system{};
      std::shared_ptr<WindowFrameData> window_frame{
          std::make_shared<WindowFrameData>()};
      GraphicsBackend graphics_backend{};
      ShaderSystem shader_system{};
      std::unique_ptr<RenderSystem> render_system{};
      std::vector<WindowRenderPipeline> render_pipelines{};
      std::unique_ptr<GuiSystem> gui_system{};
      EngineRuntimeSnapshot snapshot{};
   };

   [[nodiscard]] vve::Handle makeStableHandle(std::string_view name,
                                              std::uint64_t salt = 0);
   [[nodiscard]] VVE_API std::expected<Runtime, vve::Error>
   createRuntime(const EngineRuntimeDesc &desc);

} // namespace vve::v3::detail
