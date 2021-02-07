
#include <optional>
#include "VETable.h"

namespace vve {


	//---------------------------------------------------------------------------
	//VeVector

	template<typename T, size_t L, bool S>
	T* VeVector<T, L, S>::address(size_t n) noexcept {
		assert(n < m_size && m_size != 0);
		return &m_segment[n >> L]->m_entry[n & BIT_MASK].m_data;
	}

	template<typename T, size_t L, bool S>
	size_t VeVector<T, L, S>::push_back3(T&& value) noexcept {
		size_t idx = m_size;
		*address(idx) = value;
		m_size++;
		return idx;
	}

	template<typename T, size_t L, bool S>
	size_t VeVector<T, L, S>::push_back2(T&& value) noexcept {
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex_append); //block other writers

		if (m_size < m_segment.size() * N) {	//append within last segment
			return push_back3(value);
		}

		if (m_size < m_segment.capacity() * N) { //append new segment, no reallocation
			m_segment.push_back({});
			return push_back3(value);
		}

		std::lock_guard<std::shared_timed_mutex> writerLock2(m_mutex); //reallocation -> block readers
		m_segment.push_back({});	//reallocate vector
		return push_back3(value);
	}

	template<typename T, size_t L, bool S>
	std::optional<T> VeVector<T, L, S>::at(size_t n) noexcept {
		static_assert(L>0);
		std::shared_lock<std::shared_timed_mutex> readerLock(m_mutex);
		if (n >= m_size) return {};
		return {*address(n)};
	}

	template<typename T, size_t L, bool S>
	void VeVector<T, L, S>::set(size_t n, T&& value) noexcept {
		if (m_read_only) return;
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);
		if (n >= m_size) return;
		*address(n) = value;
	}

	template<typename T, size_t L, bool S>
	std::optional<size_t> VeVector<T, L, S>::push_back(T&& value) noexcept {
		if (m_read_only) return {};
		return { push_back2(value) };
	}

	template<typename T, size_t L, bool S>
	std::optional<T> VeVector<T, L, S>::pop_back() noexcept {
		if (m_read_only) return;
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);
		if (m_size == 0) return {};
		auto element = *address(m_size);
		--m_size;
		if constexpr (S) {
			if (m_size < (m_segment.size() - 1) * N) m_segment.pop_back();
		}
		return element;
	}

	template<typename T, size_t L, bool S>
	void VeVector<T, L, S>::erase(size_t n) noexcept {
		if (m_read_only) return;
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);
		if (n >= m_size || m_size == 0) return;
		if( n < m_size-1) *address(n) = *address(m_size-1);
		--m_size;
		if constexpr (S) {
			if (m_size < (m_segment.size() - 1) * N) m_segment.pop_back();
		}
	}

	template<typename T, size_t L, bool S>
	void VeVector<T, L, S>::swap(size_t n1, size_t n2) noexcept {
		if (m_read_only) return;
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);
		if (n1 == n2 || n1 >= m_size || n2 >= m_size) return;
		std::swap(*address(n1), *address(n2));
	}

	template<typename T, size_t L, bool S>
	void VeVector<T, L, S>::reserve(size_t n) noexcept {
		if (m_read_only) return;
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);
		while(m_segment.size() * N < n) m_segment.push_back({});
	}

	//---------------------------------------------------------------------------
	//VeTable

	template<typename T, size_t L, bool S>
	T* VeTable<T, L, S>::address(size_t n) noexcept {
		assert(n < m_size&& m_size != 0);
		return &m_segment[n >> L]->m_entry[n & BIT_MASK].m_data;
	}

	template<typename T, size_t L, bool S>
	size_t VeTable<T, L, S>::push_back3(T&& value) noexcept {
		size_t idx = m_size;
		*address(idx) = value;
		m_segment[idx >> L]->m_size++;
		m_size++;
		return idx;
	}

	template<typename T, size_t L, bool S>
	size_t VeTable<T, L, S>::add2(T&& value) noexcept {
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex_append); //block other writers

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

		std::lock_guard<std::shared_timed_mutex> writerLock2(m_mutex); //reallocation -> block readers
		m_segment.push_back({});	//reallocate vector
		return push_back3(value);
	}


	template<typename T, size_t L, bool S>
	std::optional<T> VeTable<T, L, S>::at(size_t n) noexcept {
		static_assert(L > 0);
		std::shared_lock<std::shared_timed_mutex> readerLock(m_mutex);
		if (n >= m_size) return {};
		return { *address(n) };
	}

	template<typename T, size_t L, bool S>
	void VeTable<T, L, S>::set(size_t n, T&& value) noexcept {
		if (m_read_only) return;
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);
		if (n >= m_size) return;
		*address(n) = value;
	}

	template<typename T, size_t L, bool S>
	std::optional<size_t> VeTable<T, L, S>::add(T&& value) noexcept {
		if (m_read_only) return {};
		return { add2(value) };
	}

	template<typename T, size_t L, bool S>
	void VeTable<T, L, S>::erase(size_t n) noexcept {
		if (m_read_only) return;
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);
		if (n >= m_size || m_size == 0) return;
		address(n)->m_next = m_first_free;
		m_first_free.value = n;
		m_segment[n >> L]->m_size--;
		--m_size;
	}

	template<typename T, size_t L, bool S>
	void VeTable<T, L, S>::swap(size_t n1, size_t n2) noexcept {
		if (m_read_only) return;
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);
		if (n1 == n2 || n1 >= m_size || n2 >= m_size) return;
		std::swap(*address(n1), *address(n2));
	}

	template<typename T, size_t L, bool S>
	void VeTable<T, L, S>::reserve(size_t n) noexcept {
		if (m_read_only) return;
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);
		while (m_segment.size() * N < n) m_segment.push_back({});
	}


}

