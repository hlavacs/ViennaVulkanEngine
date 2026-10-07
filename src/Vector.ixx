/// @file
/// @brief Shared contiguous sequence vocabulary for the facade and engine implementations.
export module VVEngine.Vector;

import std;

export namespace vve {
	/// @brief Standard vector storage; growth follows std::vector iterator and reference invalidation rules.
	template <typename T> using Vector = std::vector<T>;
} // namespace vve
