#ifndef VTLL_H
#define VTLL_H

#include <tuple>
#include <variant>
#include <iostream>
#include <limits>
#include <tuple>

/***********************************************************************************
* 
	Vienna Type List Library

	By Helmut Hlavacs, University of Vienna, 2021

	This library comes with no warranty and uses the MIT license

	A collection of C++ typelist algorithms.

	See the static_assert statements to understand what each function does.

	Inspired by Nils Deppe's post: https://nilsdeppe.com/posts/tmpl-part2

***********************************************************************************/

namespace vtll {


	//type_list: example for a struct that can act as type list. Any such list can be used.

	template <typename... Ts>
	struct type_list {
		using size = std::integral_constant<std::size_t, sizeof...(Ts)>;
	};

	template <>
	struct type_list<> {
		using size = std::integral_constant<std::size_t, 0>;
	};

	template<typename... Ts>
	using tl = type_list<Ts...>;

	template <size_t... Is>
	struct value_list {
		using size = std::integral_constant<std::size_t, sizeof...(Is)>;
	};

	template<size_t... Is>
	using vl = value_list<Is...>;

	namespace detail {
		template <typename... Ts>
		struct type_list2 {
			using size = std::integral_constant<std::size_t, sizeof...(Ts)>;
		};

		template <>
		struct type_list2<> {
			using size = std::integral_constant<std::size_t, 0>;
		};
	}

	//-------------------------------------------------------------------------
	//type_to_value: turn a list of std::integral_constant<> into a value list

	namespace detail {
		template<typename Seq>
		struct type_to_value_impl;

		template<template<typename...> typename Seq, typename... Ts>
		struct type_to_value_impl<Seq<Ts...>> {
			using type = value_list<Ts::value...>;
		};
	}

	template<typename Seq>
	using type_to_value = typename detail::type_to_value_impl<Seq>::type;

	static_assert(
		std::is_same_v<
			type_to_value<type_list< std::integral_constant<size_t, 2>, std::integral_constant<size_t, 4>, std::integral_constant<size_t, 6> > >
		, value_list<2, 4, 6>
		>,
		"The implementation of type_to_value is bad");

	//-------------------------------------------------------------------------
	//value_to_type: turn a value list into a list of std::integral_constant<>

	namespace detail {
		template<typename Seq>
		struct value_to_type_impl;

		template<template<size_t...> typename Seq, size_t... Is>
		struct value_to_type_impl<Seq<Is...>> {
			using type = type_list<std::integral_constant<size_t, Is>...>;
		};
	}

	template<typename Seq>
	using value_to_type = typename detail::value_to_type_impl<Seq>::type;

	static_assert(
		std::is_same_v<
		value_to_type< value_list<2, 4, 6> >
		, type_list< std::integral_constant<size_t, 2>, std::integral_constant<size_t, 4>, std::integral_constant<size_t, 6> >
		>,
		"The implementation of value_to_type is bad");


	//-------------------------------------------------------------------------
	//index_to_value_impl: Convert a std::index_sequence to a value_list

	namespace detail {
		template<typename T>
		struct index_to_value_impl;

		template<template<class Ty, Ty... Is> typename Seq, typename Ty, Ty... Is>
		struct index_to_value_impl<Seq<Ty, Is...>> {
			using type = value_list<Is...>;
		};
	}

	template<typename Seq>
	struct index_to_value {
		using type = typename detail::index_to_value_impl< Seq >::type;
	};

	static_assert( std::is_same_v< typename index_to_value<std::make_index_sequence<3> >::type , value_list<0,1,2>>
		, "The implementation of intseq_to_value is bad");


	//-------------------------------------------------------------------------
	//value_to_index: 

	namespace detail {
		template<typename T>
		struct value_to_index_impl;

		template<template<size_t... Is> typename Seq, size_t... Is>
		struct value_to_index_impl<Seq<Is...>> {
			using type = std::index_sequence<Is...>;
		};
	}

	template<typename Seq>
	struct value_to_index {
		using type = typename detail::value_to_index_impl< Seq >::type;
	};

	static_assert(std::is_same_v< typename value_to_index<value_list<0, 1, 2>>::type, std::make_index_sequence<3> >
		, "The implementation of value_to_intseq is bad");


	//-------------------------------------------------------------------------
	//type list algorithms

	//-------------------------------------------------------------------------
	//is_type_list: test if a template is a type list

	namespace detail {
		template <typename T>
		struct is_type_list_impl : std::false_type {};

		template <template <typename...> typename Seq, typename... Ts>
		struct is_type_list_impl<Seq<Ts...>> : std::true_type {};

		template <template <typename...> typename Seq>
		struct is_type_list_impl<Seq<>> : std::true_type {};
	}
	template <typename T>
	using is_type_list = detail::is_type_list_impl<T>;

	static_assert(!is_type_list<int>::value, "The implementation of is_type_list is bad");
	static_assert(is_type_list<type_list<double, char, bool, double>>::value, "The implementation of is_type_list is bad");
	static_assert(is_type_list<type_list<>>::value, "The implementation of is_type_list is bad");

	//-------------------------------------------------------------------------
	//size: size of a type list

	namespace detail {
		template <typename Seq>
		struct size_impl;

		template <template <typename...> typename Seq, typename... Ts>
		struct size_impl<Seq<Ts...>> {
			using type = std::integral_constant<std::size_t, sizeof...(Ts)>;
		};
	}
	template <typename Seq>
	using size = typename detail::size_impl<Seq>::type;

	static_assert( size<type_list<double, char, bool, double>>::value==4, "The implementation of size is bad");

	//-------------------------------------------------------------------------
	//Nth_type: get the Nth element from a type list

	namespace detail {
		template <typename Seq, size_t N>
		struct Nth_type_impl;

		template <template <typename...> typename Seq, typename... Ts>
		requires (sizeof...(Ts)>0)
		struct Nth_type_impl<Seq<Ts...>, std::numeric_limits<size_t>::max()> {
			using type = Seq<>;
		};

		template <int N, template <typename...> typename Seq>
		struct Nth_type_impl<Seq<>, N> {
			using type = Seq<>;
		};

		template <int N, template <typename...> typename Seq, typename... Ts>
		struct Nth_type_impl<Seq<Ts...>, N> {
			using type = typename std::tuple_element<N, std::tuple<Ts...>>::type;
		};
	}

	template <typename Seq, size_t N>
	using Nth_type = typename detail::Nth_type_impl<Seq, N>::type;

	static_assert(
		std::is_same_v<Nth_type<type_list<double, char, bool, double>, 1>, char>,
		"The implementation of Nth_type is bad");

	//-------------------------------------------------------------------------
	//front: first element of a type list

	template <typename Seq>
	using front = Nth_type<Seq,0>;

	static_assert(
		std::is_same< front<type_list<double, char, bool, float>>, double >::value,
		"The implementation of front is bad");

	static_assert(
		!std::is_same< front<type_list<int, char, bool, float>>, double >::value,
		"The implementation of front is bad");

	//-------------------------------------------------------------------------
	//back: get the last element from a list

	template <typename Seq>
	using back = Nth_type<Seq, std::integral_constant<std::size_t, size<Seq>::value - 1>::value >;

	static_assert(
		std::is_same<back<type_list<double, char, bool, float>>, float>::value,
		"The implementation of back is bad");

	static_assert(
		!std::is_same<back<type_list<double, char, bool, float>>, char>::value,
		"The implementation of back is bad");

	//-------------------------------------------------------------------------
	//index_of: index of first occurrence of a type within a type list

	namespace detail {
		template<typename, typename>
		struct index_of_impl;

		template <typename T, template <typename...> typename Seq>
		struct index_of_impl<Seq<>, T> : std::integral_constant<std::size_t, std::numeric_limits<size_t>::max()> {};

		template <typename T, template <typename...> typename Seq, typename... Ts>
		struct index_of_impl<Seq<T, Ts...>,T> : std::integral_constant<std::size_t, 0> {};

		template <typename T, typename TOther, template <typename...> typename Seq, typename... Ts>
		struct index_of_impl<Seq<TOther, Ts...>,T> : std::integral_constant<std::size_t, 1 + index_of_impl<Seq<Ts...>,T>::value> {};
	}

	template <typename Seq, typename T>
	using index_of = typename detail::index_of_impl<Seq,T>::type;

	static_assert(index_of< type_list<double, char, bool, double>, char >::value == 1);

	static_assert(
		std::is_same_v< index_of< type_list<double, char, bool, double>, char >, std::integral_constant<std::size_t, 1> >,
		"The implementation of index_of is bad");

