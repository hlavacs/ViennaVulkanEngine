module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 runtime assembly helpers.
 *
 * This file constructs the concrete runtime object that wires together all v3
 * subsystem implementations behind their public facades.
 */
namespace vve::v3::detail {

   /**
    * @brief Builds a deterministic handle from a stable name plus salt.
    * @param name Stable textual seed.
    * @param salt Numeric disambiguator appended to the name.
    * @return Deterministic handle derived from the formatted seed.
    */
   vve::Handle makeStableHandle(std::string_view name, std::uint64_t salt) {
      // Salted names let related resources share a semantic prefix while still
      // producing distinct stable handles.
      const auto mixed = std::format("{}:{}", name, salt);
      return vve::Handle::fromHash(mixed);
   }

   /**
    * @brief Assembles the concrete v3 runtime from the engine runtime descriptor.
    * @param desc Runtime descriptor produced from public engine configuration.
    * @return Fully assembled runtime object, or an error when the configuration is unsupported.
    */
   std::expected<Runtime, vve::Error> createRuntime(const EngineRuntimeDesc &desc) {
      // The current v3 runtime only implements Vulkan. Reject unsupported
      // backends early so later subsystem creation stays simple.
      if (desc.graphics_api != vve::GraphicsApi::vulkan) {
         return std::unexpected(vve::Error::unsupported_version);
      }

      Runtime runtime{};
      // External task systems are stored directly on the runtime so later task
      // graph rebuilds can flatten them into non-owning views.
      runtime.task_systems = desc.task_systems;
      runtime.render_system =
          std::make_unique<RenderSystem>(desc.renderer, desc.shadow, runtime.graphics_backend, desc.imgui_enabled);
      // Window polling owns the authoritative frame snapshot shared with the
      // rest of the frame systems.
      runtime.window_system.setFrameDataSink(runtime.window_frame);
      if (auto window_result = runtime.window_system.init(makeRange(desc.windows));
          !window_result) {
         return std::unexpected(window_result.error());
      }
      *runtime.window_frame = runtime.window_system.frameData();
      runtime.render_pipelines.clear();
      runtime.render_pipelines.reserve(runtime.window_system.windows().size());
      for (const auto &window : runtime.window_system.windows()) {
         // Each runtime window receives a static render graph during runtime
         // assembly so frame execution can reuse the same pipeline description.
         runtime.render_pipelines.push_back(
             WindowRenderPipeline{.window = window.handle,
                                  .window_id = window.id,
                                  .graph = runtime.render_system->buildStaticGraph(window.handle)});
      }
      if (desc.imgui_enabled) {
         // GUI integration stays optional and is omitted entirely when disabled.
         runtime.gui_system = std::make_unique<GuiSystem>();
      }

      // Capture a human-readable snapshot so diagnostics can inspect the
      // assembled runtime without traversing subsystem internals.
      runtime.snapshot.graphics_api = desc.graphics_api;
      runtime.snapshot.renderer = desc.renderer;
      runtime.snapshot.shadow = desc.shadow;
      runtime.snapshot.imgui_enabled = desc.imgui_enabled;
      runtime.snapshot.asset_system = std::string(runtime.asset_system.name());
      runtime.snapshot.resource_system = std::string(runtime.resource_system.name());
      runtime.snapshot.scene_system = std::string(runtime.scene_system.name());
      runtime.snapshot.task_graph_system = std::string(runtime.task_graph_system.name());
      runtime.snapshot.shader_system = std::string(runtime.shader_system.name());
      runtime.snapshot.render_system = std::string(runtime.render_system->name());
      runtime.snapshot.window_system = std::string(runtime.window_system.name());
      runtime.snapshot.gui_system = runtime.gui_system ? std::string(runtime.gui_system->name()) : "Disabled";
      runtime.snapshot.task_systems.reserve(runtime.task_systems.size());
      for (const auto &task_system : runtime.task_systems) {
         // Preserve task-system names in the snapshot even when the runtime
         // only stores owning pointers at execution time.
         runtime.snapshot.task_systems.push_back(task_system ? std::string(task_system->name()) : "UnnamedTaskSystem");
      }

      return runtime;
   }

} // namespace vve::v3::detail
