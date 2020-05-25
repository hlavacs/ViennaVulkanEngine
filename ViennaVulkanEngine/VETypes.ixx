export module VVE:VeTypes;

import std.core;

export namespace vve {

	typedef int GUID1;

	//----------------------------------------------------------------------------------
	//lifted from Boost, to convert integers into strong types

	namespace detail {
		template <typename T> class empty_base {};
	}

	template <class T, class U, class B = detail::empty_base<T> >
	struct less_than_comparable2 : B
	{
		friend bool operator<=(const T& x, const U& y) { return !(x > y); }
		friend bool operator>=(const T& x, const U& y) { return !(x < y); }
		friend bool operator>(const U& x, const T& y) { return y < x; }
		friend bool operator<(const U& x, const T& y) { return y > x; }
		friend bool operator<=(const U& x, const T& y) { return !(y < x); }
		friend bool operator>=(const U& x, const T& y) { return !(y > x); }
	};

	template <class T, class B = detail::empty_base<T> >
	struct less_than_comparable1 : B
	{
		friend bool operator>(const T& x, const T& y) { return y < x; }
		friend bool operator<=(const T& x, const T& y) { return !(y < x); }
		friend bool operator>=(const T& x, const T& y) { return !(x < y); }
	};

	template <class T, class U, class B = detail::empty_base<T> >
	struct equality_comparable2 : B
	{
		friend bool operator==(const U& y, const T& x) { return x == y; }
		friend bool operator!=(const U& y, const T& x) { return !(x == y); }
		friend bool operator!=(const T& y, const U& x) { return !(y == x); }
	};

	template <class T, class B = detail::empty_base<T> >
	struct equality_comparable1 : B {
		friend bool operator!=(const T& x, const T& y) { return !(x == y); }
	};

	template <class T, class U, class B = detail::empty_base<T> >
	struct totally_ordered2 : less_than_comparable2<T, U, equality_comparable2<T, U, B> > {};

	template <class T, class B = detail::empty_base<T> >
	struct totally_ordered1 : less_than_comparable1<T, equality_comparable1<T, B> > {};


	#define SAFE_TYPEDEF(T, D)												\
	struct D : totally_ordered1< D, totally_ordered2< D, T > >				\
	{																		\
		static D NULL() { return D(std::numeric_limits<T>::max()); };		\
		T t;																\
		explicit D(const T& t_) : t(t_) {};									\
	    explicit D(T&& t_) : t(std::move(t_)) {};							\
	    D() { t = NULL();};													\
	    D(const D & t_) = default;											\
	    D(D&&) = default;													\
	    D& operator=(const D & rhs) = default;								\
	    D& operator=(D&&) = default;										\
	    operator T& () { return t; }										\
	    bool operator==(const D & rhs) const { return t == rhs.t; }			\
	    bool operator<(const D & rhs) const { return t < rhs.t; }			\
	};

	//----------------------------------------------------------------------------------
	//basic data types

	SAFE_TYPEDEF(uint16_t, VeIndex16);
	SAFE_TYPEDEF(uint32_t, VeIndex32);

	SAFE_TYPEDEF(uint16_t, VeNextIndex16);
	SAFE_TYPEDEF(uint32_t, VeNextIndex32);

	SAFE_TYPEDEF(uint16_t, VeChunkIndex16);
	SAFE_TYPEDEF(uint32_t, VeChunkIndex32);

	SAFE_TYPEDEF(uint16_t, VeInChunkIndex16);
	SAFE_TYPEDEF(uint32_t, VeInChunkIndex32);

	SAFE_TYPEDEF(uint32_t, VeGuid32);
	SAFE_TYPEDEF(uint64_t, VeGuid64);


	//----------------------------------------------------------------------------------
	//define the size of GUIDs, the other data structures follow

	using VeGuid = VeGuid32;

	//----------------------------------------------------------------------------------
	//packing 2 integers into a larger unsigned integer

	using VePackingType = typename std::conditional < std::is_same<VeGuid, VeGuid32>::value, uint32_t, uint64_t >::type;
	using VeChunkIndexType = typename std::conditional < std::is_same<VeGuid, VeGuid32>::value, VeChunkIndex16, VeChunkIndex32 >::type;
	using VeInChunkIndexType = typename std::conditional < std::is_same<VeGuid, VeGuid32>::value, VeInChunkIndex16, VeInChunkIndex32 >::type;
	using VeNextIndexType = typename std::conditional < std::is_same<VeGuid, VeGuid32>::value, VeNextIndex16, VeNextIndex32 >::type;

	template<typename T>
	concept Packable = std::is_same<T, uint32_t>::value || std::is_same<T, uint64_t>::value;

	template<Packable PackingType>
	struct VePackedInt {

		using T = typename std::conditional < std::is_same<PackingType, uint32_t>::value, uint16_t, uint32_t >::type;
		static const uint32_t bits			= std::is_same<PackingType, uint32_t>::value ? 16 : 32;
		static const uint32_t upper_bits	= std::is_same<PackingType, uint32_t>::value ? 0xFF00 : 0xFFFF0000;
		static const uint32_t lower_bits	= std::is_same<PackingType, uint32_t>::value ? 0x00FF : 0x0000FFFF;

		PackingType d_int;

		VePackedInt() : d_int() {};
		auto getUpper() { return T((d_int & upper_bits) >> bits); };
		void setUpper(T value) { d_int = PackingType((d_int & lower_bits) | (((PackingType)value) << bits)); };
		auto getLower() { return T(d_int & lower_bits); };
		void setLower(T value) { d_int = PackingType((d_int & upper_bits) | value ); };
	};


	//----------------------------------------------------------------------------------
	//a table index consists of a chunk index and an in-chunk index, both packed into one integer

	struct VeTableIndex {
		VePackedInt<VePackingType>	d_chunk_and_in_chunk_idx;

		VeTableIndex() : d_chunk_and_in_chunk_idx() { setChunkIndex(VeChunkIndexType::NULL()); setInChunkIndex(VeInChunkIndexType::NULL()); };
		VeTableIndex(VeTableIndex& table_index) = default;

		VeChunkIndexType	getChunkIndex() { return VeChunkIndexType(d_chunk_and_in_chunk_idx.getUpper()); };
		void				setChunkIndex( VeChunkIndexType idx) { d_chunk_and_in_chunk_idx.setUpper(idx); };
		VeInChunkIndexType	getInChunkIndex() { return VeInChunkIndexType(d_chunk_and_in_chunk_idx.getLower()); };
		void				setInChunkIndex( VeInChunkIndexType idx) { d_chunk_and_in_chunk_idx.setLower(idx); };
	};




	//----------------------------------------------------------------------------------
	//a handle consists of a GUID and an table index 

	//template<typename GuidType>
	struct VeHandle {
		VeTableIndex	d_table_index;
		VeGuid			d_guid;

		VeHandle() = default;

		explicit VeHandle(VeTableIndex table_index, VeGuid guid) : d_table_index(table_index), d_guid(guid) {};

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

};