	//-------------------------------------------------------------------------
	//cat: concatenate type lists to one big type list, the result is of the first list type

	namespace detail {
		template <typename... Seq>
		struct cat_impl;

		template <template <typename...> typename Seq1, template <typename...> typename Seq2>
		struct cat_impl<Seq1<>, Seq2<>> {
			using type = Seq1<>;
		};

		template <template <typename...> typename Seq1, template <typename...> typename Seq2, typename... Ts>
		struct cat_impl<Seq1<Ts...>, Seq2<>> {
			using type = Seq1<Ts...>;
		};

		template <template <typename...> typename Seq1, typename... Ts1, template <typename...> typename Seq2, typename T, typename... Ts2>
		struct cat_impl<Seq1<Ts1...>, Seq2<T, Ts2...>> {
			using type = typename cat_impl<Seq1<Ts1..., T>, Seq2<Ts2...>>::type;
		};

		template <typename Seq1, typename Seq2, typename... Seq>
		struct cat_impl<Seq1, Seq2, Seq...> {
			using type0 = typename cat_impl<Seq1, Seq2>::type;
			using type = typename cat_impl<type0, Seq...>::type;

		};
	}

	template <typename... Seq>
	using cat = typename detail::cat_impl<Seq...>::type;

	static_assert(
		std::is_same_v< cat< type_list<double, int>, detail::type_list2<char, float> >, type_list<double, int, char, float> >,
		"The implementation of cat is bad");

	static_assert(
		std::is_same_v< cat< type_list<double, int>, type_list<char, float>, type_list<int, float> >, type_list<double, int, char, float, int, float> >,
		"The implementation of cat is bad");

	//-------------------------------------------------------------------------
	//app: append a parameter pack to a type list

	template <typename Seq, typename... Ts>
	using app = cat< Seq, type_list<Ts>... >;

	static_assert(
		std::is_same_v< app< type_list<double, int>, char, float>, type_list<double, int, char, float> >,
		"The implementation of app is bad");

	static_assert(
		std::is_same_v< app< type_list<double, int, char, float>, int, float >, type_list<double, int, char, float, int, float> >,
		"The implementation of app is bad");

	//-------------------------------------------------------------------------
	//to_ref: turn list elements into references

	namespace detail {
		template <typename Seq>
		struct to_ref_impl;

		template <template <typename...> typename Seq, typename... Ts>
		struct to_ref_impl<Seq<Ts...>> {
			using type = Seq<Ts&...>;
		};
	}

	template <typename Seq>
	using to_ref = typename detail::to_ref_impl<Seq>::type;

	static_assert(
		std::is_same_v< to_ref< type_list<double, int> >, type_list<double&, int&> >,
		"The implementation of to_ref is bad");

	//-------------------------------------------------------------------------
	//to_ptr: turn list elements into pointers

	namespace detail {
		template <typename Seq>
		struct to_ptr_impl;

		template <template <typename...> typename Seq, typename... Ts>
		struct to_ptr_impl<Seq<Ts...>> {
			using type = Seq<Ts*...>;
		};
	}

	template <typename Seq>
	using to_ptr = typename detail::to_ptr_impl<Seq>::type;

	static_assert(
		std::is_same_v< to_ptr< type_list<double, int> >, type_list<double*, int*> >,
		"The implementation of to_ptr is bad");

	//-------------------------------------------------------------------------
	//to_variant: make a summary variant type of all elements in a list

	namespace detail {
		template <typename Seq>
		struct to_variant_impl;

		template <template <typename...> typename Seq, typename... Ts>
		struct to_variant_impl<Seq<Ts...>> {
			using type = std::variant<Ts...>;
		};
	}

	template <typename Seq>
	using to_variant = typename detail::to_variant_impl<Seq>::type;

	static_assert(
		std::is_same_v< to_variant< type_list<double, int, char> >, std::variant<double, int, char> >,
		"The implementation of to_variant is bad");

	//-------------------------------------------------------------------------
	//transform: transform list<types> into list<Function<types>>

	namespace detail {
		template<typename List, template<typename...> typename Fun>
		struct transform_impl;

		template<template <typename...> typename Seq, typename ...Ts, template<typename...> typename Fun>
		struct transform_impl<Seq<Ts...>, Fun> {
			using type = Seq<Fun<Ts>...>;
		};
	}
	template <typename Seq, template<typename...> typename Fun>
	using transform = typename detail::transform_impl<Seq, Fun>::type;

	static_assert(
		std::is_same_v< transform< type_list<double, int>, detail::type_list2 >, type_list<detail::type_list2<double>, detail::type_list2<int>> >,
		"The implementation of transform is bad");

	//-------------------------------------------------------------------------
	//transform_size_t: transform list<types> into list<Function<types,size_t>>

	namespace detail {
		template<typename List, template<typename, size_t> typename Fun, size_t N>
		struct transform_size_t_impl;

		template<template <typename...> typename Seq, typename... Ts, template<typename, size_t> typename Fun, size_t N>
		struct transform_size_t_impl<Seq<Ts...>, Fun, N> {
			using type = Seq<Fun<Ts, N>...>;
		};
	}
	template <typename Seq, template<typename, size_t> typename Fun, size_t N>
	using transform_size_t = typename detail::transform_size_t_impl<Seq, Fun, N>::type;

	static_assert(
		std::is_same_v< transform_size_t< type_list<double, int>, std::array, 10 >, type_list<std::array<double, 10>, std::array<int,10>> >,
		"The implementation of transform_size_t is bad");

	//-------------------------------------------------------------------------
	//transform_front: transform list<types> + T into list<Function<T,types>>

	namespace detail {
		template<typename List, template<typename...> typename Fun, typename T>
		struct transform_front_impl;

		template<template <typename...> typename Seq, typename... Ts, template<typename...> typename Fun, typename T>
		struct transform_front_impl<Seq<Ts...>, Fun, T> {
			using type = Seq<Fun<T, Ts>...>;
		};
	}
	template <typename Seq, template<typename...> typename Fun, typename T>
	using transform_front = typename detail::transform_front_impl<Seq, Fun, T>::type;

	static_assert(
		std::is_same_v< 
			  transform_front< type_list< type_list<double, int>, type_list<float, char>, type_list<int, float> >, cat, type_list<char> >
			, type_list< type_list<char, double, int>, type_list<char, float, char>, type_list<char, int, float> >
		> 
		,	"The implementation of transform_front is bad");

	//-------------------------------------------------------------------------
	//transform_back: transform list<types> + T into list<Function<types,T>>

	namespace detail {
		template<typename List, template<typename...> typename Fun, typename T>
		struct transform_back_impl;

		template<template <typename...> typename Seq, typename... Ts, template<typename...> typename Fun, typename T>
		struct transform_back_impl<Seq<Ts...>, Fun, T> {
			using type = Seq<Fun<Ts,T>...>;
		};
	}
	template <typename Seq, template<typename...> typename Fun, typename T>
	using transform_back = typename detail::transform_back_impl<Seq, Fun, T>::type;

	static_assert(
		std::is_same_v<
		transform_back< type_list< type_list<double, int>, type_list<float, char>, type_list<int, float> >, cat, type_list<char> >
		, type_list< type_list<double, int, char>, type_list<float, char, char>, type_list<int, float, char> >
		>
		, "The implementation of transform_back is bad");

	//-------------------------------------------------------------------------
	//substitute: substitute a type list TYPE with another list type

	namespace detail {
		template<typename List, template<typename...> typename Fun>
		struct substitute_impl;

		template<template <typename...> typename Seq, typename... Ts, template<typename...> typename Fun>
		struct substitute_impl<Seq<Ts...>, Fun> {
			using type = Fun<Ts...>;
		};
	}
	template <typename Seq, template<typename...> typename Fun>
	using substitute = typename detail::substitute_impl<Seq, Fun>::type;

	static_assert(
		std::is_same_v< substitute< type_list<double, int, char>, detail::type_list2 >, detail::type_list2<double, int, char> >,
		"The implementation of substitute is bad");

	//-------------------------------------------------------------------------
	//transfer: transfer a list of types1 into a list of types2

	namespace detail {
		template<typename List, template<typename...> typename Fun>
		struct transfer_impl;

		template<template <typename...> typename Seq, template<typename...> typename Fun>
		struct transfer_impl<Seq<>, Fun> {
			using type = Seq<>;
		};

		template<template <typename...> typename Seq, typename T, typename... Ts, template<typename...> typename Fun>
		struct transfer_impl<Seq<T, Ts...>, Fun> {
			using type = cat< Seq< substitute<T, Fun> >, typename transfer_impl< Seq<Ts...>, Fun>::type >;
		};
	}
	template <typename Seq, template<typename...> typename Fun>
	using transfer = typename detail::transfer_impl<Seq, Fun>::type;

