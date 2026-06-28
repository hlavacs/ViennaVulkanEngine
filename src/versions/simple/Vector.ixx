export module VEEngine.Simple.Vector;
import std;
export import VEEngine.V5.Vector;

/**
	* @file
	* @brief Simple-engine vector aliases backed by the v5 segmented vector helper.
	*
	* Functional objects:
	* - Vector aliases the facade-visible dynamic container type used by simple subsystems.
	* - VectorIterator and VectorConstIterator expose the mutable and immutable iterator surface.
	* - VectorConstRange and makeRange expose read-only range traversal without storage details.
	*
	* The simple engine reuses the v5 helper directly. Storage, allocation, and iterator
	* mechanics remain owned by `VEEngine.V5.Vector`; this module only names the common surface.
	*/
export namespace vve::simple {

	template <typename T, std::size_t SegmentSize = 256>
	using Vector = vve::v5::Vector<T, SegmentSize>; ///< Facade-visible vector container with v5-backed storage.

	template <typename T, std::size_t SegmentSize = 256>
	using VectorIterator = typename Vector<T, SegmentSize>::iterator; ///< Mutable random-access iterator alias.

	template <typename T, std::size_t SegmentSize = 256>
	using VectorConstIterator = typename Vector<T, SegmentSize>::const_iterator; ///< Immutable random-access iterator alias.

	template <typename T, std::size_t SegmentSize = 256>
	using VectorConstRange = std::ranges::subrange<VectorConstIterator<T, SegmentSize>>; ///< Read-only vector range alias.

	/// @brief Returns a read-only iterator range over the logical vector contents.
	template <typename T, std::size_t SegmentSize = 256>
	[[nodiscard]] VectorConstRange<T, SegmentSize> makeRange(const Vector<T, SegmentSize> &values) {
		return VectorConstRange<T, SegmentSize>(values.cbegin(), values.cend());
	}

} // namespace vve::simple
