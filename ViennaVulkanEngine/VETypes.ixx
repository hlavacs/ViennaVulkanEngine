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
	    D() = default;														\
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

	SAFE_TYPEDEF(uint16_t, VeChunkIndex16);
	SAFE_TYPEDEF(uint32_t, VeChunkIndex32);

	SAFE_TYPEDEF(uint16_t, VeInChunkIndex16);
	SAFE_TYPEDEF(uint32_t, VeInChunkIndex32);

	SAFE_TYPEDEF(uint32_t, VeGuid32);
	SAFE_TYPEDEF(uint64_t, VeGuid64);

	SAFE_TYPEDEF(uint32_t, VePackedIntegers32);
	SAFE_TYPEDEF(uint64_t, VePackedIntegers64);


	//----------------------------------------------------------------------------------
	//packing 2 integers into a larger unsigned integer

	template<typename T>
	concept Packable = std::is_same<T, uint32_t>::value || std::is_same<T, uint64_t>::value;

	template<Packable PackedType>
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
			d_int = PackedType((d_int & lower_bits) | (value << bits));
		};

		auto getLower() {
			return T(d_int & lower_bits);
		};

		void setLower(T value) {
			d_int = PackedType((d_int & upper_bits) | value );
		};
	};


	//----------------------------------------------------------------------------------
	//a table index consists of a chunk index and an in-chunk index, both packed into one integer

	template<typename GuidType>
	struct VeTableIndex {
		using PackedType = typename std::conditional < std::is_same<GuidType, VeGuid32>::value, uint32_t, uint64_t >::type;
		using ChunkIndexType = typename std::conditional < std::is_same<GuidType, VeGuid32>::value, VeChunkIndex16, VeChunkIndex32 >::type;
		using InChunkIndexType = typename std::conditional < std::is_same<GuidType, VeGuid32>::value, VeInChunkIndex16, VeInChunkIndex32 >::type;

		VePackedInt<PackedType>	d_chunk_and_in_chunk_idx;

		VeTableIndex() : d_chunk_and_in_chunk_idx() {
			setChunkIdx(ChunkIndexType::NULL());
			setInChunkIdx(InChunkIndexType::NULL());
		};

		VeTableIndex(ChunkIndexType chunk_index, InChunkIndexType in_chunk_index) {
			setChunkIdx(chunk_index);
			setInChunkIdx(in_chunk_index);
		};


		ChunkIndexType		getChunkIdx() { return ChunkIndexType(d_chunk_and_in_chunk_idx.getUpper()); };
		void				setChunkIdx(ChunkIndexType idx) { d_chunk_and_in_chunk_idx.setUpper(idx); };

		InChunkIndexType	getInChunkIdx() { return InChunkIndexType(d_chunk_and_in_chunk_idx.getLower()); };
		void				setInChunkIdx(InChunkIndexType idx) { d_chunk_and_in_chunk_idx.setLower(idx); };
	};


	//----------------------------------------------------------------------------------
	//a handle consists of a GUID and an table index 

	template<typename GuidType>
	struct VeHandle {
		using PackedType		= typename std::conditional < std::is_same<GuidType, VeGuid32>::value, uint32_t, uint64_t >::type;
		using ChunkIndexType	= typename std::conditional < std::is_same<GuidType, VeGuid32>::value, VeChunkIndex16, VeChunkIndex32 >::type;
		using InChunkIndexType	= typename std::conditional < std::is_same<GuidType, VeGuid32>::value, VeInChunkIndex16, VeInChunkIndex32 >::type;

		VeTableIndex<GuidType>	d_table_idx;
		GuidType				d_guid;

		VeHandle() {};

		explicit VeHandle(ChunkIndexType chunk_index, InChunkIndexType in_chunk_index, GuidType guid) : d_table_idx(0), d_guid(guid) {
			setChunkIdx(chunk_index);
			setInChunkIdx(in_chunk_index);
		};

		ChunkIndexType	getChunkIdx() { return d_table_idx.getChunkIdx(); };
		void			setChunkIdx(ChunkIndexType idx) { d_table_idx.setChunkIdx(idx); };

		InChunkIndexType	getInChunkIdx() { return d_table_idx.getInChunkIdx(); };
		void				setInChunkIdx(InChunkIndexType idx) { d_table_idx.setInChunkIdx(idx); };

		auto	getGuid() { return d_guid; };
		void	setGuid(GuidType guid) { d_guid = guid; };
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