	static_assert(
		std::is_same_v< transfer<type_list<type_list<double, int>>, detail::type_list2 >, type_list<detail::type_list2<double, int>> >,
		"The implementation of transfer is bad");

	//-------------------------------------------------------------------------
	//is_same: test if a list contains the same types as types of a variadic parameter pack

	namespace detail {
		template<typename Seq, typename... Args>
		struct is_same_impl {
			static const bool value = false;
		};

		template<template <typename...> class Seq, typename... Ts, typename... Args>
		struct is_same_impl<Seq<Ts...>, Args...> {
			static const bool value = (sizeof...(Ts) == sizeof...(Args) && (std::is_same_v<std::decay_t<Ts>, std::decay_t<Args>> && ... && true));
		};
	}
	template <typename Seq, typename... Args>
	struct is_same {
		static const bool value = detail::is_same_impl<Seq, Args...>::value;
	};

	static_assert( is_same<type_list<double, int>, double, int>::value, "The implementation of is_same is bad");

	//-------------------------------------------------------------------------
	//all_pow2: return a list of all possible powers of 2 with 64 bits

	namespace detail {
		template<typename T>
		struct all_pow2_impl;

		template<template<class Ty, Ty... Is> typename T, typename Ty, Ty... Is>
		struct all_pow2_impl<T<Ty, Is...>> {
			using type = type_list< std::integral_constant<size_t, (1ULL << Is)>... >;
		};
	}

	using all_pow2 = typename detail::all_pow2_impl< std::make_index_sequence<64> >::type;

	//-------------------------------------------------------------------------
	//has_type: check whether a type list contains a type

	namespace detail {
		template<typename Seq, typename T>
		struct has_type_impl;

		template<template <typename...> typename Seq, typename T>
		struct has_type_impl<Seq<>, T> {
			static const bool value = false;
		};

		template<template <typename...> typename Seq, typename... Ts, typename T>
		struct has_type_impl<Seq<Ts...>, T> {
			static const bool value = (std::is_same_v<T, Ts> || ... );
		};
	}
	template <typename Seq, typename T>
	struct has_type {
		static const bool value = detail::has_type_impl<Seq, T>::value;
	};

	static_assert(has_type<type_list<double, int, char, double>, char>::value, "The implementation of has_type is bad");
	static_assert(!has_type<type_list<double, int, char, double>, float>::value, "The implementation of has_type is bad");

	//-------------------------------------------------------------------------
	//have_type: check whether all elements of a list of lists contain an element

	namespace detail {
		template<typename Seq, typename T>
		struct have_type_impl;

		template<template <typename...> typename Seq, typename... Ts, typename T>
		struct have_type_impl<Seq<Ts...>, T> {
			static const bool value = (has_type<Ts, T>::value && ...);
		};
	}
	template <typename Seq, typename T>
	struct have_type {
		static const bool value = detail::have_type_impl<Seq, T>::value;
	};

	static_assert(have_type<  type_list< type_list<char, double>, type_list<float, char, int> >, char>::value, "The implementation of have_type is bad");

	static_assert(!have_type< type_list< type_list<double, int>, type_list<int> >, float>::value, "The implementation of have_type is bad");

	//-------------------------------------------------------------------------
	//is_pow2: test whether a std::integral_constant<size_t, I> is a power of 2

	template <typename T>
	constexpr auto is_pow2() {
		return has_type<all_pow2, T>::value;
	}

	static_assert(is_pow2<std::integral_constant<size_t, 128>>(), "The implementation of is_pow2 is bad");
	static_assert(is_pow2<std::integral_constant<size_t, 1 << 20>>(), "The implementation of is_pow2 is bad");
	static_assert(!is_pow2<std::integral_constant<size_t, (1 << 20) + 1>>(), "The implementation of is_pow2 is bad");
	static_assert(!is_pow2<std::integral_constant<size_t, 63>>(), "The implementation of is_pow2 is bad");

	//-------------------------------------------------------------------------
	//erase_type: erase a type C from a type list

	namespace detail {
		template<typename Seq, typename T>
		struct erase_type_impl;

		template<template <typename...> typename Seq, typename C>
		struct erase_type_impl<Seq<>, C> {
			using type = Seq<>;
		};

		template<template <typename...> typename Seq, typename... Ts, typename T, typename C>
		struct erase_type_impl<Seq<T, Ts...>, C> {
			using type1 = cat< Seq<T>, typename erase_type_impl<Seq<Ts...>, C>::type>;
			using type2 = typename erase_type_impl<Seq<Ts...>, C>::type;
			using type = typename std::conditional< !std::is_same_v<T, C>, type1, type2 >::type;
		};
	}
	template <typename Seq, typename C>
	using erase_type = typename detail::erase_type_impl<Seq, C>::type;

	static_assert( std::is_same_v< erase_type< type_list<double, int, char, double>, double>, type_list<int, char> >,
		"The implementation of erase is bad");

	//-------------------------------------------------------------------------
	//erase_Nth: erase the Nth element of a list

	namespace detail {
		template<typename, size_t>
		struct erase_Nth_impl;

		template <template <typename...> typename Seq, typename T, typename... Ts>
		struct erase_Nth_impl<Seq<T, Ts...>, 0> {
			using type = Seq<Ts...>;
		};

		template <template <typename...> typename Seq, typename T, typename... Ts, size_t N>
		struct erase_Nth_impl<Seq<T, Ts...>, N> {
			using type = cat< Seq<T>, typename erase_Nth_impl<Seq<Ts...>, N-1 >::type >;
		};
	}

	template <typename Seq, size_t N>
	using erase_Nth = typename detail::erase_Nth_impl<Seq, N>::type;

	static_assert( std::is_same_v< erase_Nth< type_list<double, char, bool, double>, 1>, type_list<double, bool, double > > );
	static_assert( std::is_same_v< erase_Nth< type_list<double, char, bool, double>, 0>, type_list<char, bool, double > >);

	namespace detail {
		using example_list = type_list<double, char, bool, double>;
	}
	static_assert(
		std::is_same_v<	
			erase_Nth< detail::example_list, size<detail::example_list>::value -1 >
			, type_list<double, char, bool > >);

	//-------------------------------------------------------------------------
	//has_any_type: check whether a type list contains ANY type of a second typelist

	namespace detail {
		template<typename Seq1, typename Seq2>
		struct has_any_type_impl;

		template<template <typename...> typename Seq1, typename... Ts1, template <typename...> typename Seq2, typename... Ts2>
		struct has_any_type_impl<Seq1<Ts1...>, Seq2<Ts2...>> {
			static const bool value = (has_type<Seq1<Ts1...>, Ts2>::value || ...);
		};
	}
	template <typename Seq1, typename Seq2>
	struct has_any_type {
		static const bool value = detail::has_any_type_impl<Seq1, Seq2>::value;
	};

	static_assert(has_any_type<type_list<double, int, char>, type_list<int, float>>::value, "The implementation of has_any_type is bad");
	static_assert(!has_any_type<type_list<double, int, char>, type_list<bool,float>>::value, "The implementation of has_any_type is bad");

	//-------------------------------------------------------------------------
	//has_all_types: check whether a type list contains ALL types of a second typelist

	namespace detail {
		template<typename Seq1, typename Seq2>
		struct has_all_types_impl;

		template<template <typename...> typename Seq1, typename... Ts1, template <typename...> typename Seq2>
		struct has_all_types_impl<Seq1<Ts1...>, Seq2<>> {
			static const bool value = true;
		};

		template<template <typename...> typename Seq1, typename... Ts1, template <typename...> typename Seq2, typename... Ts2>
		struct has_all_types_impl<Seq1<Ts1...>, Seq2<Ts2...>> {
			static const bool value = (has_type<Seq1<Ts1...>, Ts2>::value && ... && true);
		};
	}
	template <typename Seq1, typename Seq2>
	struct has_all_types {
		static const bool value = detail::has_all_types_impl<Seq1, Seq2>::value;
	};

	static_assert(has_all_types<type_list<double, int, char>, type_list<int, char>>::value, "The implementation of has_all_types is bad");
	static_assert(!has_all_types<type_list<double, int, char>, type_list<bool, char>>::value, "The implementation of has_all_types is bad");
	static_assert(has_all_types<type_list<double, int, char>, type_list<>>::value, "The implementation of has_all_types is bad");

	//-------------------------------------------------------------------------
	//not_in_list: Return those elements of Seq2 that are NOT in Seq1

