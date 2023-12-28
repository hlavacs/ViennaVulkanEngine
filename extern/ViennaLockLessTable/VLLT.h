#pragma once

#include <assert.h>
#include <algorithm>
#include <memory_resource>
#include <shared_mutex>
#include <optional>
#include <array>
#include <stack>
#include <concepts>
#include <algorithm>
#include <type_traits>

#include "VTLL.h"
#include "VSTY.h"

//todo: partition table indices into state/counter, turn spinlock into lockless with state/counter, align atomics, segment allocation on demand not all alloc when constructing, also pay constr/destr costs, lockless segment cache
//read-write locking segments: push - dont have to do anything (even if other are read/writing last segment), pop - write lock the last segment

namespace vllt {


	/////
	// \brief VlltTable is the base class for some classes, enabling management of tables that can be
	// appended in parallel.
	//

	template<typename DATA, size_t N0 = 1 << 10, bool ROW = true, size_t SLOTS = 16>
	class VlltTable {
	protected:
		static_assert(std::is_default_constructible_v<DATA>, "Your components are not default constructible!");

		using table_index_t = vsty::strong_type_null_t<size_t, vsty::counter<>, std::numeric_limits<size_t>::max()>;
		using segment_idx_t = vsty::strong_integral_t<size_t, vsty::counter<>>; ///<strong integer type for indexing segments, 0 to size vector-1

		static const size_t N = vtll::smallest_pow2_leq_value< N0 >::value;									///< Force N to be power of 2
		static const size_t L = vtll::index_largest_bit< std::integral_constant<size_t, N> >::value - 1;	///< Index of largest bit in N
		static const uint64_t BIT_MASK = N - 1;			///< Bit mask to mask off lower bits to get index inside segment

		using tuple_value_t = vtll::to_tuple<DATA>;		///< Tuple holding the entries as value
		using tuple_ref_t = vtll::to_ref_tuple<DATA>;	///< Tuple holding ptrs to the entries

		using array_tuple_t1 = std::array<tuple_value_t, N>;								///< ROW: an array of tuples
		using array_tuple_t2 = vtll::to_tuple<vtll::transform_size_t<DATA, std::array, N>>;	///< COLUMN: a tuple of arrays
		using segment_t = std::conditional_t<ROW, array_tuple_t1, array_tuple_t2>;			///< Memory layout of the table

		using segment_ptr_t = std::shared_ptr<segment_t>;						///<Shared pointer to a segment
		struct segment_vector_t {
			std::pmr::vector<segment_ptr_t> m_segments;	///<Vector of shared pointers to the segments
			segment_idx_t m_seg_offset = 0;								///<Segment offset for FIFO queue (offsets segments NOT rows)
		};

	public:
		VlltTable(size_t r = 1 << 16, std::pmr::memory_resource* mr = std::pmr::new_delete_resource()) noexcept
			: m_mr{ mr }, m_allocator{ mr }, m_seg_vector{ nullptr } {};
		~VlltTable() noexcept {};

		/// <summary>
		/// Return a pointer to a component.
		/// </summary>
		/// <typeparam name="C">Type of component.</typeparam>
		/// <typeparam name="I">Index in the component list.</typeparam>
		/// <param name="n">Slot number in the table.</param>
		/// <returns>Pointer to the component.</returns>
		template<size_t I, typename C = vtll::Nth_type<DATA, I>>
		inline auto component_ptr(table_index_t n, std::shared_ptr<segment_vector_t>& vector_ptr) noexcept -> C* {		///< \returns a pointer to a component
			auto idx = segment(n, vector_ptr);
			auto segment_ptr = (vector_ptr->m_segments[idx]); // .load();	///< Access the segment holding the slot
			if constexpr (ROW) { return &std::get<I>((*segment_ptr)[n & BIT_MASK]); }
			else { return &std::get<I>(*segment_ptr)[n & BIT_MASK]; }
		}

