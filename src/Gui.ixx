export module VEEngine:Gui;
import std;
#if defined(VVE_ENGINE_IMPLEMENTATION_IS_V5)
import VEEngine.V5;
#else
import VEEngine.V4;
#endif

/**
 * @file
 * @brief Public GUI facade backed by the selected engine implementation.
 */
export namespace vve {

   class GuiSystem {
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::GuiSystem;

   public:
      explicit GuiSystem(Impl &implementation) : impl_{implementation} {}
      GuiSystem(const GuiSystem &) = default;
      GuiSystem(GuiSystem &&) noexcept = default;
      GuiSystem &operator=(const GuiSystem &) = delete;
      GuiSystem &operator=(GuiSystem &&) noexcept = delete;

   private:
      Impl &impl_;
   }; ///< Public GUI-system wrapper.

} // namespace vve
