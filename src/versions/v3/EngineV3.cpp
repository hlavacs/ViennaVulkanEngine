module VEEngine.V3;
import std;
#include "V3Internal.hpp"

namespace vve::v3 {

namespace {

class EngineImpl final : public vve::detail::EngineImpl {
public:
    explicit EngineImpl(const vve::EngineConfig& config)
        : application_name_("ViennaVulkanEngine"),
          validation_enabled_(false) {
        if (const auto application_name = config.tryGet<vve::ApplicationName>()) {
            application_name_ = application_name->value;
        }

        if (const auto enable_validation = config.tryGet<vve::EnableValidation>()) {
            validation_enabled_ = enable_validation->value;
        }

        if (const auto graphics_api = config.tryGet<vve::PreferredGraphicsApi>()) {
            runtime_desc_.graphics_api = graphics_api->value;
        }

        if (const auto renderer = config.tryGet<vve::PreferredRenderer>()) {
            runtime_desc_.renderer = renderer->value;
        }

        if (const auto shadow = config.tryGet<vve::PreferredShadow>()) {
            runtime_desc_.shadow = shadow->value;
        }

        if (const auto imgui = config.tryGet<vve::EnableImGui>()) {
            runtime_desc_.imgui_enabled = imgui->value;
        }
    }

    [[nodiscard]] bool isInitialized() const noexcept override {
        return initialized_;
    }

    [[nodiscard]] std::expected<void, vve::Result> init() override {
        if (isInitialized()) {
            return std::unexpected(vve::Result::already_initialized);
        }

        auto runtime = detail::createRuntime(runtime_desc_);
        if (!runtime) {
            return std::unexpected(runtime.error());
        }

        runtime_ = std::move(*runtime);
        if (auto backend_result = runtime_.graphics_backend->init(); !backend_result) {
            return backend_result;
        }

        if (runtime_.gui_system != nullptr) {
            if (auto gui_result = runtime_.gui_system->init(*runtime_.graphics_backend); !gui_result) {
                return gui_result;
            }
        }

        initialized_ = true;
        running_ = false;
        last_time_ = std::chrono::time_point_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now());
        return {};
    }

    [[nodiscard]] std::expected<void, vve::Result> run() override {
        if (!isInitialized()) {
            if (auto init_result = init(); !init_result) {
                return init_result;
            }
        }

        running_ = true;
        while (running_) {
            if (auto step_result = step(); !step_result) {
                running_ = false;
                return step_result;
            }
            running_ = false;
        }

        return {};
    }

    [[nodiscard]] std::expected<void, vve::Result> step() override {
        if (!isInitialized()) {
            return std::unexpected(vve::Result::not_initialized);
        }

        if (!scene_) {
            return std::unexpected(vve::Result::invalid_argument);
        }

        const auto current_time = std::chrono::time_point_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now());
        const double seconds_elapsed =
            std::chrono::duration<double>(current_time - last_time_).count();
        last_time_ = current_time;

        const FrameContext frame_context{
            .frame_index = frame_index_++,
            .delta_seconds = seconds_elapsed
        };

        const auto task_graph = runtime_.task_graph_system->build(*scene_, frame_context);
        const auto shader_metadata = runtime_.shader_system->reflect(
            "shaders/frame.slang",
            runtime_desc_.renderer,
            runtime_desc_.shadow);
        if (!shader_metadata) {
            return std::unexpected(shader_metadata.error());
        }

        const auto render_graph = runtime_.render_system->build(
            frame_context, *scene_, task_graph, *shader_metadata);
        return runtime_.render_system->render(frame_context, render_graph);
    }

    [[nodiscard]] std::expected<int, vve::Result> getVersionMajor() const noexcept override {
        return 3;
    }

    [[nodiscard]] std::expected<void, vve::Result> loadFile(
        const std::filesystem::path& file_path) override {
        if (!isInitialized()) {
            return std::unexpected(vve::Result::not_initialized);
        }

        if (file_path.empty()) {
            return std::unexpected(vve::Result::invalid_argument);
        }

        const auto imported_scene = runtime_.asset_system->importScene(file_path);
        if (!imported_scene) {
            return std::unexpected(imported_scene.error());
        }

        if (auto register_result = runtime_.resource_system->registerImportedScene(
                *imported_scene, file_path); !register_result) {
            return register_result;
        }

        const auto scene = runtime_.scene_system->instantiate(*imported_scene);
        if (!scene) {
            return std::unexpected(scene.error());
        }

        loaded_file_path_ = file_path;
        scene_ = std::move(*scene);
        return {};
    }

private:
    std::string application_name_;
    bool validation_enabled_{false};
    bool initialized_{false};
    bool running_{false};
    std::uint64_t frame_index_{0};
    EngineRuntimeDesc runtime_desc_{};
    detail::Runtime runtime_{};
    std::filesystem::path loaded_file_path_{};
    std::optional<SceneData> scene_{};
    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds>
        last_time_{};
};

} // namespace

std::unique_ptr<vve::detail::EngineImpl> makeEngine(const vve::EngineConfig& config) {
    return std::make_unique<EngineImpl>(config);
}

} // namespace vve::v3
