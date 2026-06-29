export module VEEngine.Simple.Handle;
export import VEEngine.Handle;

/// @file
/// @brief simple handle module re-exporting the facade-owned typed handle vocabulary.

export namespace vve::simple {

	using vve::HandleHash;						///< Shared typed-handle hash.
	using vve::TypedHandle;						///< Shared typed-handle value type.
	using vve::makeCounterHandle;				///< Shared runtime counter-handle builder.
	using vve::makeHandleForTest;				///< Shared deterministic test-handle builder.
	using vve::makeSlotMapHandleForTest;	///< Shared deterministic slot-map test-handle builder.

} // namespace vve::simple
