module;

#include "FacadeMacros.hpp"

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
   VVE_V3_DEFINE_FACADE_CTOR(GuiSystemFacade, ImGuiSystemImplementation, (), ())

   /// @brief Returns the GUI-system name for the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GuiSystemFacade, ImGuiSystemImplementation, name, (), (), const noexcept,
                               std::string_view)

   /// @brief Initializes the GUI system through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(GuiSystemFacade, ImGuiSystemImplementation, init,
                               (GraphicsBackendFacade<VulkanGraphicsBackendImplementation> &graphics_backend),
                               (graphics_backend), , std::expected<void, vve::Error>)

   /// @brief Emits the explicit GUI-system facade instantiation for v3.
   template class GuiSystemFacade<ImGuiSystemImplementation>;

} // namespace vve::v3
