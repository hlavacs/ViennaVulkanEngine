#include <string_view>
#include <vector>

import VEEngine.V3;

namespace {

class RecordingTaskSystem final : public vve::v3::ITaskSystem {
public:
    explicit RecordingTaskSystem(std::vector<int>& events)
        : events_(events) {
    }

    [[nodiscard]] std::string_view name() const noexcept override {
        return "RecordingTaskSystem";
    }

    void registerTasks(
        vve::v3::TaskGraphBuilder& builder,
        const vve::v3::SceneData&) override {
        [[maybe_unused]] const auto game_update = builder.addTask(
            "game.update",
            vve::v3::TaskKernelId::none,
            [this](const vve::v3::TaskExecutionContext&) -> std::expected<void, vve::Result> {
                events_.push_back(3);
                return {};
            },
            {vve::v3::TaskNodeHandle{vve::Handle::fromHash(std::string_view{"engine.upload"})}});
    }

private:
    std::vector<int>& events_;
};

} // namespace

int main() {
    {
        std::vector<int> events{};
        vve::v3::TaskGraphBuilder builder{};

        const auto transforms = builder.addTask(
            "engine.transforms",
            vve::v3::TaskKernelId::update_transforms,
            [&events](const vve::v3::TaskExecutionContext&) -> std::expected<void, vve::Result> {
                events.push_back(1);
                return {};
            });

        const auto uploads = builder.addTask(
            "engine.upload",
            vve::v3::TaskKernelId::upload_resources,
            [&events](const vve::v3::TaskExecutionContext&) -> std::expected<void, vve::Result> {
                events.push_back(2);
                return {};
            },
            {transforms});

        RecordingTaskSystem system(events);
        system.registerTasks(builder, {});

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

        const std::vector<int> expected{1, 2, 3};
        if (events != expected) {
            return 2;
        }
    }

    {
        vve::v3::TaskGraphBuilder builder{};
        [[maybe_unused]] const auto invalid_task = builder.addTask(
            "task.invalid",
            vve::v3::TaskKernelId::none,
            {},
            {vve::v3::TaskNodeHandle{vve::Handle::fromHash(std::string_view{"task.missing"})}});

        const auto graph = std::move(builder).build();
        const auto result = vve::v3::executeTaskGraph(graph, {});
        if (result || result.error() != vve::Result::invalid_argument) {
            return 3;
        }
    }

    return 0;
}
