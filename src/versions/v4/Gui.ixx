export module VEEngine.V4:Gui;
import std;
export import :Types;

/// @file
/// @brief Tiny GUI descriptor table used until a real GUI backend is added.

export namespace vve::v4 {

   /// @brief GUI widget descriptor.
   struct GuiWidget {
      Handle handle{};    ///< Stable widget handle.
      std::string label{}; ///< Text shown by the widget.
   };

   /// @brief Minimal GUI registry.
   class GuiSystem {
   public:
      /// @brief Adds a text label and returns its handle.
      [[nodiscard]] std::expected<Handle, Error> label(std::string text) {
         const auto handle = makeHandle(ObjectKind::gui, static_cast<std::uint32_t>(widgets_.size()));
         auto added = widgets_.add(GuiWidget{.handle = handle, .label = std::move(text)});
         if (!added) { return std::unexpected(added.error()); }
         return handle;
      }

      /// @brief Finds a widget by handle, or returns null.
      [[nodiscard]] const GuiWidget *find(Handle handle) const { return widgets_.find(handle); }

      /// @brief Returns widget count.
      [[nodiscard]] std::size_t size() const { return widgets_.size(); }

   private:
      DescriptorMap<GuiWidget> widgets_{}; ///< Widgets by handle.
   };

} // namespace vve::v4
