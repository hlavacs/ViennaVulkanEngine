#include <string_view>
#include <vector>

import VEEngine.V3;

/**
 * @file
 * @brief Regression tests for task-graph construction and execution.
 */
namespace {

/**
 * @brief Test task system that appends a marker when executed.
 */
class RecordingTaskSystem final : public vve::v3::ITaskSystem {
public:
    /// @brief Creates the recording task system over the shared event vector.
    explicit RecordingTaskSystem(std::vector<int>& events)
        : events_(events) {
    }

    /// @brief Returns the task-system name for diagnostics.
    [[nodiscard]] std::string_view name() const noexcept override {
        return "RecordingTaskSystem";
    }

    /// @brief Registers a single post-frame task that records execution.
    void registerTasks(
        vve::v3::TaskGraphBuilder& builder,
        const vve::v3::SceneData&) override {
        [[maybe_unused]] const auto game_update = builder.addTask(
            "game.update",
            vve::v3::TaskKernelId::none,
            [this](const vve::v3::TaskExecutionContext&) -> std::expected<void, vve::Error> {
                events_.push_back(3);
                return {};
            },
            {vve::v3::TaskGraphBuilder::taskHandleFor("engine.upload")},
            {},
            {},
            vve::v3::TaskPhase::post_frame);
    }

private:
    std::vector<int>& events_; ///< Shared event log used to verify execution order.
};

} // namespace

/**
 * @brief Executes the task-graph regression tests.
 * @return Process exit code expected by the lightweight test runner.
 */
int main() {
    {
        // Verify that task execution respects dependency order, including tasks
        std::vector<int> events{}; // registered later by user task systems.
        vve::v3::TaskGraphBuilder builder{};

        const auto transforms = builder.addTask(
            "engine.transforms",
            vve::v3::TaskKernelId::update_transforms,
            [&events](const vve::v3::TaskExecutionContext&) -> std::expected<void, vve::Error> {
                events.push_back(1);
                return {};
            });

        const auto uploads = builder.addTask(
            "engine.upload",
            vve::v3::TaskKernelId::upload_resources,
            [&events](const vve::v3::TaskExecutionContext&) -> std::expected<void, vve::Error> {
                events.push_back(2);
                return {};
            },
            {transforms});

        RecordingTaskSystem system(events);
        system.registerTasks(builder, {});

        if (!builder.containsTask("engine.upload")) {
            return 4;
        }

        [[maybe_unused]] const auto late_update = builder.addTask(
            "game.late_update",
            vve::v3::TaskKernelId::none,
            [&events](const vve::v3::TaskExecutionContext&) -> std::expected<void, vve::Error> {
                events.push_back(4);
                return {};
            },
            {},
            {},
            {},
            vve::v3::TaskPhase::post_frame);
        [[maybe_unused]] const auto dependency_added = builder.addDependency("game.late_update", "game.update");

        const auto graph = std::move(builder).build();
        const vve::v3::FrameContext frame_context{
            .frame_index = 7,
            .delta_seconds = 0.016
        };
        vve::v3::SceneData scene{};
        const auto result = vve::v3::executeTaskGraph(
            graph,
            vve::v3::TaskExecutionContext{
                .frame_context = &frame_context,
                .scene = &scene
            });
        if (!result) {
            return 1;
        }

        const std::vector<int> expected{1, 2, 3, 4};
        if (events != expected) {
            return 2;
        }
    }

    {
        vve::v3::TaskGraphBuilder builder{}; // Graph compilation must reject dependencies on missing tasks.
        [[maybe_unused]] const auto invalid_task = builder.addTask(
            "task.invalid",
            vve::v3::TaskKernelId::none,
            {},
            {vve::v3::TaskNodeHandle{vve::Handle::fromHash(std::string_view{"task.missing"})}});

        const auto graph = std::move(builder).build();
        const auto result = vve::v3::executeTaskGraph(graph, {});
        if (result || result.error() != vve::Error::invalid_argument) {
            return 3;
        }
    }

    {
        vve::v3::TaskGraphBuilder builder{}; // Invalid phase ordering must be caught during graph validation.
        [[maybe_unused]] const auto end_frame = builder.addTask(
            "task.end_frame",
            vve::v3::TaskKernelId::end_frame);
        [[maybe_unused]] const auto invalid_user_update = builder.addTask(
            "task.invalid_phase",
            vve::v3::TaskKernelId::none,
            {},
            {vve::v3::TaskGraphBuilder::taskHandleFor("task.end_frame")},
            {},
            {},
            vve::v3::TaskPhase::user_update);

        const auto graph = std::move(builder).build();
        const auto result = vve::v3::executeTaskGraph(graph, {});
        if (result || result.error() != vve::Error::invalid_argument) {
            return 5;
        }
    }

    return 0;
}