		/// <summary>
		/// Insert a new row at the end of the table. Make sure that there are enough segments to store the new data.
		/// If not allocate a new vector to hold the segements, and allocate new segments.
		/// </summary>
		/// <param name="slot">Slot number in the table.</param>
		/// <param name="vector_ptr">Shared pointer to the segment vector.</param>
		/// <param name="first_seg">Index of first segment that currently holds information.</param>
		/// <param name="last_seg">Index of the last segment that currently holds informaton.</param>
		/// <param name="data">The data for the new row.</param>
		template<typename... Cs>
			requires std::is_same_v<vtll::tl<std::decay_t<Cs>...>, vtll::remove_atomic<DATA>>
		void insert(table_index_t slot, std::atomic<table_index_t>* first_slot, Cs&&... data) {
			auto vector_ptr = resize(slot, first_slot); //if need be, grow the vector of segments

			//copy or move the data to the new slot, using a recursive templated lambda
			auto f = [&]<size_t I, typename T, typename... Ts>(auto && fun, T && dat, Ts&&... dats) {
				if constexpr (vtll::is_atomic<T>::value) component_ptr<I>(slot, vector_ptr)->store(dat);	//copy value for atomic
				else *component_ptr<I>(slot, vector_ptr) = std::forward<T>(dat);							//move or copy
				if constexpr (sizeof...(dats) > 0) { fun.template operator() < I + 1 > (fun, std::forward<Ts>(dats)...); } //recurse
			};
			f.template operator() < 0 > (f, std::forward<Cs>(data)...);
		}

	protected:

		/// <summary>
		/// Return the segment index for a given slot.
		/// </summary>
		/// <param name="n">Slot.</param>
		/// <param name="offset">Segment offset to be used.</param>
		/// <returns>Index of the segment for the slot.</returns>
		static auto segment(table_index_t n, size_t offset) {
			return segment_idx_t{ (n >> L) - offset };
		}

		/// <summary>
		/// Return the segment index for a given slot.
		/// </summary>
		/// <param name="n">Slot.</param>
		/// <param name="vector_ptr">Pointer to the vector of segments, and offset</param>
		/// <returns>Index of the segment for the slot.</returns>
		static auto segment(table_index_t n, std::shared_ptr<segment_vector_t>& vector_ptr) {
			return segment(n, vector_ptr->m_seg_offset );
		}

		/// <summary>
		/// Transfer segments that have been unnessearily allocated to a global cache.
		/// </summary>
		void put_cache_cache( auto& vector_ptr ) {
			std::scoped_lock lock(m_mutex);
			while (segment_cache_cache.size()) {
				if(segment_cache.size() < vector_ptr->m_segments.size()) segment_cache.emplace(segment_cache_cache.top());
				segment_cache_cache.pop();
			}
		}

		void put_cache(auto&& ptr, auto& vector_ptr) {
			std::scoped_lock lock(m_mutex);
			if (segment_cache.size() < vector_ptr->m_segments.size()) segment_cache.emplace(ptr);
		}

		/// <summary>
		/// Get a new segment - either from the global cache or allocate a new one.
		/// 
		/// This might be an unnessesary allocation, so also put it into a temp cache cache.
		/// </summary>
		/// <returns></returns>
		auto get_cache() {
			std::scoped_lock lock(m_mutex);
			segment_ptr_t ptr;
			if (!segment_cache.size()) {
				ptr = std::make_shared<segment_t>();
			}
			else {
				ptr = segment_cache.top();
				segment_cache.pop();
			}
			segment_cache_cache.emplace(ptr);
			return ptr;
		}

		/// <summary>
		/// Allocations were useful, so clear the temp cache cache.
		/// </summary>
		void clear_cache_cache() {
			while (segment_cache_cache.size()) segment_cache_cache.pop();
		}

