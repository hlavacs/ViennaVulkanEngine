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

	SAFE_TYPEDEF(std::uint16_t, VeIndex16);
	SAFE_TYPEDEF(std::uint32_t, VeIndex32);
	SAFE_TYPEDEF(std::uint64_t, VeIndex64);
	SAFE_TYPEDEF(std::uint32_t, VeGuid32);
	SAFE_TYPEDEF(std::uint64_t, VeGuid64);

	using VeHandle64_t = std::tuple<VeIndex16, VeIndex16, VeGuid32>;
	SAFE_TYPEDEF(VeHandle64_t, VeHandle64);

	   
	//----------------------------------------------------------------------------------
	//hashing for tuples

	class hash_tuple {
		template<class T>
		struct component {
			const T& value;
			component(const T& value) : value(value) {}
			uintmax_t operator,(uintmax_t n) const {
				n ^= std::hash<T>()(value);
				n ^= n << (sizeof(uintmax_t) * 4 - 1);
				return n ^ std::hash<uintmax_t>()(n);
			}
		};

	public:
		template<class Tuple>
		size_t operator()(const Tuple& tuple) const {
			return std::hash<uintmax_t>()(
				std::apply([](const auto& ... xs) { return (component(xs), ..., 0); }, tuple)
			);
		}
	};

};


#endif


