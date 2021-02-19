#ifndef VTLL_H
#define VTLL_H

#include <tuple>
#include <variant>
#include <iostream>

//
//	Vienna Type List Library
//	By Helmut Hlavacs, University of Vienna
//
//	A collection of C++ typelist algorithms
//
//	See the static_assert statements to understand what each function does
//
//
//	Based on the following source:
//	https://nilsdeppe.com/posts/tmpl-part2
//


namespace vtll {

	//standard list of types. Any such list can be used.

	template <typename... Ts>
	struct type_list {
		using size = std::integral_constant<std::size_t, sizeof...(Ts)>;
	};

	template <>
	struct type_list<> {
		using size = std::integral_constant<std::size_t, 0>;
	};

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

		template <int N, template <typename...> typename Seq, typename... Ts>
		struct Nth_type_impl<Seq<Ts...>, N> {
			using type = typename std::tuple_element<N, std::tuple<Ts...>>::type;
		};
	}

	template <typename Seq, size_t N>
	using Nth_type = typename detail::Nth_type_impl<Seq, N>::type;

	static_assert(
		std::is_same<Nth_type<type_list<double, char, bool, double>, 1>, char>::value,
		"The implementation of Nth_type is bad");

	//-------------------------------------------------------------------------
	//front: first element of a type list

	template <typename Seq>
	using front = Nth_type<Seq,0>;

	static_assert(
		std::is_same< front<type_list<double, char, bool, float>>, double >::value,
		"The implementation of front is bad");

	//-------------------------------------------------------------------------
	//pop_front: pop the front element from a type list

	namespace detail {
		template <typename Seq>
		struct pop_front_impl;

		template <template <typename...> typename Seq, typename T, typename... Ts>
		struct pop_front_impl<Seq<T, Ts...>> {
			using type = Seq<Ts...>;
		};
	}

	template <typename Seq>
	using pop_front = typename detail::pop_front_impl<Seq>::type;

	static_assert(std::is_same<pop_front<type_list<double, char, bool, double>>, type_list<char, bool, double>>::value,
		"The implementation of pop_front is bad");

	//-------------------------------------------------------------------------
	//back: get the last element from a list

	template <typename Seq, size_t N>
	using back = Nth_type<Seq, std::integral_constant<std::size_t, size<Seq>::value - 1>::value >;

	static_assert(
		std::is_same<back<type_list<double, char, bool, float>, 1>, float>::value,
		"The implementation of back is bad");

	static_assert(
		!std::is_same<back<type_list<double, char, bool, float>, 1>, char>::value,
		"The implementation of back is bad");

	//-------------------------------------------------------------------------
	//index_of: index of a type within a type list

	namespace detail {
		template<typename, typename>
		struct index_of_impl {};

		// Index Of base case: found the type we're looking for.
		template <typename T, template <typename...> typename Seq, typename... Ts>
		struct index_of_impl<Seq<T, Ts...>,T> : std::integral_constant<std::size_t, 0> {
			using type = std::integral_constant<std::size_t, 0>;
		};

		// Index Of recursive case: 1 + Index Of the rest of the types.
		template <typename T, typename TOther, template <typename...> typename Seq, typename... Ts>
		struct index_of_impl<Seq<TOther, Ts...>,T> : std::integral_constant<std::size_t, 1 + index_of_impl<Seq<Ts...>,T>::value> {
			using type = std::integral_constant<std::size_t, 1 + index_of_impl<Seq<Ts...>,T>::value>;
		};
	}

	template <typename Seq, typename T>
	using index_of = typename detail::index_of_impl<Seq,T>::type;

	static_assert(index_of< type_list<double, char, bool, double>, char >::value == 1);

	static_assert(
		std::is_same_v< index_of< type_list<double, char, bool, double>, char >, std::integral_constant<std::size_t, 1> >,
		"The implementation of index_of is bad");

	//-------------------------------------------------------------------------
	//cat: concatenate two type lists to one big type list, the result is of the first list type

	namespace detail {
		template <typename Seq1, typename Seq2>
		struct cat_impl;

		template <template <typename...> typename Seq1, template <typename...> typename Seq2, typename... Ts>
		struct cat_impl<Seq1<>, Seq2<Ts...>> {
			using type = Seq1<Ts...>;
		};

		template <template <typename...> typename Seq1, template <typename...> typename Seq2, typename... Ts>
		struct cat_impl<Seq1<Ts...>, Seq2<>> {
			using type = Seq1<Ts...>;
		};

		template <template <typename...> typename Seq1, typename... Ts1, template <typename...> typename Seq2, typename T, typename... Ts2>
		struct cat_impl<Seq1<Ts1...>, Seq2<T, Ts2...>> {
			using type = typename cat_impl<Seq1<Ts1..., T>, Seq2<Ts2...>>::type;
		};
	}

	template <typename Seq1, typename Seq2>
	using cat = typename detail::cat_impl<Seq1, Seq2>::type;

	static_assert(
		std::is_same_v< cat< type_list<double, int>, detail::type_list2<char, float> >, type_list<double, int, char, float> >,
		"The implementation of cat is bad");

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
	//variant_type: make a summary variant type of all elements in a list

	namespace detail {
		template <typename Seq>
		struct variant_type_impl;

		template <template <typename...> typename Seq, typename... Ts>
		struct variant_type_impl<Seq<Ts...>> {
			using type = std::variant<Ts...>;
		};
	}

	template <typename Seq>
	using variant_type = typename detail::variant_type_impl<Seq>::type;

	static_assert(
		std::is_same_v< variant_type< type_list<double, int, char> >, std::variant<double, int, char> >,
		"The implementation of variant_type is bad");

	//-------------------------------------------------------------------------
	//transform: transform a list of types into a list of Function<types>

	namespace detail {
		template<typename List, template<typename> typename Fun>
		struct transform_impl;

		template<template <typename...> typename Seq, typename ...Ts, template<typename> typename Fun>
		struct transform_impl<Seq<Ts...>, Fun> {
			using type = Seq<Fun<Ts>...>;
		};
	}
	template <typename Seq, template<typename> typename Fun>
	using transform = typename detail::transform_impl<Seq, Fun>::type;

	static_assert(
		std::is_same_v< transform< type_list<double, int>, detail::type_list2 >, type_list<detail::type_list2<double>, detail::type_list2<int>> >,
		"The implementation of transform is bad");

	//-------------------------------------------------------------------------
	//substitute: substitute a type list TYPE with another list type

	namespace detail {
		template<typename List, template<typename> typename Fun>
		struct substitute_impl;

		template<template <typename...> typename Seq, typename... Ts, template<typename...> typename Fun>
		struct substitute_impl<Seq<Ts...>, Fun> {
			using type = Fun<Ts...>;
		};
	}
	template <typename Seq, template<typename> typename Fun>
	using substitute = typename detail::substitute_impl<Seq, Fun>::type;

	static_assert(
		std::is_same_v< substitute< type_list<double, int, char>, detail::type_list2 >, detail::type_list2<double, int, char> >,
		"The implementation of substitute is bad");

	//-------------------------------------------------------------------------
	//transfer: transfer a list of types1 into a list of types2

	namespace detail {
		template<typename List, template<typename> typename Fun>
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
	template <typename Seq, template<typename> typename Fun>
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
	struct to_tuple {
		using type = typename detail::to_tuple_impl<Seq>::type;
	};

	static_assert(
		std::is_same_v< to_tuple<type_list<double, int>>::type, std::tuple<double, int> >,
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
	struct to_ref_tuple {
		using type = typename detail::to_ref_tuple_impl<Seq>::type;
	};

	static_assert(
		std::is_same_v< to_ref_tuple<type_list<double, int>>::type, std::tuple<double&, int&> >,
		"The implementation of to_ref_tuple is bad");


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
	struct to_ptr_tuple {
		using type = typename detail::to_ptr_tuple_impl<Seq>::type;
	};

	static_assert(
		std::is_same_v< to_ptr_tuple<type_list<double, int>>::type, std::tuple<double*, int*> >,
		"The implementation of to_ptr_tuple is bad");

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
	//erase: erase a type C from a type list

	namespace detail {
		template<typename Seq, typename T>
		struct erase_impl;

		template<template <typename...> typename Seq, typename C>
		struct erase_impl<Seq<>, C> {
			using type = Seq<>;
		};

		template<template <typename...> typename Seq, typename... Ts, typename T, typename C>
		struct erase_impl<Seq<T, Ts...>, C> {
			using type1 = cat< Seq<T>, typename erase_impl<Seq<Ts...>, C>::type>;
			using type2 = typename erase_impl<Seq<Ts...>, C>::type;
			using type = typename std::conditional< !std::is_same_v<T, C>, type1, type2 >::type;
		};
	}
	template <typename Seq, typename C>
	struct erase {
		using type = typename detail::erase_impl<Seq, C>::type;
	};

	static_assert( std::is_same_v< typename erase< type_list<double, int, char, double>, double>::type, type_list<int, char> >, 
		"The implementation of erase is bad");

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

		template<template <typename...> typename Seq1, typename... Ts1, template <typename...> typename Seq2, typename... Ts2>
		struct has_all_types_impl<Seq1<Ts1...>, Seq2<Ts2...>> {
			static const bool value = (has_type<Seq1<Ts1...>, Ts2>::value && ...);
		};
	}
	template <typename Seq1, typename Seq2>
	struct has_all_types {
		static const bool value = detail::has_all_types_impl<Seq1, Seq2>::value;
	};

	static_assert(has_all_types<type_list<double, int, char>, type_list<int, char>>::value, "The implementation of has_all_types is bad");
	static_assert(!has_all_types<type_list<double, int, char>, type_list<bool, char>>::value, "The implementation of has_all_types is bad");


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
	struct filter_have_type {
		using type = typename detail::filter_have_type_impl<Seq, C>::type;
	};

	static_assert(std::is_same_v<
		typename filter_have_type< type_list< type_list<char, float>, type_list<char, int, double> >, float >::type, type_list< type_list<char, float> > >,
		"The implementation of filter_have_type is bad");

	static_assert(	std::is_same_v<
		typename filter_have_type<	type_list< type_list<char, float>, type_list<bool, double>, type_list<float, double> >, float >::type,
							type_list< type_list<char, float>, type_list<float, double>> >,
		"The implementation of filter_have_type is bad");


	//-------------------------------------------------------------------------
	//filter_have_all_type: keep only those type lists Ts<...> that have ALL specified type Cs from another list Seq2<Cs...> as member

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
	struct filter_have_all_types {
		using type = typename detail::filter_have_all_types_impl<Seq1, Seq2>::type;
	};

	static_assert(
		std::is_same_v<
			typename filter_have_all_types< type_list< type_list<char, float, int>, type_list<char, bool, double>, type_list<float, double, char> >
							, type_list< char, float>  >::type,

					 type_list< type_list<char, float, int>, type_list<float, double, char> > >,
		"The implementation of filter_have_all_types is bad");


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
	//static for: with this compile time for loop you can loop over any tuple, type list, or variadic argument list

	namespace detail {
		template <typename T, T Begin, class Func, T ...Is>
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
			static_for< int, 0, size<list>::value >([&](auto i) {
				using type = Nth_type<list, i>;
				std::cout << i << " " << typeid(type).name(); }
			);
		}
	}
}



#endif