		/// <summary>
		/// If the vector of segments is too small, allocate a larger one and copy the previous segment pointers into it.
		/// Then make one CAS attempt. If the attempt succeeds, then remember the new segment vector.
		/// If the CAS fails because another thread beat us, then CAS will copy the new pointer so we can use it.
		/// </summary>
		/// <param name="slot">Slot number in the table.</param>
		/// <param name="vector_ptr">Shared pointer to the segment vector.</param>
		/// <param name="first_seg">Index of first segment that currently holds information.</param>
		/// <returns></returns>
		inline auto resize(table_index_t slot, std::atomic<table_index_t>* first_slot = nullptr) {
			auto vector_ptr{ m_seg_vector.load() };

			if (!vector_ptr) {
				auto new_vector_ptr = std::make_shared<segment_vector_t>( //vector has always as many slots as its capacity is -> size==capacity
					segment_vector_t{ std::pmr::vector<segment_ptr_t>{SLOTS, m_mr}, segment_idx_t{ 0 } } //increase existing one
				);
				for (auto& ptr : new_vector_ptr->m_segments) {
					ptr = std::make_shared<segment_t>();
				}
				if (m_seg_vector.compare_exchange_strong(vector_ptr, new_vector_ptr)) {	///< Try to exchange old segment vector with new
					vector_ptr = new_vector_ptr;										///< If success, remember for later
				} //Note: if we were beaten by other thread, then compare_exchange_strong itself puts the new value into vector_ptr
			}

			auto sz = vector_ptr->m_segments.size() / 16;
			double rnd = (rand() % 1000) / 1000.0;
			//if (rnd > 0.2 && segment_cache.size() < vector_ptr->m_segments.size()) { put_cache(std::make_shared<segment_t>(), vector_ptr); }
			auto f = sz* rnd - sz;
			while ( slot >= N * (vector_ptr->m_segments.size() + vector_ptr->m_seg_offset + f)) {
				f = 0.0;

				segment_idx_t first_seg{ 0 };
				auto fs = first_slot ? first_slot->load() : table_index_t{0};
				if (vsty::has_value(fs)) {
					first_seg = segment(fs, vector_ptr);
				}

				size_t num_segments = vector_ptr->m_segments.size();
				size_t new_offset = vector_ptr->m_seg_offset + first_seg;
				size_t min_size = segment(slot, new_offset);
				size_t smaller_size = std::max((num_segments >> 2), SLOTS);
				size_t medium_size = std::max((num_segments >> 1), SLOTS);
				size_t new_size = num_segments + (num_segments >> 1);
				while (min_size > new_size) { new_size *= 2; }

				if (first_seg > num_segments * 0.85 && min_size < smaller_size) { new_size = smaller_size; }
				else if (first_seg > num_segments * 0.65 && min_size < medium_size) { new_size = medium_size; }
				else if (first_seg > (num_segments >> 1) && min_size < num_segments) new_size = num_segments;

				//std::scoped_lock lock(m_allocate);

				auto vector_ptr2 = m_seg_vector.load();
				if (vector_ptr != vector_ptr2) {
					vector_ptr = vector_ptr2;
					continue;
				}

				auto new_vector_ptr = std::make_shared<segment_vector_t>( //vector has always as many slots as its capacity is -> size==capacity
					segment_vector_t{ std::pmr::vector<segment_ptr_t>{new_size, m_mr}, segment_idx_t{ new_offset } } //increase existing one
				);

				size_t idx = 0;
				size_t remain = num_segments - first_seg;


				std::ranges::for_each(new_vector_ptr->m_segments.begin(), new_vector_ptr->m_segments.end(), [&](auto& ptr) {
					if (first_seg + idx < num_segments) { ptr = vector_ptr->m_segments[first_seg + idx]; } 
					else {
						size_t i1 = idx - remain;
						if (i1 < first_seg) { ptr = vector_ptr->m_segments[i1]; }
						else { ptr = get_cache(); }
					}
					++idx;
				});

				//m_seg_vector.store( new_vector_ptr );
				//put_cache_cache(vector_ptr);
				//clear_cache_cache();

				if (m_seg_vector.compare_exchange_strong(vector_ptr, new_vector_ptr)) {	///< Try to exchange old segment vector with new

					//also reuse segments that we did not reuse because vector was shrunk
					auto reused = (int64_t)new_vector_ptr->m_segments.size() - (int64_t)remain;
					auto unused = (int64_t)vector_ptr->m_segments.size() - (int64_t)new_vector_ptr->m_segments.size();
					for (int64_t i = 0; i < unused; ++i) {
						put_cache(vector_ptr->m_segments[i + reused], vector_ptr); //these segments are left since we are shrinking
					}

					vector_ptr = new_vector_ptr;										///< If success, remember for later

					clear_cache_cache();  //we used those for the new segment
				} //Note: if we were beaten by other thread, then compare_exchange_strong itself puts the new value into vector_ptr
				else {
					put_cache_cache(vector_ptr); //we were beaten, so save the new segments in the global cache
				}
			}

			return vector_ptr;
		}

		std::pmr::memory_resource* m_mr;					///< Memory resource for allocating segments
		std::pmr::polymorphic_allocator<segment_vector_t>	m_allocator;			///< use this allocator
		std::atomic<std::shared_ptr<segment_vector_t>>		m_seg_vector;			///< Atomic shared ptr to the vector of segments

		std::mutex m_mutex;
		std::stack<segment_ptr_t> segment_cache;
		static inline thread_local std::stack<segment_ptr_t> segment_cache_cache;

		std::mutex m_allocate;
	};


	//---------------------------------------------------------------------------------------------------