	namespace detail {
		template<typename Seq1, typename Seq2>
		struct not_in_list_impl;

		template<typename Seq1, template <typename...> typename Seq2>
		struct not_in_list_impl<Seq1, Seq2<>> {
			using type = Seq2<>;
		};

		template<typename Seq1, template <typename...> typename Seq2, typename T, typename... Ts>
		struct not_in_list_impl<Seq1, Seq2<T, Ts...>> {
			using T1 = typename not_in_list_impl<Seq1, tl<Ts...>>::type;
			using T2 = cat< Seq2<T>, typename not_in_list_impl<Seq1, Seq2<Ts...>>::type >;
			using type = std::conditional_t< has_type<Seq1, T>::value, T1, T2 >;
		};

	}
	template <typename Seq1, typename Seq2>
	using not_in_list = typename detail::not_in_list_impl<Seq1, Seq2>::type;

	static_assert(
		std::is_same_v<not_in_list<tl<double, int, char>, tl<bool, int, char, float>>, tl<bool, float>>
		, "The implementation of has_all_types is bad");

	//-------------------------------------------------------------------------
	//filter_remove_types: remove types from a list, that are also part of another list

	namespace detail {
		template< typename Seq1, typename Seq2>
		struct remove_types_impl;

		template<template <typename...> typename Seq1, typename Seq2>
		struct remove_types_impl<Seq1<>, Seq2> {
			using type = Seq1<>;
		};

		template<template <typename...> typename Seq1, typename T, typename... Ts, typename Seq2 >
		struct remove_types_impl<Seq1<T, Ts...>, Seq2> {
			using rest = typename remove_types_impl<Seq1<Ts...>, Seq2>::type;
			using type = std::conditional_t< has_type<Seq2, T>::value, rest, cat< type_list<T>, rest > >;
		};
	}
	template <typename Seq1, typename Seq2>
	using remove_types = typename detail::remove_types_impl<Seq1, Seq2>::type;

	static_assert(std::is_same_v <
		remove_types < type_list<char, float, char, int, bool, double>, type_list < char, double > >
		, type_list<float, int, bool> >,
		"The implementation of remove_types is bad");

	//-------------------------------------------------------------------------
	//filter_have_type: keep only those type lists Ts<...> that have a specific type C as member

	namespace detail {
		template< typename Seq, typename C>
		struct filter_have_type_impl;

		template<template <typename...> typename Seq, typename C>
		struct filter_have_type_impl<Seq<>, C> {
			using type = Seq<>;
		};

		template<template <typename...> typename Seq, typename T, typename... Ts, typename C>
		struct filter_have_type_impl<Seq<T, Ts...>, C> {
			using type1 = cat< Seq<T>, typename filter_have_type_impl<Seq<Ts...>, C>::type>;
			using type2 = typename filter_have_type_impl<Seq<Ts...>, C>::type;

			using type = typename std::conditional< has_type<T, C>::value, type1, type2 >::type;
		};
	}
	template <typename Seq, typename C>
	using filter_have_type = typename detail::filter_have_type_impl<Seq, C>::type;

	static_assert(std::is_same_v<
		filter_have_type< 
			type_list< type_list<char, float>, type_list<char, int, double> >, float >
			, type_list< type_list<char, float> > >,
		"The implementation of filter_have_type is bad");

	static_assert(	std::is_same_v<
		filter_have_type<	
			type_list< type_list<char, float>, type_list<bool, double>, type_list<float, double> >, float >
			, type_list< type_list<char, float>, type_list<float, double>> >,
		"The implementation of filter_have_type is bad");

	//-------------------------------------------------------------------------
	//filter_have_all_types: keep only those type lists Ts<...> that have ALL specified type Cs from another list Seq2<Cs...> as member

	namespace detail {
		template<typename Seq1, typename Seq2>
		struct filter_have_all_types_impl;

		template<template <typename...> typename Seq1, template <typename...> typename Seq2, typename... Cs >
		struct filter_have_all_types_impl<Seq1<>, Seq2<Cs...>> {
			using type = Seq1<>;
		};

		template<template <typename...> typename Seq1, typename T, typename... Ts, template <typename...> typename Seq2, typename... Cs>
		struct filter_have_all_types_impl<Seq1<T, Ts...>, Seq2<Cs...>> {
			using type1 = cat< Seq1<T>, typename filter_have_all_types_impl<Seq1<Ts...>, Seq2<Cs...>>::type  >;
			using type2 = typename filter_have_all_types_impl<Seq1<Ts...>, Seq2<Cs...>>::type;

			using type = typename std::conditional< has_all_types<T, Seq2<Cs...>>::value, type1, type2 >::type;
		};
	}
	template <typename Seq1, typename Seq2>
	using filter_have_all_types = typename detail::filter_have_all_types_impl<Seq1, Seq2>::type;

	static_assert(
		std::is_same_v<
			filter_have_all_types< 
				type_list< type_list<char, float, int>, type_list<char, bool, double>, type_list<float, double, char> >
				, type_list< char, float>  >
		
		, type_list< type_list<char, float, int>, type_list<float, double, char> > >,
		"The implementation of filter_have_all_types is bad");


	//-------------------------------------------------------------------------
	//filter_have_any_type: keep only those type lists Ts<...> that have ANY specified type Cs from another list Seq2<Cs...> as member

	namespace detail {
		template<typename Seq1, typename Seq2>
		struct filter_have_any_type_impl;

		template<template <typename...> typename Seq1, template <typename...> typename Seq2, typename... Cs >
		struct filter_have_any_type_impl<Seq1<>, Seq2<Cs...>> {
			using type = Seq1<>;
		};

		template<template <typename...> typename Seq1, typename T, typename... Ts, template <typename...> typename Seq2, typename... Cs>
		struct filter_have_any_type_impl<Seq1<T, Ts...>, Seq2<Cs...>> {
			using type1 = cat< Seq1<T>, typename filter_have_any_type_impl<Seq1<Ts...>, Seq2<Cs...>>::type  >;
			using type2 = typename filter_have_any_type_impl<Seq1<Ts...>, Seq2<Cs...>>::type;

			using type = typename std::conditional< has_any_type<T, Seq2<Cs...>>::value, type1, type2 >::type;
		};
	}
	template <typename Seq1, typename Seq2>
	using filter_have_any_type = typename detail::filter_have_any_type_impl<Seq1, Seq2>::type;

	static_assert(
		std::is_same_v<
			filter_have_any_type< 
				type_list< type_list<char, int>, type_list<bool, double>, type_list<float, double, char> >
				, type_list<char, float>  >

		, type_list< type_list<char, int>, type_list<float, double, char> > >,
		"The implementation of filter_have_any_type is bad");

	//-------------------------------------------------------------------------
	//unique: test whether the elements in a type list are all unique

	namespace detail {
		template<typename Seq>
		struct unique_impl;

		template<template <typename...> typename Seq>
		struct unique_impl<Seq<>> {
			static const bool value = true;
		};

		template<template <typename...> typename Seq, typename C, typename... Cs >
		struct unique_impl<Seq<C, Cs...>> {
			static const bool value = !has_type<Seq<Cs...>, C>::value && unique_impl<Seq<Cs...>>::value;
		};
	}

	template <typename Seq>
	struct unique {
		static const bool value = detail::unique_impl<Seq>::value;
	};

	static_assert( unique<type_list<int, char, float>>::value, "The implementation of unique is bad");
	static_assert( !unique<type_list<int, char, char, float>>::value, "The implementation of unique is bad");

	//-------------------------------------------------------------------------
	//are_unique: test whether all given lists contain only unique elements

	namespace detail {
		template<typename Seq>
		struct are_unique_impl;

		template<template <typename...> typename Seq>
		struct are_unique_impl<Seq<>> {
			static const bool value = true;
		};

		template<template <typename...> typename Seq, typename... Cs >
		struct are_unique_impl<Seq<Cs...>> {
			static const bool value = ( unique<Cs>::value && ... );
		};
	}

	template <typename Seq>
	struct are_unique {
		static const bool value = detail::are_unique_impl<Seq>::value;
	};

	static_assert( are_unique<type_list<type_list<int, char, float>, type_list<double, char, float, int>>>::value, "The implementation of are_unique is bad");
	static_assert(!are_unique<type_list<type_list<int, char, float>, type_list<int, char, char, float>>>::value, "The implementation of are_unique is bad");


	//-------------------------------------------------------------------------
	//N_tuple: make a tuple containing a type T N times

	template <typename T, size_t N>
	struct N_tuple {
		template <typename U>
		struct impl;

		template <>
		struct impl<std::integer_sequence<std::size_t>> {
			using type = std::tuple<>;
		};

