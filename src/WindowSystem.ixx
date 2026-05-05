export module VEEngine:WindowSystem;
import std;
import VEEngine.V4;
import VEEngine.Vector;
import :Window;

/**
 * @file
 * @brief Public window-system facade backed by the selected engine implementation.
 */
export namespace vve {

   class WindowSystem {
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowSystem;

   public:
      explicit WindowSystem(Impl &implementation) : impl_{implementation} {}
      WindowSystem(const WindowSystem &) = default;
      WindowSystem(WindowSystem &&) noexcept = default;
      WindowSystem &operator=(const WindowSystem &) = delete;
      WindowSystem &operator=(WindowSystem &&) noexcept = delete;

      [[nodiscard]] std::string_view name() const noexcept { return impl_.name(); }
      [[nodiscard]] std::size_t windowCount() const { return impl_.windowCount(); }
      [[nodiscard]] Vector<Window> windows() const {
         Vector<Window> result{};
         const auto implementation_windows = impl_.windows();
         result.reserve(implementation_windows.size());
         for (const auto window : implementation_windows) { result.push_back(Window{window.get()}); }
         return result;
      }
      [[nodiscard]] std::optional<Window> findWindow(std::string_view id) const {
         const auto *window = impl_.findWindow(id);
         return window == nullptr ? std::optional<Window>{} : std::optional<Window>{Window{*window}};
      }
      [[nodiscard]] std::optional<Window> findWindow(WindowHandle handle) const {
         const auto *window = impl_.findWindow(handle);
         return window == nullptr ? std::optional<Window>{} : std::optional<Window>{Window{*window}};
      }

   private:
      Impl &impl_;
   }; ///< Public window-system wrapper.

} // namespace vve
