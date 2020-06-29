export module VVE:VETypes;

import std.core;

export namespace vve {



	//----------------------------------------------------------------------------------
	//basic data types

	template<typename T, typename P>
	struct NumType {
		static NumType<T, P> NULL() { return NumType<T,P>(); };
		T value{};
		operator T& () { return value; }
		NumType() : value(T(std::numeric_limits<T>::max())) {};
		NumType(const T& t) : value(t) {};
		NumType(const NumType<T,P>& t) : value(t.value) {};
		auto operator<=>(const NumType& v) const = default;
		auto operator<=>(const T& v) { return value <=> v; };
	};


	//----------------------------------------------------------------------------------
	//define the size of GUIDs - either 32 or 64

	using VeGuid = NumType<uint32_t, struct P0>;
	using VeIndex = NumType<uint32_t, struct P1>;
	using VeChunkIndex = NumType<uint16_t, struct P2>;
	using VeInChunkIndex = NumType<uint16_t, struct P3>;
	using VeHash = NumType<uint64_t, struct P4>;


	/*
	using VeGuid = IntType<uint64_t, P0>;
	using VeIndex = IntType<uint64_t, P1>;
	using VeChunkIndex = IntType<uint32_t, P2>;
	using VeInChunkIndex = IntType<uint32_t, P3>;
	*/

	//----------------------------------------------------------------------------------
	//a table index consists of a chunk index and an in-chunk index, both packed into one integer

	struct VeTableIndex {
		static VeTableIndex NULL() { return VeTableIndex(); }
		VeChunkIndex	d_chunk_index;
		VeInChunkIndex	d_in_chunk_index;
		VeTableIndex() : d_chunk_index(VeChunkIndex::NULL()), d_in_chunk_index(VeInChunkIndex::NULL()) {};
		VeTableIndex(VeChunkIndex chunk_index, VeInChunkIndex in_chunk_index) : d_chunk_index(chunk_index), d_in_chunk_index(in_chunk_index) {};
		VeTableIndex(const VeTableIndex &v) : d_chunk_index(v.d_chunk_index), d_in_chunk_index(v.d_in_chunk_index) {};
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
		VeHandle(VeGuid guid, VeIndex index) : d_guid(guid), d_index(index) {};
		VeHandle(const VeHandle& v) : d_guid(v.d_guid), d_index(v.d_index) {};
		bool operator==(const VeHandle& v) const { return d_guid.value == v.d_guid.value; };
		bool operator!=(const VeHandle& v) const { return !(d_guid.value == v.d_guid.value); };
	};

	//----------------------------------------------------------------------------------
	// A template to hold a parameter pack

	template < typename... Ts>
	struct Typelist {};

	template < int... Is >
	struct Intlist {
		auto getTuple() {
			return std::make_tuple(Is...);
		}

		template<typename U>
		auto getInstance() {
		}
	};



};