		template <size_t... Is>
		struct impl<std::index_sequence<Is...>> {
			template <size_t >
			using wrap = T;
			using type = std::tuple<wrap<Is>...>;
		};

	public:
		using type = typename impl<std::make_index_sequence<N>>::type;
	};

	static_assert( std::is_same_v< N_tuple<int,4>::type, std::tuple<int,int,int,int> >, "The implementation of N_tuple is bad");

	//-------------------------------------------------------------------------
	//sum: compute the sum of a list of std::integral_constant<size_t, I>

	namespace detail {
		template<typename Seq>
		struct sum_impl;

		template<template <typename...> typename Seq, typename... Ts>
		struct sum_impl<Seq<Ts...>> {
			using type = std::integral_constant<size_t, (Ts::value + ... + 0)>;
		};
	}
	template <typename Seq>
	using sum = typename detail::sum_impl<Seq>::type;

	static_assert(
		std::is_same_v<
		sum< type_list<std::integral_constant<size_t, 1>, std::integral_constant<size_t, 2>, std::integral_constant<size_t, 3> > >
		, std::integral_constant<size_t, 6> >,

		"The implementation of sum is bad");

	//-------------------------------------------------------------------------
	//max: compute the max of a list of std::integral_constant<size_t, I>

	namespace detail {
		template<typename Seq, typename M>
		struct max_impl;

		template<template <typename...> typename Seq, typename M>
		struct max_impl<Seq<>, M> {
			using type = M;
		};

		template<template <typename...> typename Seq, typename... Ts, typename T, typename M>
		struct max_impl<Seq<T,Ts...>, M> {
			using type = typename std::conditional<
							(bool) (T::value > M::value),
							typename max_impl<Seq<Ts...>, T>::type,
							typename max_impl<Seq<Ts...>, M>::type
						 >::type ;
		};
	}
	template <typename Seq>
	using max = typename detail::max_impl<Seq, std::integral_constant<size_t, 0>>::type;

	static_assert(
		std::is_same_v< 
			max< type_list<std::integral_constant<size_t, 5>, std::integral_constant<size_t, 4>, std::integral_constant<size_t, 3> > > 
			, std::integral_constant<size_t, 5> >,
		"The implementation of max is bad");

	//-------------------------------------------------------------------------
	//min: compute the min of a list of std::integral_constant<size_t, I>

	namespace detail {
		template<typename Seq, typename M>
		struct min_impl;

		template<template <typename...> typename Seq, typename M>
		struct min_impl<Seq<>, M> {
			using type = M;
		};

		template<template <typename...> typename Seq, typename T, typename... Ts, typename M>
		struct min_impl<Seq<T, Ts...>, M> {
			static const bool B = T::value < M::value;
			using type = typename std::conditional<
										(bool) (T::value < M::value),
										typename min_impl<Seq<Ts...>, T>::type,
										typename min_impl<Seq<Ts...>, M>::type
								  >::type;
		};
	}
	template <typename Seq>
	using min = typename detail::min_impl<Seq, std::integral_constant<size_t, std::numeric_limits<size_t>::max()>>::type;

	static_assert(
		std::is_same_v<
				min< type_list<std::integral_constant<size_t, 6>, std::integral_constant<size_t, 4>, std::integral_constant<size_t, 9> > >
				, std::integral_constant<size_t, 4> >,
		"The implementation of min is bad");

	//-------------------------------------------------------------------------
	//smallest_pow2_larger_eq: find smallest power of 2 larger or equal than a given number

	namespace detail {
		template<typename P2, typename T>
		struct smaller_or_max {
			using max_t = std::integral_constant<size_t, std::numeric_limits<size_t>::max()>;
			using type = typename std::conditional< (P2::value < T::value), max_t, P2 >::type;
		};

		template<typename Seq, typename T>
		struct smallest_pow2_larger_eq_impl;

		template<template<typename...> typename Seq, typename... Ps, typename T>
		struct smallest_pow2_larger_eq_impl<Seq<Ps...>, T> {
			using type = min< type_list< typename smaller_or_max<Ps, T>::type...> >;
		};
	}

	template <typename T>
	using smallest_pow2_larger_eq = typename detail::smallest_pow2_larger_eq_impl<all_pow2, T>::type;

	static_assert( std::is_same_v< smallest_pow2_larger_eq< std::integral_constant<size_t, 62> >
		, std::integral_constant<size_t, 64> >
		, "The implementation of smallest_pow2_larger is bad");
	
	//-------------------------------------------------------------------------
	//function: compute function on list of std::integral_constant<size_t, I>

	namespace detail {
		template<typename Seq, template<typename> typename Fun>
		struct function_impl;

		template<template <typename...> typename Seq, typename... Ts, template<typename> typename Fun>
		struct function_impl<Seq<Ts...>, Fun> {
			using type = Seq< typename Fun<Ts>::type... >;
		};

		template<typename T>
		struct test_func {
			using type = std::integral_constant<size_t, 2*T::value>;
		};
	}
	template <typename Seq, template<typename> typename Fun>
	using function = typename detail::function_impl<Seq,Fun>::type;

	static_assert(
		std::is_same_v<
			function< 
				type_list<std::integral_constant<size_t, 1>, std::integral_constant<size_t, 2>, std::integral_constant<size_t, 3> >
				, detail::test_func >
			, type_list<std::integral_constant<size_t, 2>, std::integral_constant<size_t, 4>, std::integral_constant<size_t, 6> > >,

		"The implementation of function is bad");

	//-------------------------------------------------------------------------
	//function2: compute function on list of std::integral_constant<size_t, I> which has one additional template parameter

	namespace detail {
		template<typename T, typename Seq, template<typename, typename> typename Fun>
		struct function2_impl;

		template<typename T, template <typename...> typename Seq, typename... Ts, template<typename, typename> typename Fun>
		struct function2_impl<T, Seq<Ts...>, Fun> {
			using type = Seq< typename Fun<T,Ts>::type... >;
		};

		template<typename Ty, typename T>
		struct test_func_function2 {
			using type = std::integral_constant<Ty, 2 * T::value>;
		};
	}
	template <typename T, typename Seq, template<typename, typename> typename Fun>
	using function2 = typename detail::function2_impl<T, Seq, Fun>::type;

	static_assert(
		std::is_same_v<
			function2<size_t, type_list<std::integral_constant<size_t, 1>, std::integral_constant<size_t, 2>, std::integral_constant<size_t, 3> >
				, detail::test_func_function2 >
		, type_list<std::integral_constant<size_t, 2>, std::integral_constant<size_t, 4>, std::integral_constant<size_t, 6> > >,
		"The implementation of function2 is bad");

	//-------------------------------------------------------------------------
	//function3: compute function on list of std::integral_constant<size_t, I> which has two additional template parameters

	namespace detail {
		template<typename T1, typename T2, typename Seq, template<typename, typename, typename> typename Fun>
		struct function3_impl;

		template<typename T1, typename T2, template <typename...> typename Seq, typename... Ts, template<typename, typename, typename> typename Fun>
		struct function3_impl<T1, T2, Seq<Ts...>, Fun> {
			using type = Seq< typename Fun<T1, T2, Ts>::type... >;
		};

		template<typename T1, typename T2, typename T>
		struct test_func_function3 {
			using type = std::integral_constant<T1, 2 * T::value>;
		};
	}
	template <typename T1, typename T2, typename Seq, template<typename, typename, typename> typename Fun>
	using function3 = typename detail::function3_impl<T1, T2, Seq, Fun>::type;

	static_assert(
		std::is_same_v<
		function3<size_t, size_t, type_list<std::integral_constant<size_t, 1>, std::integral_constant<size_t, 2>, std::integral_constant<size_t, 3> >
		, detail::test_func_function3 >
		, type_list<std::integral_constant<size_t, 2>, std::integral_constant<size_t, 4>, std::integral_constant<size_t, 6> > >,
		"The implementation of function3 is bad");

	//-------------------------------------------------------------------------
	//map: find a type key in a map, i.e. a list of type key - type value pairs, and retrieve its type value, or a default type if not found

	namespace detail {

		template<typename Map, typename Key, typename Default>
		struct map_impl {
			using Keys = transform<Map, front>;

			using type = typename std::conditional<
				has_type<Keys, Key>::value									//keys contain the key?
				, back< Nth_type<Map, index_of<Keys, Key>::value> >			//yes - get the value
				, Default													//no - get default value
			>::type;
		};

		template<template<typename...> typename Map, typename Key, typename Default>
		struct map_impl<Map<>, Key, Default> {
			using type = Default;
		};

