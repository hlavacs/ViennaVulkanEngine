export module VEEngine.Simple.Vector;
import std;
import VEEngine.Vector;

/**
	* @file
	* @brief Simple engine aliases for the facade-owned vector vocabulary.
	*/
export namespace vve::simple {

	template <typename T, std::size_t SegmentSize = 256>
	using Vector = vve::Vector<T, SegmentSize>;										///< Simple engine dynamic array alias.

	template <typename T>
	using VectorConstRange = vve::VectorConstRange<T>;							///< Simple engine read-only vector range alias.

	using vve::makeRange;																		///< Simple engine range helper alias.

} // namespace vve::simple
