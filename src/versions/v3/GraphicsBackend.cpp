module VEEngine.V3;
import std;
import :Internal;

namespace vve::v3 {

class VulkanGraphicsBackendImplementation {
public:
    [[nodiscard]] std::string_view name() const noexcept {
        return "VulkanGraphicsBackend";
    }

    [[nodiscard]] vve::GraphicsApi api() const noexcept {
        return vve::GraphicsApi::vulkan;
    }

    [[nodiscard]] std::expected<void, vve::Error> init() {
        initialized_ = true;
        return {};
    }

    [[nodiscard]] std::expected<void, vve::Error> beginFrame(
        const FrameContext&) {
        if (!initialized_) {
            return std::unexpected(vve::Error::not_initialized);
        }

        return {};
    }

    [[nodiscard]] std::expected<void, vve::Error> endFrame(
        const FrameContext&) {
        if (!initialized_) {
            return std::unexpected(vve::Error::not_initialized);
        }

        return {};
    }

    void registerTasks(TaskGraphBuilder& builder) {
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

template <>
GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::GraphicsBackendFacade()
    : implementation_(
          new VulkanGraphicsBackendImplementation(),
          [](void* implementation) {
              delete static_cast<VulkanGraphicsBackendImplementation*>(implementation);
          }) {
}

template <>
GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::~GraphicsBackendFacade() = default;

template <>
std::string_view GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::name() const noexcept {
    return static_cast<VulkanGraphicsBackendImplementation*>(implementation_.get())->name();
}

template <>
vve::GraphicsApi GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::api() const noexcept {
    return static_cast<VulkanGraphicsBackendImplementation*>(implementation_.get())->api();
}

template <>
std::expected<void, vve::Error> GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::init() {
    return static_cast<VulkanGraphicsBackendImplementation*>(implementation_.get())->init();
}

template <>
std::expected<void, vve::Error> GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::beginFrame(
    const FrameContext& frame_context) {
    return static_cast<VulkanGraphicsBackendImplementation*>(implementation_.get())->beginFrame(frame_context);
}

template <>
std::expected<void, vve::Error> GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::endFrame(
    const FrameContext& frame_context) {
    return static_cast<VulkanGraphicsBackendImplementation*>(implementation_.get())->endFrame(frame_context);
}

template <>
void GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::registerTasks(TaskGraphBuilder& builder) {
    static_cast<VulkanGraphicsBackendImplementation*>(implementation_.get())->registerTasks(builder);
}

template class GraphicsBackendFacade<VulkanGraphicsBackendImplementation>;

} // namespace vve::v3