		using test_map = type_list<
			  type_list<int, char>
			, type_list<float, double>
			, type_list<double, float>
		>;

		using test_map2 = type_list<>;
	}
	template <typename Map, typename Key, typename Default>
	using map = typename detail::map_impl<Map, Key, Default>::type;

	static_assert(std::is_same_v< map<detail::test_map, int, float >, char >, "The implementation of map is bad");
	static_assert(std::is_same_v< map<detail::test_map, char, float >, float >, "The implementation of map is bad");
	static_assert(std::is_same_v< map<detail::test_map2, int, float >, float >, "The implementation of map is bad");


	//-------------------------------------------------------------------------
	//apply_map: apply a list of keys to a map, get the list of their values, of defaults if the keys are not found
	namespace detail {
		template<typename Map, typename Keys, typename Default>
		struct apply_map_impl;

		template<typename Map, template<typename...> typename Keys, typename... Ks, typename Default>
		struct apply_map_impl<Map, Keys<Ks...>, Default> {
			using type = vtll::type_list< vtll::map<Map, Ks, Default>... >;
		};
	}

	template<typename Map, typename Keys, typename Default>
	using apply_map = typename detail::apply_map_impl<Map, Keys, Default>::type;

	static_assert(
		std::is_same_v< apply_map<detail::test_map, type_list<int, float, char>, char >, type_list<char, double, char> >, 
		"The implementation of apply_map is bad");

	//-------------------------------------------------------------------------
	//remove_duplicates: remove duplicates from a list

	namespace detail {
		template<typename Seq>
		struct remove_duplicates_impl;

		template<template<typename...> typename Seq>
		struct remove_duplicates_impl<Seq<>> {
			using type = Seq<>;
		};

		template<typename T, template<typename...> typename Seq>
		struct remove_duplicates_impl<Seq<T>> {
			using type = Seq<T>;
		};

		template<typename T, template<typename...> typename Seq, typename... Ts>
		struct remove_duplicates_impl<Seq<T, Ts...>> {
			using type = typename std::conditional_t<	has_type<Seq<Ts...>, T>::value
														, typename remove_duplicates_impl<Seq<Ts...>>::type
														, cat< type_list<T>, typename remove_duplicates_impl<Seq<Ts...>>::type >
													>;
		};
	}

	template<typename Seq>
	using remove_duplicates = typename detail::remove_duplicates_impl<Seq>::type;

	static_assert(
		std::is_same_v< remove_duplicates<type_list<char>>, type_list<char> >,
		"The implementation of remove_duplicates is bad");

	static_assert(
		std::is_same_v< remove_duplicates<type_list<char, char>>, type_list<char> >,
		"The implementation of remove_duplicates is bad");

	static_assert(
		std::is_same_v< remove_duplicates<type_list<int, float, int, char, char >>, type_list<float, int, char> >,
		"The implementation of remove_duplicates is bad");

	//-------------------------------------------------------------------------
	//flatten: turn a list LL of lists into one list that holds all elements from LL

	namespace detail {
		template<typename Seq>
		struct flatten_impl;

		template<template<typename...> typename Seq>
		struct flatten_impl<Seq<>> {
			using type = type_list<>;
		};

		template<typename T, template<typename...> typename Seq>
		struct flatten_impl<Seq<T>> {
			using type = T;
		};

		template<typename T, template<typename...> typename Seq, typename... Ts>
		struct flatten_impl<Seq<T, Ts...>> {
			using type = cat< T, typename flatten_impl<Seq<Ts...>>::type >;
		};
	}

	template<typename Seq>
	using flatten = typename detail::flatten_impl<Seq>::type;

	static_assert(
		std::is_same_v< flatten< type_list<type_list<int, float>, type_list<int, char>, type_list<double>, type_list<> > >
						, type_list<int, float, int, char, double> >,
		"The implementation of flatten is bad");

	//-------------------------------------------------------------------------
	//intersection: return a list of those elements that are in all given lists

	namespace detail {
		template<typename Seq, typename E>
		struct intersection_impl;

		template<template<typename...> typename Seq, typename... Ts, typename E>
		struct intersection_impl<Seq<Ts...>, Seq<E>> {
			using type = typename std::conditional_t<	have_type<Seq<Ts...>, E>::value
														, type_list<E>
														, type_list<>
													>;
		};

		template<template<typename...> typename Seq, typename... Ts, typename E, typename... Es>
		struct intersection_impl<Seq<Ts...>, Seq<E, Es...>> {
			using type = typename std::conditional_t<	have_type<Seq<Ts...>, E>::value
														, cat< type_list<E>, typename intersection_impl<Seq<Ts...>, Seq<Es...>>::type >
														, typename intersection_impl<Seq<Ts...>, Seq<Es...>>::type
													>;
		};
	}

	template<typename Seq>
	using intersection = typename detail::intersection_impl<Seq, remove_duplicates<flatten<Seq>>>::type;

	static_assert(
		std::is_same_v< intersection< type_list<type_list<int, float, char>, type_list<int, char>, type_list<double, char, int>> >
						, type_list<char, int> >,  
		"The implementation of intersection is bad");

	//-------------------------------------------------------------------------
	//power_set: turn a set into a set of all subsets

	namespace detail {
		template <typename Seq>
		struct power_set_impl;

		template<template<typename...> typename Seq>
		struct power_set_impl<Seq<>> {
			using type = type_list<type_list<>>;
		};

		template<template<typename...> typename Seq, typename T>
		struct power_set_impl<Seq<T>> {
			using type = type_list<type_list<>, type_list<T>>;
		};

		template < template<typename...> typename Seq, typename... Ts, typename T>
		struct power_set_impl<Seq<T, Ts...>> {
			using ps = typename power_set_impl<type_list<Ts...>>::type;
			using type = cat< ps, transform_front< ps, cat, type_list<T> > >;
		};
	}

	template <typename Seq>
	using power_set = typename detail::power_set_impl<Seq>::type;

	static_assert(
		std::is_same_v< power_set< type_list<char> >, type_list< type_list<>, type_list<char> >
		>, "The implementation of power_set is bad");

	static_assert(
		std::is_same_v< power_set< type_list<int, char> >
		, type_list< type_list<>, type_list<char>, type_list<int>, type_list<int, char>>
		>, "The implementation of power_set is bad");

	//-------------------------------------------------------------------------
	//static for: with this compile time for loop you can loop over any tuple, type list, or variadic argument list

	namespace detail {
		template <typename T, T Begin, class Func, T... Is>
		constexpr void static_for_impl(Func&& f, std::integer_sequence<T, Is...>) {
			(f(std::integral_constant<T, Begin + Is>{ }), ...);
		}
	}

	template <typename T, T Begin, T End, class Func >
	constexpr void static_for(Func&& f) {
		detail::static_for_impl<T, Begin>(std::forward<Func>(f), std::make_integer_sequence<T, End - Begin>{ });
	}

	namespace detail {
		void f() {
			using list = type_list<int, double, bool, float >;

			auto fun = [&]<typename T, T I>(std::integral_constant<T, I> i) {
				using type = Nth_type<list, I>;
				std::cout << i << " " << typeid(type).name() << std::endl;
			};

			static_for< int, 0, size<list>::value >(fun);
		}
	}


	//-------------------------------------------------------------------------
	//is_atomic: true if the provided type is atomic

	template<typename T>
	struct is_atomic : std::false_type {};

	template<typename T>
	struct is_atomic<std::atomic<T>> : std::true_type {};


	//-------------------------------------------------------------------------
	//remove_atomic: change std::atomic<type> to type

	namespace detail {
		template<typename Seq>
		struct remove_atomic_impl;

		template<template <typename...> typename Seq>
		struct remove_atomic_impl<Seq<>> {
			using type = Seq<>;
		};

		template<template <typename...> typename Seq, typename T>
		struct remove_atomic_impl<Seq<T>> {
			using type = Seq<T>;
		};

		template<template <typename...> typename Seq, typename T>
		struct remove_atomic_impl<Seq<std::atomic<T>>> {
			using type = Seq<T>;
		};

		template<template <typename...> typename Seq, typename T, typename... Ts >
		struct remove_atomic_impl<Seq<T, Ts...>> {
			using type = cat< typename remove_atomic_impl<Seq<T>>::type, typename remove_atomic_impl<Seq<Ts...>>::type >;
		};
	}
	template <typename Seq>
	using remove_atomic = typename detail::remove_atomic_impl<Seq>::type;

	static_assert(std::is_same_v <
			remove_atomic < type_list<char, float, char, int, bool, double> >, 
			type_list<char, float, char, int, bool, double> 
		>,
		"The implementation of remove_atomic is bad");

