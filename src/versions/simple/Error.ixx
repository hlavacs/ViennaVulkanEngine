export module VEEngine.Simple.Error;
import VEEngine.Error;

/// @file
/// @brief simple error codes shared with the facade error contract.
export namespace vve::simple {

	using Error = vve::Error;										///< Shared facade error vocabulary.
	using vve::errorName;											///< Shared facade diagnostic name lookup.

} // namespace vve::simple
