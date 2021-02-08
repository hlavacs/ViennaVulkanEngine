
#include <optional>
#include "VEContainer.h"

namespace vve {


	//---------------------------------------------------------------------------
	//VeVector

	const int SYNC_APPEND = 1;
	const int SYNC_ALL = 2;

	template<typename T, size_t L, int SYNC, bool S>
	T* VeVector<T, L, SYNC, S>::address(size_t n) noexcept {
		assert(n < m_size && m_size != 0);
		return &m_segment[n >> L]->m_entry[n & BIT_MASK];
	}

	template<typename T, size_t L, int SYNC, bool S>
	size_t VeVector<T, L, SYNC, S>::push_back3(T&& value) noexcept {
		size_t idx = m_size;
		*address(idx) = value;
		m_size++;
		return idx;
	}

	template<typename T, size_t L, int SYNC, bool S>
	size_t VeVector<T, L, SYNC, S>::push_back2(T&& value) noexcept {
		if constexpr (SYNC >= SYNC_APPEND) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex_append); } //block other writers
		if (m_size < m_segment.size() * N) {	//append within last segment
			return push_back3(value);
		}

		if (m_size < m_segment.capacity() * N) { //append new segment, no reallocation
			m_segment.push_back({});
			return push_back3(value);
		}

		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock2(m_mutex); } //reallocation -> block readers
		m_segment.push_back({});	//reallocate vector
		return push_back3(value);
	}

	template<typename T, size_t L, int SYNC, bool S>
	std::optional<T> VeVector<T, L, SYNC, S>::at(size_t n) noexcept {
		static_assert(L>0);
		if constexpr (SYNC >= SYNC_ALL) { std::shared_lock<std::shared_timed_mutex> readerLock(m_mutex); }
		if (n >= m_size) return {};
		return {*address(n)};
	}

	template<typename T, size_t L, int SYNC, bool S>
	void VeVector<T, L, SYNC, S>::set(size_t n, T&& value) noexcept {
		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex); }
		if (n >= m_size) return;
		*address(n) = value;
	}

	template<typename T, size_t L, int SYNC, bool S>
	size_t VeVector<T, L, SYNC, S>::push_back(T&& value) noexcept {
		return push_back2(value);
	}

	template<typename T, size_t L, int SYNC, bool S>
	std::optional<T> VeVector<T, L, SYNC, S>::pop_back() noexcept {
		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex); }
		if (m_size == 0) return {};
		auto element = *address(m_size);
		--m_size;
		if constexpr (S) {
			if (m_size < (m_segment.size() - 1) * N) m_segment.pop_back();
		}
		return element;
	}

	template<typename T, size_t L, int SYNC, bool S>
	void VeVector<T, L, SYNC, S>::erase(size_t n) noexcept {
		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex); }
		if (n >= m_size || m_size == 0) return;
		if( n < m_size-1) *address(n) = *address(m_size-1);
		--m_size;
		if constexpr (S) {
			if (m_size < (m_segment.size() - 1) * N) m_segment.pop_back();
		}
	}

	template<typename T, size_t L, int SYNC, bool S>
	void VeVector<T, L, SYNC, S>::swap(size_t n1, size_t n2) noexcept {
		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex); }
		if (n1 == n2 || n1 >= m_size || n2 >= m_size) return;
		std::swap(*address(n1), *address(n2));
	}

	template<typename T, size_t L, int SYNC, bool S>
	void VeVector<T, L, SYNC, S>::reserve(size_t n) noexcept {
		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex); }
		while(m_segment.size() * N < n) m_segment.push_back({});
	}

	//---------------------------------------------------------------------------
	//VeTable

	template<typename T, size_t L, int SYNC, bool S>
	T* VeTable<T, L, SYNC, S>::address(size_t n) noexcept {
		assert(n < m_size&& m_size != 0);
		return &m_segment[n >> L]->m_entry[n & BIT_MASK].m_data;
	}

	template<typename T, size_t L, int SYNC, bool S>
	size_t VeTable<T, L, SYNC, S>::push_back3(T&& value) noexcept {
		size_t idx = m_size;
		*address(idx) = value;
		m_segment[idx >> L]->m_size++;
		m_size++;
		return idx;
	}

	template<typename T, size_t L, int SYNC, bool S>
	size_t VeTable<T, L, SYNC, S>::add2(T&& value) noexcept {
		if constexpr (SYNC >= SYNC_APPEND) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex_append); } //block other writers
		if (!m_first_free.is_null()) {		//there is an empty slot in the table
			auto free = m_first_free;
			auto ptr = address(free.value);
			m_first_free = ptr->m_next;
			*ptr = value;
			m_segment[free.value >> L]->m_size++;
			m_size++;
			return free.value;
		}

		if (m_size < m_segment.size() * N) {	//append within last segment
			return push_back3(value);
		}

		if (m_size < m_segment.capacity() * N) { //append new segment, no reallocation
			m_segment.push_back({});
			return push_back3(value);
		}

		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock2(m_mutex); } //reallocation -> block readers
		m_segment.push_back({});	//reallocate vector
		return push_back3(value);
	}


	template<typename T, size_t L, int SYNC, bool S>
	std::optional<T> VeTable<T, L, SYNC, S>::at(size_t n) noexcept {
		static_assert(L > 0);
		if constexpr (SYNC >= SYNC_ALL) { std::shared_lock<std::shared_timed_mutex> readerLock(m_mutex); }
		if (n >= m_size) return {};
		return { *address(n) };
	}

	template<typename T, size_t L, int SYNC, bool S>
	void VeTable<T, L, SYNC, S>::set(size_t n, T&& value) noexcept {
		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex); }
		if (n >= m_size) return;
		*address(n) = value;
	}

	template<typename T, size_t L, int SYNC, bool S>
	size_t VeTable<T, L, SYNC, S>::add(T&& value) noexcept {
		return add2(value);
	}

	template<typename T, size_t L, int SYNC, bool S>
	void VeTable<T, L, SYNC, S>::erase(size_t n) noexcept {
		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex); }
		if (n >= m_size || m_size == 0) return;
		address(n)->m_next = m_first_free;
		m_first_free.value = n;
		m_segment[n >> L]->m_size--;
		--m_size;
	}

	template<typename T, size_t L, int SYNC, bool S>
	void VeTable<T, L, SYNC, S>::swap(size_t n1, size_t n2) noexcept {
		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex); }
		if (n1 == n2 || n1 >= m_size || n2 >= m_size) return;
		std::swap(*address(n1), *address(n2));
	}

	template<typename T, size_t L, int SYNC, bool S>
	void VeTable<T, L, SYNC, S>::reserve(size_t n) noexcept {
		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex); }
		while (m_segment.size() * N < n) m_segment.push_back({});
	}


	//---------------------------------------------------------------------------
	//VeSlotMap

	template<typename T, typename ID, size_t L, int SYNC, bool S>
	std::optional<T> VeSlotMap<T, ID, L, SYNC, S>::at(size_t n, ID& id) noexcept {
		if constexpr (SYNC >= SYNC_ALL) { std::shared_lock<std::shared_timed_mutex> readerLock(m_mutex); }
		auto m = m_map.at(n);
		if (!m.has_value() || m.m_id != id) return {};
		return { m_entry.at(m.m_entry_index) };
	}

	template<typename T, typename ID, size_t L, int SYNC, bool S>
	void VeSlotMap<T, ID, L, SYNC, S>::set(size_t n, ID& id, T&& v) noexcept {
		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex); }
		auto m = m_map.at(n);
		if (!m.has_value() || m.m_id != id) return;
		m_entry.set(m.entry_index, v);
	}

	template<typename T, typename ID, size_t L, int SYNC, bool S>
	size_t VeSlotMap<T, ID, L, SYNC, S>::add(ID&&id, T&& value) noexcept {
		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex); }
		auto index = m_entry.push_back(value);
		return m_map.add({index, id});
	}

	template<typename T, typename ID, size_t L, int SYNC, bool S>
	void VeSlotMap<T, ID, L, SYNC, S>::erase(size_t n, ID& id) noexcept {
		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex); }
	}

	template<typename T, typename ID, size_t L, int SYNC, bool S>
	void VeSlotMap<T, ID, L, SYNC, S>::swap(size_t n1, size_t n2) noexcept {
		if constexpr (SYNC >= SYNC_ALL) { std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex); }
	}


}

