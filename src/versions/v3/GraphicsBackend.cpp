module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 graphics-backend implementation.
 *
 * The backend currently models only frame-boundary lifecycle and task
 * registration, but it owns the seam where concrete Vulkan work will live.
 */
namespace vve::v3 {

   /**
    * @brief Concrete Vulkan backend implementation used by v3.
    */
   class VulkanGraphicsBackendImplementation {
   public:
      /// @brief Returns the backend name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "VulkanGraphicsBackend"; }

      /// @brief Returns the public graphics API implemented by this backend.
      [[nodiscard]] vve::GraphicsApi api() const noexcept { return vve::GraphicsApi::vulkan; }

      /// @brief Initializes backend-owned state.
      [[nodiscard]] std::expected<void, vve::Error> init() {
         initialized_ = true;
         return {};
      }

      /// @brief Performs begin-frame backend work.
      [[nodiscard]] std::expected<void, vve::Error> beginFrame(const FrameContext &) {
         if (!initialized_) {
            return std::unexpected(vve::Error::not_initialized);
         }

         return {};
      }

      /// @brief Performs end-frame backend work.
      [[nodiscard]] std::expected<void, vve::Error> endFrame(const FrameContext &) {
         if (!initialized_) {
            return std::unexpected(vve::Error::not_initialized);
         }

         return {};
      }

      /// @brief Registers the backend's built-in frame-boundary tasks.
      void registerTasks(TaskGraphBuilder &builder) {
          const auto begin_frame_task =
             builder.addTask("task.begin_frame", TaskKernelId::begin_frame, {}, {}, {}, "Begin Frame",
                             TaskPhase::begin_frame);
          const auto end_frame_task =
             builder.addTask("task.end_frame", TaskKernelId::end_frame, {}, {}, {}, "End Frame", TaskPhase::end_frame);

         [[maybe_unused]] const auto begin_callback_set = builder.setTaskCallback(
             begin_frame_task,
             [this](const TaskExecutionContext &execution_context) -> std::expected<void, vve::Error> {
                if (execution_context.frame_context == nullptr) {
                   return std::unexpected(vve::Error::invalid_argument);
                }

                return beginFrame(*execution_context.frame_context);
             });

         [[maybe_unused]] const auto end_callback_set = builder.setTaskCallback(
             end_frame_task, [this](const TaskExecutionContext &execution_context) -> std::expected<void, vve::Error> {
                if (execution_context.frame_context == nullptr) {
                   return std::unexpected(vve::Error::invalid_argument);
                }

                return endFrame(*execution_context.frame_context);
             });
      }

   private:
      bool initialized_{false};	///< Tracks whether backend initialization has completed.
   };

   /// @brief Constructs the public graphics-backend facade around the concrete implementation.
   template <>
   GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::GraphicsBackendFacade()
       : implementation_(new VulkanGraphicsBackendImplementation(),
                         [](VulkanGraphicsBackendImplementation *implementation) { delete implementation; }) {}

   /// @brief Returns the backend name for the public facade.
   std::string_view GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::name() const noexcept {
      return implementation_->name();
   }

   /// @brief Returns the graphics API through the public facade.
   template <> vve::GraphicsApi GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::api() const noexcept {
      return implementation_->api();
   }

   /// @brief Initializes the backend through the public facade.
   template <> std::expected<void, vve::Error> GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::init() {
      return implementation_->init();
   }

   /// @brief Begins a frame through the public facade.
   template <>
   std::expected<void, vve::Error>
   GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::beginFrame(const FrameContext &frame_context) {
      return implementation_->beginFrame(frame_context);
   }

   /// @brief Ends a frame through the public facade.
   template <>
   std::expected<void, vve::Error>
   GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::endFrame(const FrameContext &frame_context) {
      return implementation_->endFrame(frame_context);
   }

   /// @brief Registers backend tasks through the public facade.
   template <>
   void GraphicsBackendFacade<VulkanGraphicsBackendImplementation>::registerTasks(TaskGraphBuilder &builder) {
      implementation_->registerTasks(builder);
   }

   /// @brief Emits the explicit graphics-backend facade instantiation for v3.
   template class GraphicsBackendFacade<VulkanGraphicsBackendImplementation>;

} // namespace vve::v3
