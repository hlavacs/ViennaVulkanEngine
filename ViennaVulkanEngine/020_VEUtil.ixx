export module VVE:VEUtil;

import std.core;
import :VETypes;


namespace vve {
	inline std::atomic<uint64_t> g_guid = 0;
}


export namespace vve {

	//----------------------------------------------------------------------------------
	//produces unique guids
	VeGuid newGuid() {
		return VeGuid( (decltype(std::declval<VeGuid>().value)) g_guid.fetch_add(1) );
	}

	//----------------------------------------------------------------------------------
	//Test whether an index for an array is within bounds

	template<typename T>
	concept has_size = requires(T t) { t.size(); };

	template<has_size T>
	bool isInArray(std::size_t index, const T& container) {
		return index < container.size();
	}

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
	//hashing of arbitrary arguments

	template<typename... Args>
	struct ve_hash {
		size_t operator()() { return 0; };
	};

	template<typename i, typename... Args>
	struct ve_hash< i, Typelist<>, Args...> {
		size_t operator()() {
			return 0;
		};
	};


	template<typename i, typename I, typename T>
	struct ve_hash< i, Typelist<I>, T> {
		size_t operator()(T const t) {
			static_assert(i == I);
			return 0;
		};
	};

	template<typename i, typename I, typename... Is, typename T, typename... Args>
	struct ve_hash<i, Typelist<I, Is...>, T, Args... > {
		size_t operator()(T const t, Args... args) {
			if constexpr (i == I) {
				return hash_combine(hash(t), ve_hash<i + 1, Typelist<Is...>, Args...>(args...));
			}
			else {
				return ve_hash<i + 1, Typelist<I, Is...>>(t, args...);
			}
		};
	};

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
	//Turn tuple of types and list of integers into tuple of instances

	template<typename T>
	concept has_getInstance = requires(T t) { t.getInstance(); };

	template<typename tuple_type, typename T>
	constexpr auto TupleOfInstances_impl() {
		T t;
		return t.getInstance<tuple_type>(); //calls Listtype::getInstance or other map type
	}

	template<typename tuple_type, typename... Ts>
	constexpr auto TupleOfInstances() {
		return std::make_tuple(TupleOfInstances_impl<tuple_type, Ts>()...); //return a tuple of instances
	}

	//----------------------------------------------------------------------------------
	
	template<int i, class Func, typename... Args>
	void callFunc(Func&& f, Args... args) {};

	template<int i, class Func, typename T >
	void callFunc(Func&& f, T t) { 
		f.template operator() < i > (t);
	};

	template<int i, class Func, typename T, typename... Args>
	void callFunc(Func&& f, T t, Args... args) { 
		f.template operator() <i>(t);
		callFunc<i+1>(f, args...);
	};





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

