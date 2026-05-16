export module VEEngine.V5:Gui;
import std;
export import :RenderPass;
export import :Types;

/// @file
/// @brief Tiny GUI descriptor table used until a real GUI backend is added.

namespace vve::v5 {

   /// @brief Internal GUI widget record.
   struct GuiWidgetRecord {
      GuiWidgetHandle handle{}; ///< Stable widget handle.
      std::string label{};      ///< Text shown by the widget.
   };

} // namespace vve::v5

export namespace vve::v5 {

   /// @brief Minimal GUI registry.
   class GuiSystem {
   public:
      [[nodiscard]] std::expected<GuiWidgetHandle, Error> label(std::string text);
      [[nodiscard]] bool containsWidget(GuiWidgetHandle handle) const;
      [[nodiscard]] std::expected<std::string, Error> widgetLabel(GuiWidgetHandle handle) const;
      [[nodiscard]] std::size_t widgetCount() const;
      [[nodiscard]] static constexpr std::span<const RenderPassContract> passes() noexcept;

   private:
      std::map<GuiWidgetHandle, GuiWidgetRecord> widgets_{}; ///< Widgets by handle.
   };

} // namespace vve::v5

namespace vve::v5::detail {

   inline constexpr std::string_view gui_overlay_pass{"gui.overlay_pass"};         ///< Real GUI overlay pass.
   inline constexpr std::array gui_pass_dependencies{RenderMilestone::scene_color()}; ///< GUI needs scene color.
   inline constexpr std::array gui_done_dependencies{gui_overlay_pass};             ///< GUI milestone input.
   inline constexpr std::array gui_frame_dependencies{RenderMilestone::gui()};        ///< Final frame input.
   inline constexpr std::array gui_pass_contracts{                                  ///< GUI graph wiring.
       RenderPassContract{.name = gui_overlay_pass,
                          .depends_on = gui_pass_dependencies,
                          .shader_file = "Gui.slang",
                          .vertex_entry = "vveGuiVertexMain",
                          .fragment_entry = "vveGuiFragmentMain",
                          .inputs = "scene color target, GUI draw data",
                          .outputs = "color target with GUI overlay"},
       RenderPassContract{.name = RenderMilestone::gui(),
                          .depends_on = gui_done_dependencies,
                          .outputs = "GUI overlay is ready",
                          .milestone = true},
       RenderPassContract{.name = RenderMilestone::frame_finished(),
                          .depends_on = gui_frame_dependencies,
                          .outputs = "frame can be presented",
                          .milestone = true}};

} // namespace vve::v5::detail

export namespace vve::v5 {

   /// @brief Adds a text label and returns its handle.
   inline std::expected<GuiWidgetHandle, Error> GuiSystem::label(std::string text) {
      const auto handle = makeCounterHandle<GuiWidgetHandle>();
      const auto [_, inserted] = widgets_.emplace(handle, GuiWidgetRecord{.handle = handle, .label = std::move(text)});
      if (!inserted) { return std::unexpected(Error::duplicate_object); }
      return handle;
   }

   /// @brief Returns whether a widget exists.
   inline bool GuiSystem::containsWidget(GuiWidgetHandle handle) const { return widgets_.contains(handle); }

   /// @brief Returns the label text for a widget.
   inline std::expected<std::string, Error> GuiSystem::widgetLabel(GuiWidgetHandle handle) const {
      const auto widget = widgets_.find(handle);
      if (widget == widgets_.end()) { return std::unexpected(Error::missing_object); }
      return widget->second.label;
   }

   /// @brief Returns widget count.
   inline std::size_t GuiSystem::widgetCount() const { return widgets_.size(); }

   /// @brief Returns the GUI system render pass list.
   constexpr std::span<const RenderPassContract> GuiSystem::passes() noexcept { return detail::gui_pass_contracts; }

} // namespace vve::v5