	using stack_index_t = vsty::strong_type_null_t<uint32_t, vsty::counter<>, std::numeric_limits<uint32_t>::max()>;


	/////
	// \brief VlltStack is a data container similar to std::vector, but with additional properties
	//
	// VlltStack has the following properties:
	// 1) It stores tuples of data
	// 2) The memory layout is cache-friendly and can be row-oriented or column-oriented.
	// 3) Lockless multithreaded access. It can grow - by calling push_back() - even when
	// used with multiple threads. This is achieved by storing data in segments,
	// which are accessed over via a std::vector of shared_ptr. New segments can simply be added to the
	// std::vector. Also the std::vector can seamlessly grow using CAS.
	// Is can also shrink when using multithreading by calling pop_back(). Again, no locks are used!
	//
	// The number of items S per segment must be a power of 2 : N = 2^L. This way, random access to row K is esily achieved
	// by first right shift K >> L to get the index of the segment pointer, then use K & (N-1) to get the index within
	// the segment.
	//
	///
	template<typename DATA, size_t N0 = 1 << 10, bool ROW = true, size_t SLOTS = 16>
	class VlltStack : public VlltTable<DATA, N0, ROW, SLOTS> {

	public:
		using VlltTable<DATA, N0, ROW, SLOTS>::N;
		using VlltTable<DATA, N0, ROW, SLOTS>::L;
		using VlltTable<DATA, N0, ROW, SLOTS>::m_mr;
		using VlltTable<DATA, N0, ROW, SLOTS>::m_seg_vector;
		using VlltTable<DATA, N0, ROW, SLOTS>::component_ptr;
		using VlltTable<DATA, N0, ROW, SLOTS>::insert;

		using tuple_opt_t = std::optional< vtll::to_tuple< vtll::remove_atomic<DATA> > >;
		using typename VlltTable<DATA, N0, ROW, SLOTS>::table_index_t;
		using typename VlltTable<DATA, N0, ROW, SLOTS>::tuple_ref_t;
		using typename VlltTable<DATA, N0, ROW, SLOTS>::segment_t;
		using typename VlltTable<DATA, N0, ROW, SLOTS>::segment_ptr_t;
		using typename VlltTable<DATA, N0, ROW, SLOTS>::segment_vector_t;

		VlltStack(size_t r = 1 << 16, std::pmr::memory_resource* mr = std::pmr::new_delete_resource()) noexcept;

		//-------------------------------------------------------------------------------------------
		//read data

		inline auto size() noexcept -> size_t; ///< \returns the current numbers of rows in the table

		template<size_t I>
		inline auto get(stack_index_t n) noexcept -> std::optional<std::reference_wrapper<vtll::Nth_type<DATA, I>>>;		///< \returns a ref to a component

		template<typename C>									///< \returns a ref to a component
		inline auto get(stack_index_t n) noexcept -> std::optional<std::reference_wrapper<C>> requires vtll::unique<DATA>::value;

		inline auto get_tuple(stack_index_t n) noexcept	-> std::optional<tuple_ref_t>;	///< \returns a tuple with refs to all components

		//-------------------------------------------------------------------------------------------
		//add data

		template<typename... Cs>
			requires std::is_same_v<vtll::tl<std::decay_t<Cs>...>, vtll::remove_atomic<DATA>>
		inline auto push(Cs&&... data) noexcept -> stack_index_t;	///< Push new component data to the end of the table

		//-------------------------------------------------------------------------------------------
		//remove and swap data

		inline auto pop() noexcept		-> tuple_opt_t;	///< Remove the last row, call destructor on components
		inline auto clear() noexcept	-> size_t;	///< Set the number if rows to zero - effectively clear the table, call destructors
		inline auto swap(stack_index_t n1, stack_index_t n2) noexcept -> void;	///< Swap contents of two rows

	protected:

		struct slot_size_t {
			stack_index_t m_next_free_slot{ 0 };	//index of next free slot
			stack_index_t m_size{ 0 };				//number of valid entries
		};

		std::atomic<slot_size_t> m_size_cnt{ {stack_index_t{0}, stack_index_t{ 0 }} };	///< Next slot and size as atomic

		inline auto max_size() noexcept -> size_t;
	};

