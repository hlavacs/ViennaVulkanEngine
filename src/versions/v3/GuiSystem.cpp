module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 GUI-system implementation.
 *
 * The current implementation is intentionally thin and exists mainly to keep
 * GUI integration behind a stable subsystem facade.
 */
namespace vve::v3 {

   /**
    * @brief Concrete Dear ImGui integration seam used by v3.
    */
   class ImGuiSystemImplementation {
   public:
      /// @brief Returns the subsystem name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "ImGuiSystem"; }

      /// @brief Initializes GUI resources against the active graphics backend.
      [[nodiscard]] std::expected<void, vve::Error> init(GraphicsBackend &) {
         // The placeholder implementation tracks initialization state only.
         initialized_ = true;
         return {};
      }

   private:
      bool initialized_{false}; ///< Tracks whether GUI initialization has completed.
   };

   /// @brief Constructs the public GUI-system facade around the concrete implementation.
   template <>
   GuiSystemFacade<ImGuiSystemImplementation>::GuiSystemFacade()
       : implementation_(new ImGuiSystemImplementation(),
                         [](ImGuiSystemImplementation *implementation) { delete implementation; }) {}

   /// @brief Returns the GUI-system name for the public facade.
   std::string_view GuiSystemFacade<ImGuiSystemImplementation>::name() const noexcept {
      return implementation_->name();
   }

   /// @brief Initializes the GUI system through the public facade.
   template <>
   std::expected<void, vve::Error> GuiSystemFacade<ImGuiSystemImplementation>::init(
       GraphicsBackendFacade<VulkanGraphicsBackendImplementation> &graphics_backend) {
      return implementation_->init(graphics_backend);
   }

   /// @brief Emits the explicit GUI-system facade instantiation for v3.
   template class GuiSystemFacade<ImGuiSystemImplementation>;

} // namespace vve::v3
