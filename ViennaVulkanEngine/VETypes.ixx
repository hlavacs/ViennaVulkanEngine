export module VVE:VeTypes;

import std.core;
import :VeUtil;


export namespace vve {


	//----------------------------------------------------------------------------------
	//basic data types

	template<typename T, typename Phantom>
	struct IntType {
		static T NULL() { return T(std::numeric_limits<T>::max()); };
		T value;
		operator T& () { return value; }
		IntType() : value(IntType::NULL()) {};
		IntType(const T& t) : value(t) {};
		auto operator<=>(const IntType& v) const = default;
		auto operator<=>(const T& v) { return value <=> v; };
	};

	struct InType_hash {
		template<typename T, typename P>
		std::size_t operator()(const IntType<T, P> & v) const {
			return std::hash<decltype(v.value)>()(v.value);
		}
	};

	//----------------------------------------------------------------------------------
	//define the size of GUIDs - either 32 or 64

	struct P0 {};
	struct P1 {};
	struct P2 {};
	struct P3 {};

	using VeGuid = IntType<uint32_t, P0>;
	using VeIndex = IntType<uint32_t, P1>;
	using VeChunkIndex = IntType<uint16_t, P2>;
	using VeInChunkIndex = IntType<uint16_t, P3>;

	/*using VeGuid = IntType<uint64_t, P0>;
	using VeIndex = IntType<uint32_t, P1>;
	using VeChunkIndex = IntType<uint32_t, P2>;
	using VeInChunkIndex = IntType<uint32_t, P3>;*/

	//----------------------------------------------------------------------------------
	//a table index consists of a chunk index and an in-chunk index, both packed into one integer

	struct VeTableIndex {
		static VeTableIndex NULL() { return VeTableIndex(); }
		VeChunkIndex	d_chunk_index;
		VeInChunkIndex	d_in_chunk_index;
		VeTableIndex() : d_chunk_index(VeChunkIndex::NULL()), d_in_chunk_index(VeInChunkIndex::NULL()) {};
		auto operator==(const VeTableIndex& v) { return d_chunk_index == v.d_chunk_index && d_in_chunk_index == v.d_in_chunk_index; };
		auto operator!=(const VeTableIndex& v) { return !(d_chunk_index == v.d_chunk_index && d_in_chunk_index == v.d_in_chunk_index); };
	};


	//----------------------------------------------------------------------------------
	//a handle consists of a GUID and an table index

	struct VeHandle {
		static VeHandle NULL() {return VeHandle(); }
		VeGuid	d_guid;
		VeIndex	d_index;
		VeHandle() : d_guid(VeGuid::NULL()), d_index(VeIndex::NULL()) {};
		VeHandle(const VeHandle& v) : d_guid(v.d_guid), d_index(v.d_index) {};
		bool operator==(const VeHandle& v) const { return d_guid.value == v.d_guid.value; };
		bool operator!=(const VeHandle& v) const { return !(d_guid.value == v.d_guid.value); };
	};

	struct VeHandle_hash {
		std::size_t operator()(const VeHandle& v) const {
			return std::hash<decltype(v.d_guid.value)>()(v.d_guid.value);
		}
	};

	//----------------------------------------------------------------------------------
	// A template to hold a parameter pack

	template < typename... >
	struct Typelist {};

	template < int... >
	struct Intlist {};

};


namespace std {

	template<>
	struct hash<vve::VeHandle> {
		size_t operator()(const vve::VeHandle& v) const {
			return hash<decltype(v.d_guid.value)>()(v.d_guid.value);
		}
	};

}

