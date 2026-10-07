module VVEngine;
import :Gui;

namespace vve {

	/// @brief Binds the facade wrapper to the implementation object owned by the engine.
	GuiSystem::GuiSystem(Impl &implementation) noexcept : impl_{implementation} {}

	/// @brief Stores the user callback that builds one GUI frame in the selected implementation.
	auto GuiSystem::draw(std::function<void()> frame) -> void { impl_.draw(std::move(frame)); }

	/// @brief Registers font atlas setup before the first rendered frame; rejects changes once its context exists.
	auto GuiSystem::configureFonts(std::function<void()> setup) -> std::expected<void, Error> {
		return impl_.configureFonts(std::move(setup));
	}

} // namespace vve
