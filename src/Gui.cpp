module VEEngine;
import :Gui;

namespace vve {

	/// @brief Binds the facade wrapper to the implementation object owned by the engine.
	GuiSystem::GuiSystem(Impl &implementation) noexcept : impl_{implementation} {}

	/// @brief Stores the user callback that builds one GUI frame in the selected implementation.
	auto GuiSystem::draw(std::function<void()> frame) -> void { impl_.draw(std::move(frame)); }

} // namespace vve
