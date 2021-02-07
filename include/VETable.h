#ifndef VETABLE_H
#define VETABLE_H

#include <assert.h>
#include <memory_resource>
#include <shared_mutex>
#include <optional>
#include <array>
#include "VGJS.h"

namespace vve {



	template<typename T, size_t L = 8, bool SHRINK = false>
	class VeVector {
	protected:
		static const size_t N = 1 << L;
		const uint64_t BIT_MASK = (0xFFFFFFFFFFFFFFFF >> L) << L;

		struct VeTableSegment {
			std::array<T, N> m_data;
		};

		using seg_ptr = std::unique_ptr<VeTableSegment>;

		std::pmr::memory_resource*	m_mr = nullptr;
		std::pmr::vector<seg_ptr>	m_segments;
		std::atomic<size_t>			m_size = 0;
		std::shared_timed_mutex 	m_mutex;		//guard reads and writes
		std::mutex 					m_mutex_append;	//guard reads and writes
		std::atomic<bool>			m_read_only = false;

		T* address(size_t) noexcept;
		void set_read_only(bool flag) noexcept { m_read_only = flag; };
		size_t push_back2(T&&) noexcept;
		size_t push_back3(T&&) noexcept;

	public:
		VeVector(std::pmr::memory_resource* mr = std::pmr::new_delete_resource())  noexcept
			: m_mr{ mr }, m_segments{mr}  {};
		std::optional<T>		at(size_t n) noexcept;
		void					set(size_t n, T&& v) noexcept;
		size_t					size() const noexcept { return m_size; };
		std::optional<size_t>	push_back(T&&) noexcept;
		std::optional<T>		pop_back() noexcept;
		void					erase(size_t n) noexcept;
		void					swap(size_t n1, size_t n2) noexcept;
		bool					get_read_only() noexcept { return m_read_only; } ;
		void					reserve(size_t n) noexcept;
	};


	//---------------------------------------------------------------------------


	template<typename T, size_t L = 8, bool SHRINK = false>
	class VeTable {
	protected:
		static const size_t N = 1 << L;
		const uint64_t BIT_MASK = (0xFFFFFFFFFFFFFFFF >> L) << L;
		using index_t = vgjs::int_type<size_t, struct P0, std::numeric_limits<size_t>::max()>;

		struct VeTableSegment {
			std::array<T, N> m_data;
			size_t			 m_size = 0;
			index_t			 m_next{};
		};

		using seg_ptr = std::unique_ptr<VeTableSegment>;

		std::pmr::memory_resource*  m_mr = nullptr;
		std::pmr::vector<seg_ptr>	m_segments;
		std::atomic<size_t>			m_size = 0;
		index_t						m_first_free{};
		std::shared_timed_mutex 	m_mutex;		//guard reads and writes
		std::mutex 					m_mutex_append;	//guard reads and writes
		std::atomic<bool>			m_read_only = false;

		T* address(size_t) noexcept;
		void set_read_only(bool flag) noexcept { m_read_only = flag; };
		size_t add2(T&&) noexcept;
		size_t push_back3(T&&) noexcept;

	public:
		VeTable(std::pmr::memory_resource* mr = std::pmr::new_delete_resource())  noexcept
			: m_mr{ mr }, m_segments{ mr }  {};
		std::optional<T>		at(size_t n) noexcept;
		void					set(size_t n, T&& v) noexcept;
		size_t					size() const noexcept { return m_size; };
		std::optional<size_t>	add(T&&) noexcept;
		void					erase(size_t n) noexcept;
		void					swap(size_t n1, size_t n2) noexcept;
		bool					get_read_only() noexcept { return m_read_only; };
		void					reserve(size_t n) noexcept;
	};


}


#endif

