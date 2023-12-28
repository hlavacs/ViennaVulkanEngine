#pragma once

#include <limits>
#include <utility>

namespace vsty {

	template<typename T>
	concept Hashable = requires(T a) {
		{ std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
	};

	/**
	* \brief General strong type
	*
	* T...the type
	* P...phantom type as unique ID (can use __COUNTER__ or vsty::counter<>)
	*/
    template<typename T, auto P >
    struct strong_type_t {
        strong_type_t() noexcept = default;									//default constructible
        explicit strong_type_t(const T& v) noexcept { m_value = v; };			//explicit from type T
        explicit strong_type_t(T&& v) noexcept { m_value = std::move(v); };	//explicit from type T

        strong_type_t( strong_type_t<T, P> const &) noexcept = default;		//copy constructible
        strong_type_t( strong_type_t<T, P>&&) noexcept = default;			//move constructible

        strong_type_t<T, P>& operator=(T const& v) noexcept { m_value = v; return *this; };		//copy assignable from type T
        strong_type_t<T, P>& operator=(T&& v) noexcept { m_value = std::move(v); return *this; };	//copy assignable from type T

        strong_type_t<T, P>& operator=(strong_type_t<T, P> const&) noexcept = default;	//move assignable
        strong_type_t<T, P>& operator=(strong_type_t<T, P>&&) noexcept = default;			//move assignable

		T& value() { return m_value; }
		operator const T& () const noexcept { return m_value; }	//retrieve m_value
		operator T& () noexcept { return m_value; }				//retrieve m_value

		auto operator<=>(const strong_type_t<T, P>& v) const = default;
	
		struct equal_to {
			constexpr bool operator()(const T& lhs, const T& rhs) const noexcept requires std::equality_comparable<std::decay_t<T>> { return lhs == rhs; };
		};
		
        struct hash {
            std::size_t operator()(const strong_type_t<T, P>& tag) const noexcept requires Hashable<std::decay_t<T>> { return std::hash<T>()(tag.m_value); };
        };

	protected:
		T m_value{};
	};


	/**
	* \brief Strong type with a null m_value
	*
	* T...the type
	* D...default m_value (=null m_value)
	* P...phantom type as unique ID (can use __COUNTER__ or vsty::counter<>)
	*/	
	template<typename T, auto P, auto D>
	struct strong_type_null_t : strong_type_t<T, P> {
		using strong_type_t<T,P>::m_value;
		static const T null{D};
		strong_type_null_t() { m_value = D; };
		explicit strong_type_null_t(const T& v) : strong_type_t<T,P>(v) {};
		strong_type_null_t<T, P, D>& operator=(T const& v) noexcept { m_value = v; return *this; };		//copy assignable from type T
		strong_type_null_t<T, P, D>& operator=(T&& v) noexcept { m_value = std::move(v); return *this; };	//copy assignable from type T
		bool has_value() const noexcept { return m_value != D; }
	};


	/**
	* \brief Strong integral basis type, like size_t or uint32_t. 
	* Can be split into three integral m_values (upper, middle and lower).
	* NOTE: This works ONLY if the integral type is unsigned!!
	* NOTE: This is a basis class that uses CRTP to determine the m_value type.
	*
	* T...the integer type
	* P...phantom type as unique ID (can use __COUNTER__ or vsty::counter<>)
	* U...number of upper bits (if integer is cut into 2 m_values), or else 0
	* M...number of middle bits (if integer is cut into 3 m_values), or else 0
	*/
	template<typename T, typename IT, auto P, size_t U = 0, size_t M = 0>
		requires std::is_integral_v<std::decay_t<T>>
	struct strong_integral_tt : strong_type_t<T, P>  {

		static const size_t BITS = sizeof(T) * 8ull;
		static_assert(BITS >= U+M);

		static const size_t L = BITS - U - M; //number of lower bits (if integer is cut into 2/3 m_values)

		static consteval T lmask() {
			if constexpr (L == 0) return static_cast<T>(0ull);
			else if constexpr (U == 0 && M == 0) return static_cast<T>(~0ull);
			else return static_cast<T>(~0ull) >> (BITS - L);
		}

		static consteval T umask() {
			if constexpr (U == 0) return static_cast<T>(0ull);
			else if constexpr (M == 0 && L == 0) return static_cast<T>(~0ull);
			else return static_cast<T>(~0ull) << (BITS - U);
		}

		static const T LMASK = lmask();
		static const T UMASK = umask();
		static const T MMASK = ~(LMASK | UMASK);

		using strong_type_t<T, P>::m_value;

		strong_integral_tt() noexcept = default;											//default constructible
		explicit strong_integral_tt(const T& v) noexcept : strong_type_t<T, P>(v) {};	//explicit from type T
		explicit strong_integral_tt(T&& v) noexcept : strong_type_t<T, P>(v) {};			//explicit from type T

		strong_integral_tt(strong_integral_tt<T, IT, P, U, M> const&) noexcept = default;	//copy constructible
		strong_integral_tt(strong_integral_tt<T, IT, P, U, M>&& v) noexcept = default;	//move constructible

		strong_integral_tt<T, IT, P, U, M>& operator=(T const& v) noexcept { m_value = v; return *this; };			//copy assignable
		strong_integral_tt<T, IT, P, U, M>& operator=(T&& v) noexcept { m_value = std::move(v); return *this; };	//copy assignable

		strong_integral_tt<T, IT, P, U, M>& operator=(strong_integral_tt<T, IT, P, U, M> const&) noexcept = default;	//move assignable
		strong_integral_tt<T, IT, P, U, M>& operator=(strong_integral_tt<T, IT, P, U, M>&&) noexcept = default;		//move assignable

		operator const T& () const noexcept { return m_value; }	//retrieve m_value
		operator T& () noexcept { return m_value; }				//retrieve m_value

		//-----------------------------------------------------------------------------------

		auto operator<=>(const strong_integral_tt<T, IT, P, U, M>& v) const = default;

		IT operator+(strong_integral_tt<T, IT, P, U, M> const& r) { return IT{ m_value + r.m_value }; };
		IT operator-(strong_integral_tt<T, IT, P, U, M> const& r) { return IT{ m_value - r.m_value }; };
		IT operator*(strong_integral_tt<T, IT, P, U, M> const& r) { return IT{ m_value* r.m_value }; };
		IT operator/(strong_integral_tt<T, IT, P, U, M> const& r) { return IT{ m_value / r.m_value }; };

		IT operator<<(const size_t N) noexcept { return IT{ m_value << N }; };
		IT operator>>(const size_t N) noexcept { return IT{ m_value >> N }; };
		IT operator&(const size_t N) noexcept { return IT{ m_value& N }; };

		IT operator++() noexcept { ++m_value; IT it{ *this }; return it; };
		IT operator++(int) noexcept { IT it(m_value++); return it; };
		IT operator--() noexcept { --m_value; IT it{ *this }; return it; };
		IT operator--(int) noexcept { IT it(m_value--); return it; };

		auto set_upper(T v) noexcept requires std::is_unsigned_v<std::decay_t<T>> { if constexpr (U > 0) { m_value = (m_value & (LMASK | MMASK)) | ( (v << (L+M)) & UMASK); } } 
		auto get_upper()    noexcept requires std::is_unsigned_v<std::decay_t<T>> { if constexpr (U > 0) { return m_value >> (L+M); } return static_cast<T>(0); }	
		auto set_middle(T v) noexcept requires std::is_unsigned_v<std::decay_t<T>> { if constexpr (M > 0) { m_value = (m_value & (LMASK | UMASK)) | ((v << L) & MMASK); } }
		auto get_middle()    noexcept requires std::is_unsigned_v<std::decay_t<T>> { if constexpr (M > 0) { return (m_value & MMASK) >> L; } return static_cast<T>(0); }
		auto set_lower(T v) noexcept requires std::is_unsigned_v<std::decay_t<T>> { m_value = (m_value & (UMASK | MMASK)) | (v & LMASK); }
		auto get_lower()    noexcept requires std::is_unsigned_v<std::decay_t<T>> { return m_value & LMASK; }
	};

	/**
	* \brief Strong integral type.
	*
	* T...the type
	* P...phantom type as unique ID (can use __COUNTER__ or vsty::counter<>)
	* U...number of upper bits (if integer is cut into 2 m_values), or else 0
	* M...number of middle bits (if integer is cut into 3 m_values), or else 0
	*/
	template<typename T, auto P, size_t U = 0, size_t M = 0>
		requires std::is_integral_v<std::decay_t<T>>
	struct strong_integral_t : strong_integral_tt<T, strong_integral_t<T, P, U, M>, P, U, M> {
		strong_integral_t() noexcept = default;											//default constructible
		explicit strong_integral_t(const T& v) noexcept : strong_integral_tt<T, strong_integral_t<T, P, U, M>, P, U, M>(v) {};	//explicit from type T
		explicit strong_integral_t(T&& v)      noexcept : strong_integral_tt<T, strong_integral_t<T, P, U, M>, P, U, M>(v) {};	//explicit from type T
	};

	/**
	* \brief Strong integral type with a null m_value. 
	*
	* T...the type
	* P...phantom type as unique ID (can use __COUNTER__ or vsty::counter<>)
	* U...number of upper bits (if integer is cut into 2 m_values), or else 0
	* M...number of middle bits (if integer is cut into 3 m_values), or else 0
	* D...default m_value (=null m_value)
	*/
	template<typename T, auto P, size_t U = 0, size_t M = 0, auto D = std::numeric_limits<T>::max()>
		requires std::is_integral_v<std::decay_t<T>>
	struct strong_integral_null_t : strong_integral_tt<T, strong_integral_null_t<T, P, U, M, D>, P, U, M> {
		using strong_integral_tt<T, strong_integral_null_t<T, P, U, M, D>, P, U, M>::m_value;
		static const T null{ D };

		strong_integral_null_t() noexcept { m_value = D; };  //NOT default constructible
		explicit strong_integral_null_t(const T& v) noexcept : strong_integral_tt<T, strong_integral_null_t<T, P, U, M, D>, P, U, M>(v) {};	//explicit from type T
		explicit strong_integral_null_t(T&& v)      noexcept : strong_integral_tt<T, strong_integral_null_t<T, P, U, M, D>, P, U, M>(v) {}; //explicit from type T

		strong_integral_null_t<T, P, U, M, D>& operator=(T const& v) noexcept { m_value = v; return *this; };		//copy assignable from type T
		strong_integral_null_t<T, P, U, M, D>& operator=(T&& v) noexcept { m_value = std::move(v); return *this; };	//copy assignable from type T

		bool has_value() const noexcept { return m_value != D; }
	};

	//Helper functions to test with basic integer types instead of strong types
	template<typename T>
	T null_value() {
		if constexpr (std::is_integral_v<std::decay_t<T>>) return std::numeric_limits<T>::max();
		else return T{ T::null };
	}

	template<typename T>
	bool has_value(T value) {
		if constexpr (std::is_integral_v<std::decay_t<T>>) return value != null_value<T>();
		else return value.has_value(); 
	}



	//--------------------------------------------------------------------------------------------
	//type counter lifted from https://mc-deltat.github.io/articles/stateful-metaprogramming-cpp20

	template<size_t N>
	struct reader { friend auto counted_flag(reader<N>); };

	template<size_t N>
	struct setter {
		friend auto counted_flag(reader<N>) {}
		static constexpr size_t n = N;
	};

	template< auto Tag, size_t NextVal = 0 >
	[[nodiscard]] consteval auto counter_impl() {
		constexpr bool counted_past_m_value = requires(reader<NextVal> r) { counted_flag(r); };

		if constexpr (counted_past_m_value) {
			return counter_impl<Tag, NextVal + 1>();
		}
		else {
			setter<NextVal> s;
			return s.n;
		}
	}

	template< auto Tag = [] {}, auto Val = counter_impl<Tag>() >
	constexpr auto counter = Val;

}


