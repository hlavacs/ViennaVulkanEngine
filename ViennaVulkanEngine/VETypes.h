#ifndef VETYPES_H
#define VETYPES_H


/**
*
* \file
* \brief All includes of external sources are funneled through this include file.
*
* This is the basic include file of all engine parts. It includes external include files, 
* defines all basic data types, and defines operators for output and hashing for them.
*
*/



namespace vve {

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


#define SAFE_TYPEDEF(T, D)                                      \
	struct D : totally_ordered1< D, totally_ordered2< D, T > >      \
	{                                                               \
		T t;                                                        \
		explicit D(const T& t_) : t(t_) {};                         \
	    explicit D(T&& t_) : t(std::move(t_)) {};                   \
	    D() = default;                                              \
	    D(const D & t_) = default;                                  \
	    D(D&&) = default;                                           \
	    D& operator=(const D & rhs) = default;                      \
	    D& operator=(D&&) = default;                                \
	    operator T& () { return t; }                                \
	    bool operator==(const D & rhs) const { return t == rhs.t; } \
	    bool operator<(const D & rhs) const { return t < rhs.t; }   \
	};

	//----------------------------------------------------------------------------------
	//basic data types

	SAFE_TYPEDEF(uint16_t, VeIndex16);
	SAFE_TYPEDEF(uint32_t, VeIndex32);

	SAFE_TYPEDEF(uint16_t, VeChunkIndex16);
	SAFE_TYPEDEF(uint32_t, VeChunkIndex32);

	SAFE_TYPEDEF(uint16_t, VeTableIndex16);
	SAFE_TYPEDEF(uint32_t, VeTableIndex32);

	SAFE_TYPEDEF(uint32_t, VeGuid32);
	SAFE_TYPEDEF(uint64_t, VeGuid64);

	SAFE_TYPEDEF(uint32_t, VePackedIntegers32);
	SAFE_TYPEDEF(uint64_t, VePackedIntegers64);


	template<typename PackedType>
	struct VePackedInt {

		using T = typename std::conditional < std::is_same<PackedType, uint32_t>::value, uint16_t, uint32_t >::type;
		static const uint32_t bits			= std::is_same<PackedType, uint32_t>::value ? 16 : 32;
		static const uint32_t upper_bits	= std::is_same<PackedType, uint32_t>::value ? 0xFF00 : 0xFFFF0000;
		static const uint32_t lower_bits	= std::is_same<PackedType, uint32_t>::value ? 0x00FF : 0x0000FFFF;

		PackedType d_int;

		VePackedInt() : d_int(0) {};

		auto getUpper() {
			return T((d_int & upper_bits) >> bits);
		};

		void setUpper(T value) {
			d_int = Pack((d_int & lower_bits) | (value << bits));
		};

		auto getLower() {
			return T( d_int & lower_bits );
		};

		void setLower(T value) {
			d_int = Pack((d_int & upper_bits) | value );
		};
	};


	template<typename GuidType>
	struct VeHandle {
		using PackedType = typename std::conditional < std::is_same<GuidType, VeGuid32>::value, uint32_t, uint64_t >::type;
		using ChunkIndexType = typename std::conditional < std::is_same<GuidType, VeGuid32>::value, VeChunkIndex16, VeChunkIndex32 >::type;
		using TableIndexType = typename std::conditional < std::is_same<GuidType, VeGuid32>::value, VeChunkIndex16, VeChunkIndex32 >::type;

		VePackedInt<PackedType>	d_chunk_and_table_idx;
		GuidType				d_guid;

		VeHandle() : d_chunk_and_table_idx(), d_guid() {};

		explicit VeHandle(ChunkIndexType chunk_index, TableIndexType table_index, GuidType guid) : d_chunk_and_table_idx(0), d_guid(guid) {
			setChunkIdx(chunk_index);
			setTableIdx(table_index);
		};

		auto	getChunkIdx() { return ChunkIndexType(d_chunk_and_table_idx.getUpper()); };
		void	setChunkIdx(ChunkIndexType idx) { d_chunk_and_table_idx.setUpper(idx); };

		auto	getTableIdx() { return TableIndexType(d_chunk_and_table_idx.getLower()); };
		void	setTableIdx(TableIndexType idx) { d_chunk_and_table_idx.setLower(idx); };

		auto	getGuid() { return d_guid; };
		void	setGuid(GuidType guid) { d_guid = guid; };
	};
	   
	//----------------------------------------------------------------------------------
	//hashing for tuples


	template <class T>
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



#endif


