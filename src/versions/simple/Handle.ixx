export module VEEngine.Simple.Handle;
import std;
export import VEEngine.V5.Handle;

/**
	* @file
	* @brief Simple-engine handle identity aliases backed by the v5 strong uint64_t helper.
	*
	* Functional objects:
	* - TypedHandle aliases the v5 typed 64-bit identity value for simple-engine resources.
	* - HandleHash aliases the v5 payload hash for associative side tables owned by other systems.
	* - makeCounterHandle and test factories forward to v5 helpers without adding storage logic.
	*
	* Handles are non-owning identities only. Resource ownership and lifetime remain with the
	* subsystem containers that issue, validate, and erase the referenced objects.
	*/
export namespace vve::simple {

	template <typename TTag>
	using TypedHandle = vve::v5::TypedHandle<TTag>; ///< Strong resource identity; zero is invalid and no object is owned.

	template <typename THandle>
	using HandleHash = vve::v5::HandleHash<THandle>; ///< Hash helper for externally owned handle-keyed tables.

	/// @brief Creates a process-local typed identity; the owning subsystem still stores the resource.
	template <typename THandle> [[nodiscard]] inline THandle makeCounterHandle() {
		return vve::v5::makeCounterHandle<THandle>();
	}

	/// @brief Creates a deterministic counter identity for tests without registering any resource.
	template <typename THandle>
	[[nodiscard]] constexpr THandle makeHandleForTest(std::uint64_t id) noexcept {
		return vve::v5::makeHandleForTest<THandle>(id);
	}

	/// @brief Creates a deterministic slot-map-shaped identity for layout tests only.
	template <typename THandle>
	[[nodiscard]] constexpr THandle makeSlotMapHandleForTest(std::uint32_t slot_index,
																				std::uint32_t generation) noexcept {
		return vve::v5::makeSlotMapHandleForTest<THandle>(slot_index, generation);
	}

} // namespace vve::simple