	static_assert(std::is_same_v <
			remove_atomic < type_list< std::atomic<char>, float, char, int, bool, std::atomic<double>> >, 
			type_list<char, float, char, int, bool, double>  
		>,
		"The implementation of remove_atomic is bad");


	//--------------------------------------------------------------------------------------------
	//type counter lifted from https://mc-deltat.github.io/articles/stateful-metaprogramming-cpp20
	//a compile time counter starting from 0. Every time you put *counter<>* into your code, it is increased by 1. 

	template<size_t N>
	struct reader { friend auto counted_flag(reader<N>); };

	template<size_t N>
	struct setter {
		friend auto counted_flag(reader<N>) {}
		static constexpr size_t n = N;
	};

	template< auto Tag, size_t NextVal = 0 >
	[[nodiscard]] consteval auto counter_impl() {
		constexpr bool counted_past_value = requires(reader<NextVal> r) { counted_flag(r); };

		if constexpr (counted_past_value) {
			return counter_impl<Tag, NextVal + 1>();
		}
		else {
			setter<NextVal> s;
			return s.n;
		}
	}

	template< auto Tag = [] {}, auto Val = counter_impl<Tag>() >
	constexpr auto counter = Val;


	//-------------------------------------------------------------------------
	//tuple algorithms

	//-------------------------------------------------------------------------
	//to_tuple: turn a list into a tuple

	namespace detail {
		template<typename Seq>
		struct to_tuple_impl;

		template<template <typename...> class Seq>
		struct to_tuple_impl<Seq<>> {
			using type = std::tuple<>;
		};

		template<template <typename...> class Seq, typename... Ts>
		struct to_tuple_impl<Seq<Ts...>> {
			using type = std::tuple<Ts...>;
		};
	}
	template <typename Seq>
	using to_tuple = typename detail::to_tuple_impl<Seq>::type;

	static_assert(
		std::is_same_v< to_tuple<type_list<double, int>>, std::tuple<double, int> >,
		"The implementation of to_tuple is bad");

	//-------------------------------------------------------------------------
	//to_ref_tuple: turn a list into a tuple of reference types. when creating such tuples, use std::ref as a wrapper for the elements!

	namespace detail {
		template<typename Seq>
		struct to_ref_tuple_impl;

		template<template <typename...> class Seq>
		struct to_ref_tuple_impl<Seq<>> {
			using type = std::tuple<>;
		};

		template<template <typename...> class Seq, typename... Ts>
		struct to_ref_tuple_impl<Seq<Ts...>> {
			using type = std::tuple<Ts&...>;
		};
	}
	template <typename Seq>
	using to_ref_tuple = typename detail::to_ref_tuple_impl<Seq>::type;

	static_assert(
		std::is_same_v< to_ref_tuple<type_list<double, int>>, std::tuple<double&, int&> >,
		"The implementation of to_ref_tuple is bad");

	//-------------------------------------------------------------------------
	//to_rvref_tuple: turn a list into a tuple of reference types. when creating such tuples, use std::ref as a wrapper for the elements!

	namespace detail {
		template<typename Seq>
		struct to_rvref_tuple_impl;

		template<template <typename...> class Seq>
		struct to_rvref_tuple_impl<Seq<>> {
			using type = std::tuple<>;
		};

		template<template <typename...> class Seq, typename... Ts>
		struct to_rvref_tuple_impl<Seq<Ts...>> {
			using type = std::tuple<Ts&&...>;
		};
	}
	template <typename Seq>
	using to_rvref_tuple = typename detail::to_rvref_tuple_impl<Seq>::type;

	static_assert(
		std::is_same_v< to_rvref_tuple<type_list<double, int>>, std::tuple<double&&, int&&> >,
		"The implementation of to_rvref_tuple is bad");

	//-------------------------------------------------------------------------
	//to_ptr_tuple: turn a list into a tuple of pointer types

	namespace detail {
		template<typename Seq>
		struct to_ptr_tuple_impl;

		template<template <typename...> class Seq>
		struct to_ptr_tuple_impl<Seq<>> {
			using type = std::tuple<>;
		};

		template<template <typename...> class Seq, typename... Ts>
		struct to_ptr_tuple_impl<Seq<Ts...>> {
			using type = std::tuple<Ts*...>;
		};
	}
	template <typename Seq>
	using to_ptr_tuple = typename detail::to_ptr_tuple_impl<Seq>::type;

	static_assert(
		std::is_same_v< to_ptr_tuple<type_list<double, int>>, std::tuple<double*, int*> >,
		"The implementation of to_ptr_tuple is bad");

	//-------------------------------------------------------------------------
	//ptr_tuple_to_ref_tuple: turn a tuple of pointers into a tuple of references

	namespace detail {
		template<typename ...T, size_t... I>
		auto ptr_to_ref_tuple_detail(const std::tuple<T...>& t, std::index_sequence<I...>) {
			return std::tie(*std::get<I>(t)...);
		}
	}

	template<typename ...T>
	auto ptr_to_ref_tuple(const std::tuple<T...>& t) {
		return detail::ptr_to_ref_tuple_detail<T...>(t, std::make_index_sequence<sizeof...(T)>{});
	}

	namespace detail::ex1 {
		int i;
		double d;
		char c;
		auto tup1 = std::make_tuple(&i, &d, &c);
		auto tup2 = ptr_to_ref_tuple(tup1);
	}

	static_assert(
		std::is_same_v< decltype(detail::ex1::tup2), std::tuple<int&, double&, char&> >,
		"The implementation of ptr_to_ref_tuple is bad");

	//-------------------------------------------------------------------------
	//is_same_tuple: test whether two tuples are the same (Note: Clang does not accept strings as tuple elements)

	template <typename T>
	constexpr auto is_same_tuple(T a, T b) {
  		return [&a, &b]<std::size_t ...I>(std::index_sequence<I...>) {
    		return ( (std::get<I>(a) == std::get<I>(b) ) && ... && true );
  		}(std::make_index_sequence<std::tuple_size_v<T>>{});
	}

	template <typename T1, typename T2>
	constexpr auto is_same_tuple(const T1& t1, const T2& t2) {
		return false;
	}

	static_assert( is_same_tuple( std::make_tuple(1, 'a', 4.5), std::make_tuple(1, 'a', 4.5) ), "The implementation of is_same_tuple is bad");
	static_assert(!is_same_tuple( std::make_tuple(1, 'a', 4.5), std::make_tuple(1, 'b', 4.5) ), "The implementation of is_same_tuple is bad");
	static_assert(!is_same_tuple( std::make_tuple(1, 'a', 4.5), std::make_tuple('a', 4.5)), "The implementation of is_same_tuple is bad");

	//-------------------------------------------------------------------------
	//sub_tuple: extract a subtuple from a tuple

	namespace detail {
		template<size_t Begin, typename T, size_t... Is>
		constexpr auto sub_tuple_impl(T&& tup, std::index_sequence<Is...>) {
			return std::make_tuple(std::get<Begin + Is>(tup)...);
		}
	}

	template <size_t Begin, size_t End, typename T>
	constexpr auto sub_tuple(T&& tup) {
		return detail::sub_tuple_impl<Begin>(std::forward<T>(tup), std::make_integer_sequence<size_t, End - Begin>{ });
	}

	static_assert(is_same_tuple(sub_tuple<2, 4>(std::make_tuple(1, "a", 4.5, 'C', 5.0f)), std::make_tuple(4.5, 'C')), "The implementation of sub_tuple is bad");
	static_assert(is_same_tuple(sub_tuple<0, 3>(std::make_tuple(0.0, 1, 5.7f, 4.5, 'C', 5.0f)), std::make_tuple(0.0, 1, 5.7f)), "The implementation of sub_tuple is bad");
	static_assert(!is_same_tuple(sub_tuple<2, 4>(std::make_tuple(1, "a", 4.5, 'C', 5.0f)), std::make_tuple("a", 4.5, 'C')), "The implementation of sub_tuple is bad");
	static_assert(!is_same_tuple(sub_tuple<2, 4>(std::make_tuple(1, "a", 4.5, 'C', 5.0f)), std::make_tuple('C')), "The implementation of sub_tuple is bad");

	//-------------------------------------------------------------------------
	//sub_ref_tuple: extract a subtuple from a tuple of references

	namespace detail {
		template<size_t Begin, typename T, size_t... Is>
		constexpr auto sub_ref_tuple_impl(T&& tup, std::index_sequence<Is...>) {
			return std::tie(std::get<Begin + Is>(tup)...);
		}
		float data0 = 0.0f;
		int data1 = 1;
		char data2 = 'A';
		double data3 = 3.0;
		bool data4 = true;
	}

