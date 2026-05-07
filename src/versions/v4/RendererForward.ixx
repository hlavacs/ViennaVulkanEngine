export module VEEngine.V4:RendererForward;
import std;

/// @file
/// @brief Concrete forward-renderer identity and default feature choices.

export namespace vve::v4 {

   /// @brief Tiny placeholder for the first concrete renderer implementation.
   class RendererForward {
   public:
      [[nodiscard]] static constexpr std::string_view id() noexcept { return "forward"; }

      [[nodiscard]] static constexpr bool usesShadowMaps() noexcept { return true; }

   };

} // namespace vve::v4