	/////
	// \brief Constructor of class VlltStack.
	// \param[in] r Max number of rows that can be stored in the table.
	// \param[in] mr Memory allocator.
	///
	template<typename DATA, size_t N0, bool ROW, size_t SLOTS>
	inline VlltStack<DATA, N0, ROW, SLOTS>::VlltStack(size_t r, std::pmr::memory_resource* mr) noexcept : VlltTable<DATA, N0, ROW, SLOTS>(r, mr) {};

	/////
	// \brief Return number of rows when growing including new rows not yet established.
	// \returns number of rows when growing including new rows not yet established.
	///
	template<typename DATA, size_t N0, bool ROW, size_t SLOTS>
	inline auto VlltStack<DATA, N0, ROW, SLOTS>::max_size() noexcept -> size_t {
		auto size = m_size_cnt.load();
		return std::max(size.m_next_free_slot, size.m_size);
	};

	/////
	// \brief Return number of valid rows.
	// \returns number of valid rows.
	///
	template<typename DATA, size_t N0, bool ROW, size_t SLOTS>
	inline auto VlltStack<DATA, N0, ROW, SLOTS>::size() noexcept -> size_t {
		auto size = m_size_cnt.load();
		return std::min(size.m_next_free_slot, size.m_size);
	};

	/////
	// \brief Get a pointer to a particular component with index I.
	// \param[in] n Index to the entry.
	// \returns a pointer to the Ith component of entry n.
	///
	template<typename DATA, size_t N0, bool ROW, size_t SLOTS>
	template<size_t I>
	inline auto VlltStack<DATA, N0, ROW, SLOTS>::get(stack_index_t n) noexcept -> std::optional<std::reference_wrapper<vtll::Nth_type<DATA, I>>> {
		if (n >= size()) return std::nullopt;
		auto vector_ptr = m_seg_vector.load();
		return { *component_ptr<I>(table_index_t{n}, vector_ptr) };
	};

	/////
	// \brief Get a pointer to a particular component with index I.
	// \param[in] n Index to the entry.
	// \returns a pointer to the Ith component of entry n.
	///
	template<typename DATA, size_t N0, bool ROW, size_t SLOTS>
	template<typename C>
	inline auto VlltStack<DATA, N0, ROW, SLOTS>::get(stack_index_t n) noexcept -> std::optional<std::reference_wrapper<C>> requires vtll::unique<DATA>::value {
		if (n >= size()) return std::nullopt;
		auto vector_ptr = m_seg_vector.load();
		return { *component_ptr<vtll::index_of<DATA, C>::value>(table_index_t{n}, vector_ptr) };
	};

	/////
	// \brief Get a tuple with pointers to all components of an entry.
	// \param[in] n Index to the entry.
	// \returns a tuple with pointers to all components of entry n.
	///
	template<typename DATA, size_t N0, bool ROW, size_t SLOTS>
	inline auto VlltStack<DATA, N0, ROW, SLOTS>::get_tuple(stack_index_t n) noexcept -> std::optional<tuple_ref_t> {
		if (n >= size()) return std::nullopt;
		auto vector_ptr = m_seg_vector.load();
		return { [&] <size_t... Is>(std::index_sequence<Is...>) { return std::tie(*component_ptr<Is>(table_index_t{n}, vector_ptr)...); }(std::make_index_sequence<vtll::size<DATA>::value>{}) };
	};

	/////
	// \brief Push a new element to the end of the stack.
	// \param[in] data References to the components to be added.
	// \returns the index of the new entry.
	///
	template<typename DATA, size_t N0, bool ROW, size_t SLOTS>
	template<typename... Cs>
		requires std::is_same_v<vtll::tl<std::decay_t<Cs>...>, vtll::remove_atomic<DATA>>
	inline auto VlltStack<DATA, N0, ROW, SLOTS>::push(Cs&&... data) noexcept -> stack_index_t {
		//increase m_next_free_slot to announce your demand for a new slot -> slot is now reserved for you
		slot_size_t size = m_size_cnt.load();	///< Make sure that no other thread is popping currently
		while (size.m_next_free_slot < size.m_size || !m_size_cnt.compare_exchange_weak(size, slot_size_t{ stack_index_t{ size.m_next_free_slot + 1 }, size.m_size })) {
			if (size.m_next_free_slot < size.m_size) { //here compare_exchange_weak was NOT called to copy manually
				size = m_size_cnt.load();
			}
		};

		//make sure there is enough space in the segment VECTOR - if not then change the old vector to a larger vector
		insert(table_index_t{size.m_next_free_slot}, nullptr, std::forward<Cs>(data)...); ///< Make sure there are enough slots for segments

		slot_size_t new_size = m_size_cnt.load();	///< Increase size to validate the new row
		while (!m_size_cnt.compare_exchange_weak(new_size, slot_size_t{ new_size.m_next_free_slot, stack_index_t{ new_size.m_size + 1} }));

		return stack_index_t{ size.m_next_free_slot };	///< Return index of new entry
	}

