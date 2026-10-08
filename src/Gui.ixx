export module VVEngine:Gui;
import std;
import :Implementation;

/**
	* @file
	* @brief Public GUI facade backed by the selected engine implementation.
	*/
export namespace vve {

	class GuiSystem {
	public:
		GuiSystem(const GuiSystem &) = default;
		GuiSystem(GuiSystem &&) noexcept = default;
		GuiSystem &operator=(const GuiSystem &) = delete;
		GuiSystem &operator=(GuiSystem &&) noexcept = delete;

		/// @brief Stores the user callback that builds one GUI frame in the selected implementation.
		inline auto draw(std::function<void()> frame) -> void { impl_.draw(std::move(frame)); }
		/// @brief Registers font atlas setup before the first rendered frame; rejects changes once its context exists.
		[[nodiscard]] inline auto configureFonts(std::function<void()> setup) -> std::expected<void, Error> {
			return impl_.configureFonts(std::move(setup));
		}

	private:
		template <typename... TSystems> friend class Engine;

		using Impl = detail::GuiSystemImpl;	///< Wrapped implementation class.
		/// @brief Binds the facade wrapper to the implementation object owned by the engine.
		inline explicit GuiSystem(Impl &implementation) noexcept : impl_{implementation} {}

		Impl &impl_;	///< Non-owning reference to the wrapped implementation.
	};	///< Public GUI-system wrapper.

} // namespace vve
