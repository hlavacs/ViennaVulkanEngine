export module VEEngine:Gui;
import std;

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

	private:
		template <typename... TSystems> friend class Engine;

		template <typename TImplementation>
		inline explicit GuiSystem(TImplementation &implementation) : GuiSystem{static_cast<void *>(std::addressof(implementation))} {}
		explicit GuiSystem(void *implementation) noexcept;

		void *impl_{};	///< Opaque non-owning implementation pointer.
	};	///< Public GUI-system wrapper.

} // namespace vve
