module VEEngine;
import :Gui;
import VEEngine.Simple;

namespace vve {

	namespace {
		/// @brief Recovers the selected implementation GUI system from the erased facade pointer.
		[[nodiscard]] auto guiSystemImpl(void *implementation) -> VVE_ENGINE_IMPLEMENTATION_NAMESPACE::GuiSystem & {
			return *static_cast<VVE_ENGINE_IMPLEMENTATION_NAMESPACE::GuiSystem *>(implementation);
		}
	} // namespace

	/// @brief Stores the erased implementation reference used by engine-owned GUI systems.
	GuiSystem::GuiSystem(void *implementation) noexcept : impl_{implementation} {}

	/// @brief Stores the user callback that builds one GUI frame in the selected implementation.
	auto GuiSystem::draw(std::function<void()> frame) -> void { guiSystemImpl(impl_).draw(std::move(frame)); }

} // namespace vve
