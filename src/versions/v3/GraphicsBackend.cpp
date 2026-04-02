module VEEngine.V3;
import std;
import :Internal;

namespace vve::v3 {

namespace {

class VulkanGraphicsBackend final : public IGraphicsBackend {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "VulkanGraphicsBackend";
    }

    [[nodiscard]] vve::GraphicsApi api() const noexcept override {
        return vve::GraphicsApi::vulkan;
    }

    [[nodiscard]] std::expected<void, vve::Error> init() override {
        initialized_ = true;
        return {};
    }

    [[nodiscard]] std::expected<void, vve::Error> beginFrame(
        const FrameContext&) override {
        if (!initialized_) {
            return std::unexpected(vve::Error::not_initialized);
        }

        return {};
    }

    [[nodiscard]] std::expected<void, vve::Error> endFrame(
        const FrameContext&) override {
        if (!initialized_) {
            return std::unexpected(vve::Error::not_initialized);
        }

        return {};
    }

    void registerTasks(TaskGraphBuilder& builder) override {
        const auto begin_frame_task = builder.addTask(
            "task.begin_frame",
            TaskKernelId::begin_frame,
            {},
            {},
            {},
            "Begin Frame");
        const auto end_frame_task = builder.addTask(
            "task.end_frame",
            TaskKernelId::end_frame,
            {},
            {},
            {},
            "End Frame");

        builder.setTaskCallback(
            begin_frame_task,
            [this](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Error> {
                if (execution_context.frame_context == nullptr) {
                    return std::unexpected(vve::Error::invalid_argument);
                }

                return beginFrame(*execution_context.frame_context);
            });

        builder.setTaskCallback(
            end_frame_task,
            [this](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Error> {
                if (execution_context.frame_context == nullptr) {
                    return std::unexpected(vve::Error::invalid_argument);
                }

                return endFrame(*execution_context.frame_context);
            });
    }

private:
    bool initialized_{false};
};

} // namespace

std::expected<std::unique_ptr<IGraphicsBackend>, vve::Error> detail::createGraphicsBackend(
    vve::GraphicsApi api) {
    switch (api) {
        case vve::GraphicsApi::vulkan:
            return std::make_unique<VulkanGraphicsBackend>();
        case vve::GraphicsApi::direct3d12:
        case vve::GraphicsApi::metal:
            return std::unexpected(vve::Error::unsupported_version);
    }

    return std::unexpected(vve::Error::internal_error);
}

} // namespace vve::v3
