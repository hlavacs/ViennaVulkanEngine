module VEEngine.V3;
import std;
import :Internal;

namespace vve::v3::detail {

   vve::Handle makeStableHandle(std::string_view name, std::uint64_t salt) {
      const auto mixed = std::format("{}:{}", name, salt);
      return vve::Handle::fromHash(mixed);
   }

   std::expected<Runtime, vve::Error>
   createRuntime(const EngineRuntimeDesc &desc) {
      if (desc.graphics_api != vve::GraphicsApi::vulkan) {
         return std::unexpected(vve::Error::unsupported_version);
      }

      Runtime runtime{};
      runtime.task_systems = desc.task_systems;
      runtime.render_system = std::make_unique<RenderSystem>(
          desc.renderer, desc.shadow, runtime.graphics_backend,
          desc.imgui_enabled);
      runtime.window_system.setFrameDataSink(runtime.window_frame);
      if (auto window_result = runtime.window_system.init(desc.windows);
          !window_result) {
         return std::unexpected(window_result.error());
      }
      *runtime.window_frame = runtime.window_system.frameData();
      runtime.render_pipelines.clear();
      runtime.render_pipelines.reserve(runtime.window_system.windows().size());
      for (const auto &window : runtime.window_system.windows()) {
         runtime.render_pipelines.push_back(WindowRenderPipeline{
             .window = window.handle,
             .window_id = window.id,
             .graph = runtime.render_system->buildStaticGraph(window.handle)});
      }
      if (desc.imgui_enabled) {
         runtime.gui_system = std::make_unique<GuiSystem>();
      }

      runtime.snapshot.graphics_api = desc.graphics_api;
      runtime.snapshot.renderer = desc.renderer;
      runtime.snapshot.shadow = desc.shadow;
      runtime.snapshot.imgui_enabled = desc.imgui_enabled;
      runtime.snapshot.asset_system = std::string(runtime.asset_system.name());
      runtime.snapshot.resource_system =
          std::string(runtime.resource_system.name());
      runtime.snapshot.scene_system = std::string(runtime.scene_system.name());
      runtime.snapshot.task_graph_system =
          std::string(runtime.task_graph_system.name());
      runtime.snapshot.shader_system =
          std::string(runtime.shader_system.name());
      runtime.snapshot.render_system =
          std::string(runtime.render_system->name());
      runtime.snapshot.window_system =
          std::string(runtime.window_system.name());
      runtime.snapshot.gui_system =
          runtime.gui_system ? std::string(runtime.gui_system->name())
                             : "Disabled";
      runtime.snapshot.task_systems.reserve(runtime.task_systems.size());
      for (const auto &task_system : runtime.task_systems) {
         runtime.snapshot.task_systems.push_back(
             task_system ? std::string(task_system->name())
                         : "UnnamedTaskSystem");
      }

      return runtime;
   }

} // namespace vve::v3::detail