	template <size_t Begin, size_t End, typename T>
	constexpr auto sub_ref_tuple(T&& tup) {
		return detail::sub_ref_tuple_impl<Begin>(std::forward<T>(tup), std::make_integer_sequence<size_t, End - Begin>{ });
	}

	/*static_assert( 
		is_same_tuple(
			sub_ref_tuple<2, 4>(std::make_tuple(detail::data0, detail::data1, detail::data2, detail::data3, detail::data4))
			, std::make_tuple(1, 2)
		)
		, "The implementation of sub_ref_tuple is bad");*/


	//-------------------------------------------------------------------------
	//subtype_tuple: extract a subtuple from a tuple using types

	namespace detail {

		template<typename Seq, typename T, size_t... Is>
		constexpr auto subtype_tuple_impl(T&& tup, std::index_sequence<Is...>) {
			return std::make_tuple(std::get<vtll::Nth_type<Seq,Is>>(tup)...);
		}
	}

	template <typename Seq, typename T>
	constexpr auto subtype_tuple(T&& tup) {
		return detail::subtype_tuple_impl<Seq>(std::forward<T>(tup), std::make_integer_sequence<size_t, vtll::size<Seq>::value>{ });
	}

	static_assert(is_same_tuple(subtype_tuple<vtll::tl<int,char>>(std::make_tuple(1, "a", 4.5, 'C', 5.0f)), std::make_tuple(1,'C'))
		, "The implementation of subtype_tuple is bad");


	//-------------------------------------------------------------------------
	//value list algorithms


	//-------------------------------------------------------------------------
	//size_value: get the size of a value list

	namespace detail {
		template <typename Seq>
		struct size_value_impl;

		template < template<size_t...> typename Seq, size_t... Is>
		struct size_value_impl<Seq<Is...>> {
			using type = std::integral_constant<size_t, sizeof...(Is)>;
		};
	}

	template <typename Seq>
	using size_value = typename detail::size_value_impl<Seq>::type;

	static_assert(std::is_same_v< size_value< value_list<1, 2, 5>>, std::integral_constant<size_t, 3> >,
		"The implementation of size_value is bad");

	//-------------------------------------------------------------------------
	//Nth_value: get the Nth value from a value list

	namespace detail {
		template <typename Seq, size_t N>
		struct Nth_value_impl;

		template < template<size_t...> typename Seq, size_t... Is, size_t N>
		struct Nth_value_impl<Seq<Is...>,N> {
			using type = Nth_type< type_list<std::integral_constant<size_t, Is>...>, N>;
		};
	}

	template <typename Seq, size_t N>
	using Nth_value = typename detail::Nth_value_impl<Seq, N>::type;

	static_assert( std::is_same_v< Nth_value< value_list<1, 2, 3>, 1 >, std::integral_constant<size_t, 2> >,
		"The implementation of Nth_value is bad");

	//-------------------------------------------------------------------------
	//front_value: get the first value from a value list

	template <typename Seq>
	using front_value = typename Nth_value<Seq, 0>::type;

	static_assert(std::is_same_v< front_value< value_list<1, 2, 3> >, std::integral_constant<size_t, 1> >,
		"The implementation of front_value is bad");

	//-------------------------------------------------------------------------
	//back_value: get the last value from a value list

	template <typename Seq>
	using back_value = typename Nth_value<Seq, std::integral_constant<size_t, size_value<Seq>::value - 1>::value >::type;

	static_assert(std::is_same_v< back_value< value_list<1, 2, 6> >, std::integral_constant<size_t, 6> >,
		"The implementation of back_value is bad");

	//-------------------------------------------------------------------------
	//sum_value: compute the sum of a list of size_t s

	template <size_t... Is>
	using sum_value = std::integral_constant<size_t, (Is + ... + 0)>;

	static_assert(std::is_same_v< sum_value< 1, 2, 3>, std::integral_constant<size_t, 6> >,
		"The implementation of sum_value is bad");

	//-------------------------------------------------------------------------
	//cat_value: concatenate value lists

	template <typename... Seq>
	using cat_value = type_to_value< cat< value_to_type<Seq>... > >;

	//-------------------------------------------------------------------------
	//make_value_list: make a value list going from 0 to N-1

	namespace detail {
		template <size_t N, size_t K>
		struct make_value_list_impl {
			using type = cat_value < value_list<K - 1>, typename make_value_list_impl<N, K+1>::type > ;
		};

		template <size_t N>
		struct make_value_list_impl<N,N> {
			using type = value_list<N-1>;
		};
	}

	template <size_t N>
	using make_value_list = typename detail::make_value_list_impl<N,1>::type;

	static_assert(std::is_same_v< make_value_list<7>, value_list<0,1,2,3,4,5,6> >, "The implementation of make_value_list is bad");

	//-------------------------------------------------------------------------
	//is_pow2_value: test whether a value is a power of 2

	template <size_t I>
	constexpr auto is_pow2_value() {
		return is_pow2<std::integral_constant<size_t, I>>();
	}

	static_assert(is_pow2_value<64>(), "The implementation of is_pow2 is bad");
	static_assert(!is_pow2_value<63>(), "The implementation of is_pow2 is bad");

	//-------------------------------------------------------------------------
	//smallest_pow2_leq_value: find smallest power of 2 that is larger or equal to a given size_t

	template<size_t I>
	using smallest_pow2_leq_value = smallest_pow2_larger_eq<std::integral_constant<size_t, I>>;

	static_assert((smallest_pow2_leq_value<3>::value == 4), "The implementation of smallest_pow2_leq_value is bad");
	static_assert((smallest_pow2_leq_value<4>::value == 4), "The implementation of smallest_pow2_leq_value is bad");
	static_assert((smallest_pow2_leq_value<5>::value == 8), "The implementation of smallest_pow2_leq_value is bad");
	static_assert((smallest_pow2_leq_value<8>::value == 8), "The implementation of smallest_pow2_leq_value is bad");

	//-------------------------------------------------------------------------
	//function_value: compute function on a list of size_t s

	namespace detail {
		template<size_t I>
		struct test_func2 {
			using type = std::integral_constant<size_t, 2 * I>;
		};
	}

	template <template<size_t> typename Fun, size_t... Is>
	using function_value = vtll::type_list< typename Fun<Is>::type... >;

	static_assert(
		std::is_same_v<
		function_value< detail::test_func2, 1, 2, 3 >
		, type_list< std::integral_constant<size_t, 2 >, std::integral_constant<size_t, 4 >, std::integral_constant<size_t, 6 > >
		>,
		"The implementation of function_value is bad");


	//-------------------------------------------------------------------------
	//index_largest_bit: Find index of largest bit, starting with 1

	namespace detail {
		template<size_t Is, typename T>
		struct larger_or_max_pow2 {
			using type = typename std::conditional<
				((1ULL << Is) > T::value)
				, std::integral_constant<size_t, Is >
				, std::integral_constant<size_t, std::numeric_limits<size_t>::max()>
			>::type;
		};

		template<typename Seq, typename T>
		struct index_largest_bit_impl;

		template<template<size_t...> typename Seq, size_t... Is, typename T>
		struct index_largest_bit_impl<Seq<Is...>, T> {
			using type = min < type_list< typename larger_or_max_pow2<Is, T>::type... > >;
		};
	}

	template <typename T>
	using index_largest_bit = typename detail::index_largest_bit_impl< make_value_list<64>, T>::type;

	static_assert(std::is_same_v< index_largest_bit< std::integral_constant<size_t, 0> >
		, std::integral_constant<size_t, 0> >, "The implementation of index_largest_bit is bad");

	static_assert(std::is_same_v< index_largest_bit< std::integral_constant<size_t, 1> >
		, std::integral_constant<size_t, 1> >, "The implementation of index_largest_bit is bad");

	static_assert(std::is_same_v< index_largest_bit< std::integral_constant<size_t, 2> >
		, std::integral_constant<size_t, 2> >, "The implementation of index_largest_bit is bad");

	static_assert(std::is_same_v< index_largest_bit< std::integral_constant<size_t, 3> >
		, std::integral_constant<size_t, 2> >, "The implementation of index_largest_bit is bad");

	static_assert(std::is_same_v< index_largest_bit< std::integral_constant<size_t, 8> >
		, std::integral_constant<size_t, 4> >, "The implementation of index_largest_bit is bad");

	static_assert(std::is_same_v< index_largest_bit< std::integral_constant<size_t, 15> >
		, std::integral_constant<size_t, 4> >, "The implementation of index_largest_bit is bad");



}



#endif
