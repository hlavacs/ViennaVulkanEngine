module;

#include "FacadeMacros.hpp"

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
         [[maybe_unused]] const auto begin_frame_task = builder.addTask(
             "task.begin_frame", TaskKernelId::begin_frame,
             detail::requireFrame([this](const FrameContext &frame_context) { return beginFrame(frame_context); }),
             {}, {}, "Begin Frame", TaskPhase::begin_frame);
         [[maybe_unused]] const auto end_frame_task = builder.addTask(
             "task.end_frame", TaskKernelId::end_frame,
             detail::requireFrame([this](const FrameContext &frame_context) { return endFrame(frame_context); }), {},
             {}, "End Frame", TaskPhase::end_frame);
      }

   private:
      bool initialized_{false}; ///< Tracks whether backend initialization has completed.
   };

   /// @brief Constructs the public graphics-backend facade around the concrete implementation.
   VVE_V3_DEFINE_FACADE_CTOR(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, (), ())

   /// @brief Returns the backend name for the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, name, (), (),
                               const noexcept, std::string_view)

   /// @brief Returns the graphics API through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, api, (), (),
                               const noexcept, vve::GraphicsApi)

   /// @brief Initializes the backend through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, init, (), (), ,
                               std::expected<void, vve::Error>)

   /// @brief Begins a frame through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, beginFrame,
                               (const FrameContext &frame_context), (frame_context), ,
                               std::expected<void, vve::Error>)

   /// @brief Ends a frame through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, endFrame,
                               (const FrameContext &frame_context), (frame_context), ,
                               std::expected<void, vve::Error>)

   /// @brief Registers backend tasks through the public facade.
   VVE_V3_DEFINE_FACADE_VOID_METHOD(GraphicsBackendFacade, VulkanGraphicsBackendImplementation, registerTasks,
                                    (TaskGraphBuilder &builder), (builder), )

   /// @brief Emits the explicit graphics-backend facade instantiation for v3.
   template class GraphicsBackendFacade<VulkanGraphicsBackendImplementation>;

} // namespace vve::v3
