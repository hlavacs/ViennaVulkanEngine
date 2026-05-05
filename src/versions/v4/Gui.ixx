export module VEEngine.V4:Gui;
import std;
export import :Types;

/// @file
/// @brief Tiny GUI descriptor table used until a real GUI backend is added.

namespace vve::v4 {

   /// @brief Internal GUI widget record.
   struct GuiWidgetRecord {
      GuiWidgetHandle handle{}; ///< Stable widget handle.
      std::string label{};      ///< Text shown by the widget.
   };

} // namespace vve::v4

export namespace vve::v4 {

   /// @brief Minimal GUI registry.
   class GuiSystem {
   public:
      /// @brief Adds a text label and returns its handle.
      [[nodiscard]] std::expected<GuiWidgetHandle, Error> label(std::string text) {
         const auto handle = makeCounterHandle<GuiWidgetHandle>();
         const auto [_, inserted] = widgets_.emplace(handle, GuiWidgetRecord{.handle = handle, .label = std::move(text)});
         if (!inserted) { return std::unexpected(Error::duplicate_object); }
         return handle;
      }

      /// @brief Returns whether a widget exists.
      [[nodiscard]] bool containsWidget(GuiWidgetHandle handle) const { return widgets_.contains(handle); }

      /// @brief Returns the label text for a widget.
      [[nodiscard]] std::expected<std::string, Error> widgetLabel(GuiWidgetHandle handle) const {
         const auto widget = widgets_.find(handle);
         if (widget == widgets_.end()) { return std::unexpected(Error::missing_object); }
         return widget->second.label;
      }

      /// @brief Returns widget count.
      [[nodiscard]] std::size_t widgetCount() const { return widgets_.size(); }

   private:
      std::map<GuiWidgetHandle, GuiWidgetRecord> widgets_{}; ///< Widgets by handle.
   };

} // namespace vve::v4
