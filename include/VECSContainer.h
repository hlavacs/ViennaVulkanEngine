#ifndef VECSCONTAINER_H
#define VECSCONTAINER_H

#include <assert.h>
#include <memory_resource>
#include <shared_mutex>
#include <optional>
#include <array>
#include "VGJS.h"
#include "VECSUtil.h"

namespace vecs {

	template<typename T, size_t L = 8, int SYNC = 2, bool SHRINK = false>
	class VeVector {
	protected:
		static const size_t N = 1 << L;
		const uint64_t BIT_MASK = N - 1;

		struct VeTableSegment {
			std::array<T, N> m_entry;
		};

		using seg_ptr = std::unique_ptr<VeTableSegment>;

		std::pmr::memory_resource*	m_mr = nullptr;
		std::pmr::vector<seg_ptr>	m_segment;
		std::atomic<size_t>			m_size = 0;
		std::shared_timed_mutex 	m_mutex;		//guard reads and writes
		std::mutex 					m_mutex_append;	//guard reads and writes

		T* address(size_t) noexcept;
		size_t push_back2(T&&) noexcept;
		size_t push_back3(T&&) noexcept;

	public:
		VeVector(std::pmr::memory_resource* mr = std::pmr::new_delete_resource())  noexcept
			: m_mr{ mr }, m_segment{mr}  {};
		std::optional<T>	at(size_t n) noexcept;
		void				set(size_t n, T&& v) noexcept;
		size_t				size() const noexcept { return m_size; };
		size_t				push_back(T&&) noexcept;
		std::optional<T>	pop_back() noexcept;
		void				erase(size_t n) noexcept;
		void				swap(size_t n1, size_t n2) noexcept;
		void				reserve(size_t n) noexcept;
	};

	//---------------------------------------------------------------------------

	template<typename T, size_t L = 8, int SYNC = 2, bool SHRINK = false>
	class VeTable {
	protected:
		static const size_t N = 1 << L;
		const uint64_t BIT_MASK = N - 1;

		struct VeTableEntry {
			T		m_data;		//the entry data 
			index_t m_next{};	//use for list of free entries
		};

		struct VeTableSegment {
			std::array<VeTableEntry, N> m_entry;
			size_t						m_size = 0;
		};

		using seg_ptr = std::unique_ptr<VeTableSegment>;

		std::pmr::memory_resource*  m_mr = nullptr;
		std::pmr::vector<seg_ptr>	m_segment;
		std::atomic<size_t>			m_size = 0;
		index_t						m_first_free{};
		std::shared_timed_mutex 	m_mutex;		//guard reads and writes
		std::mutex 					m_mutex_append;	//guard reads and writes

		T* address(size_t) noexcept;
		size_t add2(T&&) noexcept;
		size_t push_back3(T&&) noexcept;

	public:
		VeTable(std::pmr::memory_resource* mr = std::pmr::new_delete_resource())  noexcept
			: m_mr{ mr }, m_segment{ mr }  {};
		std::optional<T>	at(size_t n) noexcept;
		void				set(size_t n, T&& v) noexcept;
		size_t				size() const noexcept { return m_size; };
		size_t				add(T&&) noexcept;
		void				erase(size_t n) noexcept;
		void				swap(size_t n1, size_t n2) noexcept;
		void				reserve(size_t n) noexcept;
	};

	//---------------------------------------------------------------------------

	template<typename T, typename ID, size_t L = 8, int SYNC = 2, bool SHRINK = false>
	class VeSlotMap {
	protected:
		static const size_t N = 1 << L;
		const uint64_t BIT_MASK = N - 1;

		struct VeMapEntry {
			index_t	m_entry_index;
			ID		m_id;
		};

		VeVector<T, L, 0, SHRINK>			m_entry;
		VeTable<VeMapEntry, L, 0, SHRINK>	m_map;
		std::shared_timed_mutex 			m_mutex;		//guard reads and writes
		std::mutex 							m_mutex_append;	//guard appends

	public:
		VeSlotMap(std::pmr::memory_resource* mr = std::pmr::new_delete_resource())  noexcept
			: m_entry{ mr }, m_map{ mr }  {};
		std::optional<T>	at(size_t n, ID& id) noexcept;
		void				set(size_t n, ID& id, T&& v) noexcept;
		size_t				size() const noexcept { return m_entry.size(); };
		size_t				add(ID&& id, T&& v) noexcept;
		void				erase(size_t n, ID& id) noexcept;
		void				swap(size_t n1, size_t n2) noexcept;
		void				reserve(size_t n) noexcept { m_entry.reserve(n); m_map.reserve(n); };
	};


}


#endif

