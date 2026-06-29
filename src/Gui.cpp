module VEEngine;
import :Gui;
import VEEngine.Simple;

namespace vve {

	/// @brief Stores the erased implementation reference used by engine-owned GUI systems.
	GuiSystem::GuiSystem(void *implementation) noexcept : impl_{implementation} {}

} // namespace vve
