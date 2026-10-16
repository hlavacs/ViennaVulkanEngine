export module VVEngine:Gui;
import std;
import :Implementation;
import VVEngine.Error;
import VVEngine.Types;

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
		/// @brief Loads an image file for the HUD; it stays loaded for the engine's lifetime.
		[[nodiscard]] inline auto loadImage(std::filesystem::path path) -> std::expected<GuiImageHandle, Error> {
			return impl_.loadImage(std::move(path));
		} ///< A missing or unreadable file returns io_error.
		/// @brief Returns the ImTextureID to pass to ImGui::Image; 0 until the GUI is running and the image is on the GPU.
		[[nodiscard]] inline auto imageId(GuiImageHandle image) const -> std::uint64_t { return impl_.imageId(image); }

	private:
		template <typename... TSystems> friend class Engine;

		using Impl = detail::GuiSystemImpl;	///< Wrapped implementation class.
		/// @brief Binds the facade wrapper to the implementation object owned by the engine.
		inline explicit GuiSystem(Impl &implementation) noexcept : impl_{implementation} {}

		Impl &impl_;	///< Non-owning reference to the wrapped implementation.
	};	///< Public GUI-system wrapper.

} // namespace vve
