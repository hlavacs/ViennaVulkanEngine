export module VVE:VEUtil;

import std.core;
import :VETypes;

export namespace vve {

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


	//----------------------------------------------------------------------------------
	//static for loop

	template <typename T, T Begin, class Func, T ...Is>
	constexpr void static_for_impl(Func&& f, std::integer_sequence<T, Is...>) {
		(f(std::integral_constant<T, Begin + Is>{ }), ...);
	}

	template <typename T, T Begin, T End, class Func >
	constexpr void static_for(Func&& f) {
		static_for_impl<T, Begin>(std::forward<Func>(f), std::make_integer_sequence<T, End - Begin>{ });
	}

	//----------------------------------------------------------------------------------
	//extract sub tuple

	template <typename T, int... Is>
	constexpr auto makeKey(T& t) {
		return std::tuple(std::get<Is>(t) ...);
	}

	template <typename T, int... Is>
	struct typed_map {

		std::map<T, int> m;
		typed_map() : m() {};

		void mapValue(T& t, int val) {
			auto key = makeKey<T, Is...>(t);
			m[key] = val;
		};
	};

	//----------------------------------------------------------------------------------
	//create tuple of arrays from tuple (experimental)

	template<class Tuple, std::size_t... Is>
	auto ToA_impl(const Tuple& t, std::index_sequence<Is...>) {
		return std::make_tuple(std::array<std::tuple_element<Is, Tuple>::type, 10>{} ...);
	}

	template<class... Args>
	auto ToA(const std::tuple<Args...>& t) {
		const uint32_t VE_TABLE_CHUNK_NUMBER = 1 << 14 / sizeof(t);
		return ToA_impl(t, std::index_sequence_for<Args...>{});
	}


	//----------------------------------------------------------------------------------
	//Turn List of List of Integers into Tuple of Lists of Integers

	template<typename T>
	constexpr auto TupleOfLists_impl() {
		T list;
		return list.getTuple();
	}

	template<typename... Ts> 
	constexpr auto TupleOfLists( ) {
		return std::make_tuple( TupleOfLists_impl<Ts>()... );
	}


	//----------------------------------------------------------------------------------
	//clock class

	class VeClock {
		std::chrono::high_resolution_clock::time_point m_last;
		uint32_t m_num_ticks = 0;
		double m_sum_time = 0;
		double m_avg_time = 0;
		double m_stat = 0;
		double f = 1.0;
		std::string m_name;

	public:
		VeClock(std::string name, double stat_time = 1.0) : m_name(name), m_stat(stat_time) {
			m_last = std::chrono::high_resolution_clock::now();
		};

		void start() {
			m_last = std::chrono::high_resolution_clock::now();
		};

		void stop() {
			auto now = std::chrono::high_resolution_clock::now();
			auto time_span = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_last);
			m_sum_time += time_span.count();
			++m_num_ticks;
			if (m_num_ticks >= m_stat) {
				double avg = m_sum_time / (double)m_num_ticks;
				m_avg_time = (1.0 - f) * m_avg_time + f * avg;
				f = f - (f - 0.9) / 100.0;
				m_sum_time = 0;
				m_num_ticks = 0;
				print();
			}
		};

		void tick() {
			auto now = std::chrono::high_resolution_clock::now();
			auto time_span = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_last);
			m_last = now;
			m_sum_time += time_span.count();
			++m_num_ticks;
			if (m_sum_time > m_stat* std::exp(9.0 * std::log(10.0))) {
				double avg = m_sum_time / m_num_ticks;
				m_avg_time = (1.0 - f) * m_avg_time + f * avg;
				f = f - (f - 0.9) / 100.0;
				m_sum_time = 0;
				m_num_ticks = 0;
				print();
			}
			m_last = std::chrono::high_resolution_clock::now();
		};

		void print() {
			std::cout << m_name << " avg " << std::setw(7) << m_avg_time / 1000000.0 << " ms" << std::endl;
		};
	};

};

