export module VVE:VeTypes;

import std.core;

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
		auto operator==(const VeHandle& v) { return d_guid.value == v.d_guid.value; };
		auto operator!=(const VeHandle& v) { return !(d_guid.value == v.d_guid.value); };
	};

	//----------------------------------------------------------------------------------
	//hashing for tuples of hashable types

	template<typename T>
	concept Hashable = requires(T a) {
		{ std::hash<T>{}(a) }->std::convertible_to<std::size_t>;
	};

	template <Hashable T>
	inline auto hash_combine(std::size_t& seed, T v) {
		seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		return seed;
	}

	template<typename T, std::size_t... Is>
	auto hash_impl(T const& t, std::index_sequence<Is...> const&) {
		size_t seed = 0;
		(hash_combine(seed, std::get<Is>(t)) + ... + 0);
		return seed;
	}

	template<typename... Args>
	std::size_t hash(std::tuple<Args...> const& value) {
		return hash_impl(value, std::make_index_sequence<sizeof...(Args)>());
	}


	//----------------------------------------------------------------------------------
	// A template to hold a parameter pack

	template < typename... >
	struct Typelist {};

};
