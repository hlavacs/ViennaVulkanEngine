module;

#include <cctype>
#include <cstdlib>

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

   namespace {

   /// @brief Resolves built-in shader files next to the v3 implementation sources.
   [[nodiscard]] std::filesystem::path builtInShaderPath(std::string_view filename) {
      return std::filesystem::path(__FILE__).parent_path() / "shaders" / filename;
   }

   struct RuntimeShaderProgram {
      ShaderHandle shader_program{};          ///< Compiled shader program handle.
      PipelineLayoutDesc pipeline_layout{};   ///< Backend-facing layout derived from reflection.
   };

   using RuntimeShaderPrograms = std::map<std::tuple<vve::RendererKind, vve::ShadowKind>, RuntimeShaderProgram>;

   /// @brief Returns an environment variable value as an owning string.
   [[nodiscard]] std::string environmentValue(const char *name) {
      if (name == nullptr) {
         return {};
      }

      const char *value = std::getenv(name);
      return value == nullptr ? std::string{} : std::string(value);
   }

   /// @brief Returns the CMake-configured default Vulkan ICD selector.
   [[nodiscard]] std::string defaultVulkanIcdSelector() {
#ifdef VVE_DEFAULT_VULKAN_ICD
      return std::string{VVE_DEFAULT_VULKAN_ICD};
#else
      return {};
#endif
   }

   /// @brief Returns a lowercase copy for small environment selector values.
   [[nodiscard]] std::string lowerAscii(std::string value) {
      std::ranges::transform(value, value.begin(), [](unsigned char character) {
         return static_cast<char>(std::tolower(character));
      });
      return value;
   }

   /// @brief Sets an environment variable before the Vulkan loader is touched by SDL or the backend.
   [[nodiscard]] bool setProcessEnvironment(const char *name, const std::filesystem::path &value) {
      const auto value_string = value.string();
#if defined(_WIN32)
      return _putenv_s(name, value_string.c_str()) == 0;
#else
      return setenv(name, value_string.c_str(), 1) == 0;
#endif
   }

   /// @brief Returns candidate KosmicKrisp ICD manifests from explicit, SDK, and system locations.
   [[nodiscard]] std::vector<std::filesystem::path> kosmicKrispIcdCandidates() {
      std::vector<std::filesystem::path> candidates{};

      if (const auto explicit_manifest = environmentValue("VVE_KOSMICKRISP_ICD"); !explicit_manifest.empty()) {
         candidates.emplace_back(explicit_manifest);
      }

#if defined(VVE_VULKAN_SDK_ROOT)
      candidates.emplace_back(std::filesystem::path(VVE_VULKAN_SDK_ROOT) / "share" / "vulkan" / "icd.d" /
                              "libkosmickrisp_icd.json");
#endif

      if (const auto vulkan_sdk = environmentValue("VULKAN_SDK"); !vulkan_sdk.empty()) {
         candidates.emplace_back(std::filesystem::path(vulkan_sdk) / "share" / "vulkan" / "icd.d" /
                                 "libkosmickrisp_icd.json");
      }

#if defined(__APPLE__)
      candidates.emplace_back("/usr/local/share/vulkan/icd.d/libkosmickrisp_icd.json");
      candidates.emplace_back("/opt/homebrew/share/vulkan/icd.d/libkosmickrisp_icd.json");
#endif

      return candidates;
   }

   /// @brief Finds the first existing KosmicKrisp ICD manifest.
   [[nodiscard]] std::optional<std::filesystem::path> findKosmicKrispIcdManifest() {
      for (const auto &candidate : kosmicKrispIcdCandidates()) {
         std::error_code error_code{};
         if (std::filesystem::exists(candidate, error_code) && !error_code) {
            return std::filesystem::weakly_canonical(candidate, error_code);
         }
      }

      return std::nullopt;
   }

   /**
    * @brief Applies the engine's optional Vulkan ICD selector before any Vulkan user initializes.
    *
    * `VK_ICD_FILENAMES` remains the authoritative Vulkan-loader override. The
    * engine selector is only a convenience for project launch scripts and
    * examples; it resolves to the SDK/system KosmicKrisp ICD manifest.
    */
   [[nodiscard]] std::expected<void, vve::Error> configureVulkanIcdSelection() {
      auto requested_icd = environmentValue("VVE_VULKAN_ICD");
      if (requested_icd.empty()) {
         requested_icd = defaultVulkanIcdSelector();
      }

      requested_icd = lowerAscii(std::move(requested_icd));
      if (requested_icd.empty() || requested_icd == "default" || requested_icd == "system") {
         return {};
      }

      if (!environmentValue("VK_ICD_FILENAMES").empty()) {
         std::clog << "[VulkanRuntime] VK_ICD_FILENAMES is already set; VVE_VULKAN_ICD=" << requested_icd
                   << " will not override it.\n";
         return {};
      }

      if (requested_icd != "kosmickrisp") {
         std::cerr << "[VulkanRuntime] Unsupported VVE_VULKAN_ICD value: " << requested_icd << '\n';
         return std::unexpected(vve::Error::invalid_argument);
      }

      const auto manifest = findKosmicKrispIcdManifest();
      if (!manifest.has_value()) {
         std::cerr << "[VulkanRuntime] Unable to locate libkosmickrisp_icd.json for VVE_VULKAN_ICD=kosmickrisp.\n";
         return std::unexpected(vve::Error::file_not_found);
      }

      if (!setProcessEnvironment("VK_ICD_FILENAMES", *manifest)) {
         std::cerr << "[VulkanRuntime] Failed to set VK_ICD_FILENAMES=" << manifest->string() << '\n';
         return std::unexpected(vve::Error::internal_error);
      }

      std::clog << "[VulkanRuntime] VK_ICD_FILENAMES=" << manifest->string() << '\n';
      return {};
   }

   } // namespace

   /**
    * @brief Builds a deterministic handle from a stable name plus salt.
    * @param name Stable textual seed.
    * @param salt Numeric disambiguator appended to the name.
    * @return Deterministic handle derived from the formatted seed.
    */
   vve::Handle makeStableHandle(std::string_view name, std::uint64_t salt) {
      // Salted names let related resources share a semantic prefix while still
      // producing distinct stable handles.
      auto mixed = std::string{name};
      mixed.push_back(':');
      mixed += std::to_string(salt);
      return vve::Handle::fromHash(mixed);
   }

   /**
    * @brief Loads built-in shader variants required by the configured window renderers.
    * @param runtime Runtime owning the resource and shader systems.
    * @param desc Runtime descriptor with normalized window renderer ids and shadow mode.
    * @return Shader handles keyed by renderer family and shadow variant.
    */
   [[nodiscard]] std::expected<RuntimeShaderPrograms, vve::Error>
   loadShaderProgramsForWindows(Runtime &runtime, const EngineRuntimeDesc &desc) {
      RuntimeShaderPrograms shader_programs{};
      for (const auto &window : desc.windows) {
         const auto renderer = runtime.graphics_backend.createRenderer(window.renderer_id);
         if (!renderer) {
            std::cerr << "[VulkanRuntime] Unsupported renderer id for window '" << window.id
                      << "': " << window.renderer_id << '\n';
            return std::unexpected(renderer.error());
         }

         const auto shader_key = std::tuple{renderer->kind, desc.shadow};
         if (shader_programs.contains(shader_key)) {
            continue;
         }

         const auto shader_metadata =
             runtime.resource_system.loadShaderProgram(builtInShaderPath("rasterizer.slang"),
                                                       runtime.shader_system, renderer->kind, desc.shadow);
         if (!shader_metadata) {
            std::cerr << "[VulkanRuntime] Failed to load shader program for renderer '" << renderer->id << "'\n";
            return std::unexpected(shader_metadata.error());
         }

         const auto pipeline_layout = runtime.graphics_backend.createPipelineLayout(*renderer, *shader_metadata);
         if (!pipeline_layout) {
            std::cerr << "[VulkanRuntime] Failed to create pipeline layout for renderer '" << renderer->id << "'\n";
            return std::unexpected(pipeline_layout.error());
         }

         shader_programs.emplace(shader_key, RuntimeShaderProgram{.shader_program = shader_metadata->handle,
                                                                  .pipeline_layout = *pipeline_layout});
      }

      return shader_programs;
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
      runtime.scene_loader =
          std::make_unique<SceneLoader>(runtime.asset_system, runtime.resource_system, runtime.scene_system);
      runtime.render_system =
          std::make_unique<RenderSystem>(desc.renderer, desc.shadow, runtime.graphics_backend, desc.imgui_enabled);
      auto shader_programs = loadShaderProgramsForWindows(runtime, desc);
      if (!shader_programs) {
         return std::unexpected(shader_programs.error());
      }

      if (auto icd_result = configureVulkanIcdSelection(); !icd_result) {
         return std::unexpected(icd_result.error());
      }
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
         const auto renderer = runtime.graphics_backend.createRenderer(window.renderer_id);
         if (!renderer) {
            std::cerr << "[VulkanRuntime] Unsupported renderer id for window '" << window.id
                      << "': " << window.renderer_id << '\n';
            return std::unexpected(renderer.error());
         }

         const auto shader_program = shader_programs->find(std::tuple{renderer->kind, desc.shadow});
         if (shader_program == shader_programs->end()) {
            std::cerr << "[VulkanRuntime] Missing shader program for renderer '" << renderer->id << "'\n";
            return std::unexpected(vve::Error::internal_error);
         }

         // Each runtime window receives a static render graph during runtime
         // assembly so frame execution can reuse the same pipeline description.
         runtime.render_pipelines.push_back(
             WindowRenderPipeline{.window = window.handle,
                                  .window_id = window.id,
                                  .renderer = *renderer,
                                  .shader_program = shader_program->second.shader_program,
                                  .pipeline_layout = shader_program->second.pipeline_layout,
                                  .graph = runtime.render_system->buildStaticGraph(window.handle, *renderer)});
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
