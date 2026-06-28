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
	* - VectorConstRange names read-only traversal without exposing storage details.
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

} // namespace vve::simple
