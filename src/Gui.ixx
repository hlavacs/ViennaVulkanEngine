export module VEEngine:Gui;
import std;
import VEEngine.Simple;

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

		auto draw(std::function<void()> frame) -> void;

	private:
		template <typename... TSystems> friend class Engine;

		using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::GuiSystem;	///< Wrapped implementation class.
		explicit GuiSystem(Impl &implementation) noexcept;

		Impl &impl_;	///< Non-owning reference to the wrapped implementation.
	};	///< Public GUI-system wrapper.

} // namespace vve
