#ifndef VETYPELIST_H
#define VETYPELIST_H


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

		//-------------------------------------------------------------------------
		//make a summary variant type

		namespace detail {
			template <typename Seq>
			struct variant_type_impl;

			template <template <typename...> typename Seq, typename... Ts>
			struct variant_type_impl<Seq<Ts...>> {
				using type = std::variant<Ts...>;
			};
		}  // namespace detail

		template <typename Seq>
		using variant_type = typename detail::variant_type_impl<Seq>::type;

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

		//-------------------------------------------------------------------------
		//substitue

		namespace detail {
			template<typename List, typename Fun>
			struct substitute_impl;

			template<template <typename...> typename Seq, typename... Ts, template<typename...> typename Fun>
			struct substitute_impl<Seq<Ts...>, Fun> {
				using type = Fun<Ts...>;
			};
		}
		template <typename Seq, typename Fun>
		using substitute = typename detail::substitute_impl<Seq, Fun>::type;

		//-------------------------------------------------------------------------
		//transfer a list of types1 into a list of types2

		namespace detail {
			template<typename List, typename Fun>
			struct transfer_impl;

			template<template <typename...> typename Seq, template<typename...> typename Fun>
			struct transfer_impl<Seq<>, Fun> {
				using type = type_list<>;
			};

			template<template <typename...> typename Seq, typename T, typename... Ts, template<typename...> typename Fun>
			struct transfer_impl<Seq<T, Ts...>, Fun> {
				//using type = cat< type_list< substitute<T, Fun> >, transfer_impl< Seq<Ts...>, Fun> >;
				using type1 = type_list< substitute<T, Fun> >;
				using type2 = transfer_impl< Seq<Ts...>, Fun>;
				using type = cat< type1, type2>;
			};
		}
		template <typename Seq, typename Fun>
		using transfer = typename detail::transfer_impl<Seq, Fun>::type;

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