	/////
	// \brief Pop the last row if there is one.
	// \param[in] tup Pointer to tuple to move the row data into.
	// \param[in] del If true, then call desctructor on the removed slot.
	// \returns true if a row was popped.
	///
	template<typename DATA, size_t N0, bool ROW, size_t SLOTS>
	inline auto VlltStack<DATA, N0, ROW, SLOTS>::pop() noexcept -> tuple_opt_t {
		vtll::to_tuple<vtll::remove_atomic<DATA>> ret{};

		slot_size_t size = m_size_cnt.load();
		if (size.m_next_free_slot == 0) return std::nullopt;	///< Is there a row to pop off?

		/// Make sure that no other thread is currently pushing a new row
		while (size.m_next_free_slot > size.m_size ||
			!m_size_cnt.compare_exchange_weak(size, slot_size_t{ stack_index_t{size.m_next_free_slot - 1}, size.m_size })) {

			if (size.m_next_free_slot > size.m_size) { size = m_size_cnt.load(); }
			if (size.m_next_free_slot == 0) return std::nullopt;	///< Is there a row to pop off?
		};

		auto vector_ptr{ m_seg_vector.load() };						///< Access the segment vector

		auto idx = size.m_next_free_slot - 1;
		vtll::static_for<size_t, 0, vtll::size<DATA>::value >(	///< Loop over all components
			[&](auto i) {
				using type = vtll::Nth_type<DATA, i>;
				if		constexpr (std::is_move_assignable_v<type>) { std::get<i>(ret) = std::move(*component_ptr<i>(table_index_t{ idx }, vector_ptr)); }	//move
				else if constexpr (std::is_copy_assignable_v<type>) { std::get<i>(ret) = *component_ptr<i>(table_index_t{ idx }, vector_ptr); }				//copy
				else if constexpr (vtll::is_atomic<type>::value) { std::get<i>(ret) = component_ptr<i>(table_index_t{ idx }, vector_ptr)->load(); } 		//atomic

				if constexpr (std::is_destructible_v<type> && !std::is_trivially_destructible_v<type>) { component_ptr<i>(table_index_t{ idx }, vector_ptr)->~type(); }	///< Call destructor
			}
		);

		slot_size_t new_size = m_size_cnt.load();	///< Commit the popping of the row
		while (!m_size_cnt.compare_exchange_weak(new_size, slot_size_t{ new_size.m_next_free_slot, stack_index_t{ new_size.m_size - 1} }));

		return ret; //RVO?
	}

	/////
	// \brief Pop all rows and call the destructors.
	// \returns number of popped rows.
	///
	template<typename DATA, size_t N0, bool ROW, size_t SLOTS>
	inline auto VlltStack<DATA, N0, ROW, SLOTS>::clear() noexcept -> size_t {
		size_t num = 0;
		while (vsty::has_value(pop())) { ++num; }
		return num;
	}

	/////
	// \brief Swap the values of two rows.
	// \param[in] n1 Index of first row.
	// \param[in] n2 Index of second row.
	// \returns true if the operation was successful.
	///
	template<typename DATA, size_t N0, bool ROW, size_t SLOTS>
	inline auto VlltStack<DATA, N0, ROW, SLOTS>::swap(stack_index_t idst, stack_index_t isrc) noexcept -> void {
		assert(idst < size() && isrc < size());
		auto src = get_tuple(isrc);
		if (!vsty::has_value(src)) return;
		auto dst = get_tuple(idst);
		if (!vsty::has_value(dst)) return;
		vtll::static_for<size_t, 0, vtll::size<DATA>::value >([&](auto i) {
			using type = vtll::Nth_type<DATA, i>;
			//std::cout << typeid(type).name() << "\n";
			if constexpr (std::is_move_assignable_v<type> && std::is_move_constructible_v<type>) {
				std::swap(std::get<i>(dst.value()), std::get<i>(src.value()));
			}
			else if constexpr (std::is_copy_assignable_v<type> && std::is_copy_constructible_v<type>) {
				auto& tmp{ std::get<i>(src.value()) };
				std::get<i>(src.value()) = std::get<i>(dst.value());
				std::get<i>(dst.value()) = tmp;
			}
			else if constexpr (vtll::is_atomic<type>::value) {
				type tmp{ std::get<i>(src.value()).load() };
				std::get<i>(src.value()).store(std::get<i>(dst.value()).load());
				std::get<i>(dst.value()).store(tmp.load());
			}
		});
		return;
	}




