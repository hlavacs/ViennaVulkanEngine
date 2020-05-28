export module VVE:VeTypes;

import std.core;

export namespace vve {

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
		static D NULL() {													\
			if constexpr (std::is_integral_v<T>) {							\
				return D(std::numeric_limits<T>::max());						\
			}																\
			else {															\
				return D();													\
			}																\
		};																	\
		T value;															\
		explicit D(const T& t_) : value(t_) {};								\
	    explicit D(T&& t_) : value(std::move(t_)) {};						\
	    D() { value = NULL(); };											\
	    D(const D & t_) = default;											\
	    D(D&&) = default;													\
	    D& operator=(const D & rhs) = default;								\
	    D& operator=(D&&) = default;										\
	    operator T& () { return value; }									\
	    bool operator==(const D & rhs) const { return value == rhs.value; }	\
	    bool operator<(const D & rhs) const { return value < rhs.value; }	\
	}; 
	//template<typename D>					\
	//std::size_t hash(D const& val) {		\
	//	return std::hash(val.value);		\
	//};


	//----------------------------------------------------------------------------------
	//basic data types

	SAFE_TYPEDEF(uint32_t, VeIndex32);
	SAFE_TYPEDEF(uint64_t, VeIndex64);

	SAFE_TYPEDEF(uint16_t, VeChunkIndex16);
	SAFE_TYPEDEF(uint32_t, VeChunkIndex32);

	SAFE_TYPEDEF(uint16_t, VeInChunkIndex16);
	SAFE_TYPEDEF(uint32_t, VeInChunkIndex32);

	SAFE_TYPEDEF(uint32_t, VeGuid32);
	SAFE_TYPEDEF(uint64_t, VeGuid64);

	//----------------------------------------------------------------------------------
	//define the size of GUIDs - either 32 or 64

	using VeGuid = VeGuid32; //or VeGuid64

	//----------------------------------------------------------------------------------
	//the other data structures follow

	using VeIndex = typename std::conditional < std::is_same_v<VeGuid, VeGuid32>, VeIndex32, VeIndex64 >::type;
	using VeChunkIndex = typename std::conditional < std::is_same_v<VeGuid, VeGuid32>, VeChunkIndex16, VeChunkIndex32 >::type;
	using VeInChunkIndex = typename std::conditional < std::is_same_v<VeGuid, VeGuid32>, VeInChunkIndex16, VeInChunkIndex32 >::type;

	//----------------------------------------------------------------------------------
	//a table index consists of a chunk index and an in-chunk index, both packed into one integer

	struct VeTableIndex {
		VeChunkIndex	d_chunk_index;
		VeInChunkIndex	d_in_chunk_index;
	};

	//----------------------------------------------------------------------------------
	//a handle consists of a GUID and an table index

	struct VeHandle {
		VeGuid	d_guid;
		VeIndex	d_table_index;
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
