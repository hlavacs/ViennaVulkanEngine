#ifndef VETYPELIST_H
#define VETYPELIST_H

#include <tuple>

namespace vve {

	namespace tl {
		//https://nilsdeppe.com/posts/tmpl-part2
		//https://ericniebler.com/2014/11/13/tiny-metaprogramming-library/

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
		//size

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

		//using result = size<typelist<double, bool>>;
		//result::value == 2

		//-------------------------------------------------------------------------
		//front

		namespace detail {
			template <typename Seq>
			struct front_impl;

			template <template <typename...> typename Seq, typename T, typename... Ts>
			struct front_impl<Seq<T, Ts...>> {
				using type = T;
			};
		}  

		template <typename Seq>
		using front = typename detail::front_impl<Seq>::type;

		static_assert(
			std::is_same<front<type_list<double, char, bool, double>>, double>::value,
			"The implementation of front is bad");

		//-------------------------------------------------------------------------
		//pop front

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

		static_assert(std::is_same<pop_front<type_list<double, char, bool, double>>,
			type_list<char, bool, double>>::value,
			"The implementation of pop_front is bad");

		//-------------------------------------------------------------------------
		//push front

		namespace detail {
			template <typename Seq, typename T>
			struct push_front_impl;

			template <template <typename...> typename Seq, typename T, typename... Ts>
			struct push_front_impl<Seq<Ts...>, T> {
				using type = Seq<T, Ts...>;
			};
		}  

		template <typename Seq, typename T>
		using push_front = typename detail::push_front_impl<Seq, T>::type;

		static_assert(
			std::is_same<push_front<type_list<double, char, bool, double>, char>,
			type_list<char, double, char, bool, double>>::value,
			"The implementation of push_front is bad");

		//-------------------------------------------------------------------------
		//Nth type element

		namespace detail {
			template <int N, typename Seq>
			struct Nth_type_impl;

			template <int N, template <typename...> typename Seq, typename... Ts>
			struct Nth_type_impl<N, Seq<Ts...>> {
				using type = typename std::tuple_element<N, std::tuple<Ts...>>::type;
			};
		}  

		template <int N, typename Seq>
		using Nth_type = typename detail::Nth_type_impl<N, Seq>::type;

		static_assert(
			std::is_same<Nth_type<1, type_list<double, char, bool, double>>, char>::value,
			"The implementation of Nth_type is bad");

		//-------------------------------------------------------------------------
		//size of

		namespace detail {
			template <typename Seq>
			struct size_of_impl;

			template< template <typename...> typename Seq>
			struct size_of_impl<Seq<>> {
				using type = std::integral_constant<std::size_t, 0>;
			};

			template< template <typename...> typename Seq, typename... Ts>
			struct size_of_impl<Seq<Ts...>> {
				using type = std::integral_constant<std::size_t, sizeof...(Ts)>;
			};
		}

		template <typename Seq>
		using size_of = typename detail::size_of_impl<Seq>::type;

		static_assert(
			size_of<type_list<double, char, bool, double>>::value == 4,
			"The implementation of size_of is bad");

		//-------------------------------------------------------------------------
		//index of

		namespace detail {
			template<typename, typename>
			struct index_of_impl {};

			// Index Of base case: found the type we're looking for.
			template <typename T, template <typename...> typename Seq, typename... Ts>
			struct index_of_impl<T, Seq<T, Ts...>> : std::integral_constant<std::size_t, 0> {
				using type = std::integral_constant<std::size_t, 0>;
			};

			// Index Of recursive case: 1 + Index Of the rest of the types.
			template <typename T, typename TOther, template <typename...> typename Seq, typename... Ts>
			struct index_of_impl<T, Seq<TOther, Ts...>>
				: std::integral_constant<std::size_t, 1 + index_of_impl<T, Seq<Ts...>>::value>
			{
				using type = std::integral_constant<std::size_t, 1 + index_of_impl<T, Seq<Ts...>>::value>;
			};
		}

		template <typename T, typename Seq>
		using index_of = typename detail::index_of_impl<T, Seq>::type;

		static_assert(index_of< char, type_list<double, char, bool, double> >::value == 1);

		static_assert(
			std::is_same_v< index_of< char, type_list<double, char, bool, double> >, std::integral_constant<std::size_t, 1> >,
			"The implementation of index_of is bad");

		//-------------------------------------------------------------------------
		//concatenate 

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
			std::is_same_v< cat< type_list<double, int>, type_list<char, float> >, type_list<double, int, char, float> >,
			"The implementation of cat is bad");

		//-------------------------------------------------------------------------
		//turn elements into pointers

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
		//make a summary variant type

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
		//transform a list of types into a list of F<types>

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
			std::is_same_v< transform< type_list<double, int>, type_list >, type_list<type_list<double>, type_list<int>> >,
			"The implementation of transform is bad");

		//-------------------------------------------------------------------------
		//substitue

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
		//transfer a list of types1 into a list of types2

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
		//test if a list is the same as given types

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
		//turn a list into a tuple

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
		//turn a list into a tuple of references type

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
		//turn a list into a tuple of pointer type

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
		//check whether a tuple contains a type

		namespace detail {
			template<typename Seq, typename T>
			struct has_type_impl;

			template<template <typename...> class Seq, typename T>
			struct has_type_impl<Seq<>,T> {
				static const bool value = false;
			};

			template<template <typename...> class Seq, typename... Ts, typename T>
			struct has_type_impl<Seq<Ts...>, T> {
				static const bool value = (std::is_same_v<T, Ts> || ...);
			};
		}
		template <typename Seq, typename T>
		struct has_type {
			static const bool value = detail::has_type_impl<Seq, T>::value;
		};

		static_assert( has_type<type_list<double, int, char>, char>::value, "The implementation of has_type is bad");
		static_assert( !has_type<type_list<double, int, char>, float>::value, "The implementation of has_type is bad");

		//-------------------------------------------------------------------------
		//turn a list into a tuple of T type with lenght N

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
		//static for

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

		//static_for<int, -3, 3 >([&](auto i) { cout << i << ", "; });
	}


}


#endif