	//----------------------------------------------------------------------------------------------------


	///
	// \brief VlltFIFOQueue is a FIFO queue that can be ued by multiple threads in parallel
	//
	// It has the following properties:
	// 1) It stores tuples of data
	// 2) Lockless multithreaded access.
	//
	// The FIFO queue is a stack thet keeps segment pointers in a vector.
	// Segments that are empty are recycled to the end of the segments vector.
	// An offset is maintained that is subtracted from a table index.
	//
	//

	template<typename DATA, size_t N0 = 1 << 10, bool ROW = true, size_t SLOTS = 16>
	class VlltFIFOQueue : public VlltTable<DATA, N0, ROW, SLOTS> {

	public:
		using VlltTable<DATA, N0, ROW, SLOTS>::N;
		using VlltTable<DATA, N0, ROW, SLOTS>::L;
		using VlltTable<DATA, N0, ROW, SLOTS>::m_seg_vector;
		using VlltTable<DATA, N0, ROW, SLOTS>::m_mr;
		using VlltTable<DATA, N0, ROW, SLOTS>::component_ptr;
		using VlltTable<DATA, N0, ROW, SLOTS>::insert;
		using VlltTable<DATA, N0, ROW, SLOTS>::resize;
		using VlltTable<DATA, N0, ROW, SLOTS>::segment;

		using tuple_opt_t = std::optional< vtll::to_tuple< vtll::remove_atomic<DATA> > >;
		using typename VlltTable<DATA, N0, ROW, SLOTS>::table_index_t;
		using typename VlltTable<DATA, N0, ROW, SLOTS>::segment_t;
		using typename VlltTable<DATA, N0, ROW, SLOTS>::segment_ptr_t;
		using typename VlltTable<DATA, N0, ROW, SLOTS>::segment_vector_t;

		VlltFIFOQueue() {};

		template<typename... Cs>
			requires std::is_same_v<vtll::tl<std::decay_t<Cs>...>, vtll::remove_atomic<DATA>>
		inline auto push(Cs&&... data) noexcept -> table_index_t;	///< Push new component data to the end of the table

		inline auto pop() noexcept		-> tuple_opt_t;	///< Remove the last row, call destructor on components
		inline auto size() noexcept		-> size_t;				///< Number of elements in the queue
		inline auto clear() noexcept	-> size_t;			///< Set the number if rows to zero - effectively clear the table, call destructors

	protected:
		std::atomic<table_index_t>	m_next{ table_index_t{0} };	//next element to be taken out of the queue
		std::atomic<table_index_t>	m_consumed{vsty::null_value<table_index_t>()};		//last element that was taken out and fully read and destroyed
		std::atomic<table_index_t>	m_next_free_slot{ table_index_t{0} };	//next element to write over
		std::atomic<table_index_t>	m_last{vsty::null_value<table_index_t>()};			//last element that has been produced and fully constructed
	};


	///
	// \brief Push a new element to the end of the stack.
	// \param[in] data References to the components to be added.
	// \returns the index of the new entry.
	///
	template<typename DATA, size_t N0, bool ROW, size_t SLOTS>
	template<typename... Cs>
		requires std::is_same_v<vtll::tl<std::decay_t<Cs>...>, vtll::remove_atomic<DATA>>
	inline auto VlltFIFOQueue<DATA, N0, ROW, SLOTS>::push(Cs&&... data) noexcept -> table_index_t {
		auto next_free_slot = m_next_free_slot.load();
		while (!m_next_free_slot.compare_exchange_weak(next_free_slot, table_index_t{ next_free_slot + 1 })); ///< Slot number to put the new data into	

		insert(next_free_slot, &m_consumed, std::forward<Cs>(data)...);

		table_index_t expected = next_free_slot > 0 ? table_index_t{ next_free_slot - 1 } : table_index_t{ vsty::null_value<table_index_t>() };
		auto old_last{ expected };
		while (!m_last.compare_exchange_weak(old_last, next_free_slot)) { old_last = expected; };

		return next_free_slot;	///< Return index of new entry
	}

