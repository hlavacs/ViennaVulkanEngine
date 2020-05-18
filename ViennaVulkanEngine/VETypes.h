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
	struct equality_comparable1 : B
	{
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

	SAFE_TYPEDEF(std::uint16_t, VeIndex16);
	SAFE_TYPEDEF(std::uint32_t, VeIndex32);
	SAFE_TYPEDEF(std::uint64_t, VeIndex64);
	SAFE_TYPEDEF(std::uint32_t, VeGuid32);
	SAFE_TYPEDEF(std::uint64_t, VeGuid64);

	using VeHandle64_t = std::tuple<VeIndex16, VeIndex16, VeGuid32>;
	SAFE_TYPEDEF(VeHandle64_t, VeHandle64);

	   
	//----------------------------------------------------------------------------------
	//hashing specialization for tuples

	///combining two hashes
	template <class T>
	inline void hash_combine(std::size_t& seed, T const& v) {
		seed ^= hash_tuple::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	};

	///recursive tuple definition - combine Nth with N-1st element
	template <class Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
	struct HashValueImpl {
		static void apply(size_t& seed, Tuple const& tuple) {
			HashValueImpl<Tuple, Index - 1>::apply(seed, tuple);
			hash_combine(seed, std::get<Index>(tuple));
		}
	};

	template <class Tuple>
	struct HashValueImpl<Tuple, 0> {
		static void apply(size_t& seed, Tuple const& tuple) {
			hash_combine(seed, std::get<0>(tuple));
		}
	};

	template <typename ... TT>
	struct std::hash<std::tuple<TT...>> {
		size_t operator()(std::tuple<TT...> const& tt) const {
			size_t seed = 0;
			HashValueImpl<std::tuple<TT...> >::apply(seed, tt);
			return seed;
		};
	};

};


#endif