	/// <summary>
	/// Pop the next item from the queue.
	/// Move its content into the return value (tuple),
	/// then remove it from the queue.
	/// </summary>
	/// <typeparam name="DATA">Type list of data items.</typeparam>
	/// <typeparam name="N0">Number of items per segment.</typeparam>
	/// <typeparam name="ROW">ROW or COLUMN layout.</typeparam>
	/// <typeparam name="SLOTS">Default number of slots in the first segment vector.</typeparam>
	/// <returns>Tuple with values from the next item, or nullopt.</returns>
	template<typename DATA, size_t N0, bool ROW, size_t SLOTS>
	inline auto VlltFIFOQueue<DATA, N0, ROW, SLOTS>::pop() noexcept -> tuple_opt_t {
		vtll::to_tuple<vtll::remove_atomic<DATA>> ret{};

		if (!vsty::has_value(m_last.load())) return std::nullopt;

		table_index_t last;
		auto next = m_next.load();
		do {
			last = m_last.load();
			if (!(next <= last)) return std::nullopt;
		} while (!m_next.compare_exchange_weak(next, table_index_t{ next + 1ul }));  ///< Slot number to put the new data into	
		
		auto vector_ptr{ m_seg_vector.load() };						///< Access the segment vector

		vtll::static_for<size_t, 0, vtll::size<DATA>::value >(	///< Loop over all components
			[&](auto i) {
				using type = vtll::Nth_type<DATA, i>;
				if		constexpr (std::is_move_assignable_v<type>) { std::get<i>(ret) = std::move(*component_ptr<i>(next, vector_ptr)); } //move
				else if constexpr (std::is_copy_assignable_v<type>) { std::get<i>(ret) = *component_ptr<i>(next, vector_ptr); } 			//copy
				else if constexpr (vtll::is_atomic<type>::value) { std::get<i>(ret) = component_ptr<i>(next, vector_ptr)->load(); } 	//atomic

				if constexpr (std::is_destructible_v<type> && !std::is_trivially_destructible_v<type>) { component_ptr<i>(next, vector_ptr)->~type(); }	///< Call destructor
			}
		);

		table_index_t expected = (next > 0 ? table_index_t{ next - 1ull } : table_index_t{ vsty::null_value<table_index_t>() });
		auto consumed{ expected };
		while (!m_consumed.compare_exchange_weak(consumed, next)) { consumed = expected; };

		return ret;		//RVO?
	}

	/// <summary>
	/// Get the number of items currently in the queue.
	/// </summary>
	/// <typeparam name="DATA">Type list of data items.</typeparam>
	/// <typeparam name="N0">Number of items per segment.</typeparam>
	/// <typeparam name="ROW">ROW or COLUMN layout.</typeparam>
	/// <typeparam name="SLOTS">Default number of slots in the first segment vector.</typeparam>
	/// <returns>Number of items currently in the queue.</returns>
	template<typename DATA, size_t N0, bool ROW, size_t SLOTS>
	inline auto VlltFIFOQueue<DATA, N0, ROW, SLOTS>::size() noexcept -> size_t {
		auto last = m_last.load();
		auto consumed = m_consumed.load();
		size_t sz{0};
		if (vsty::has_value(last)) {	//have items been produced yet?
			sz += last;			//yes -> count them
			if (vsty::has_value(consumed)) sz -= consumed; //have items been consumed yet?
			else ++sz;	//no -> we start at zero, so increase by 1
		}

		return sz;
	}

	/// <summary>
	/// Remove all items from the queue.
	/// </summary>
	/// <typeparam name="DATA">Type list of data items.</typeparam>
	/// <typeparam name="N0">Number of items per segment.</typeparam>
	/// <typeparam name="ROW">ROW or COLUMN layout.</typeparam>
	/// <typeparam name="SLOTS">Default number of slots in the first segment vector.</typeparam>
	/// <returns>Number of items removed from the queue.</returns>
	template<typename DATA, size_t N0, bool ROW, size_t SLOTS>
	inline auto VlltFIFOQueue<DATA, N0, ROW, SLOTS>::clear() noexcept -> size_t {
		size_t num = 0;
		while (vsty::has_value(pop())) { ++num; }
		
		//auto next_free_slot = m_next_free_slot.load();
		//auto vector_ptr{ m_seg_vector.load() };		///< Shared pointer to current segment ptr vector, can be nullptr
		//resize(next_free_slot, vector_ptr, &m_consumed);

		return num;
	}

}


