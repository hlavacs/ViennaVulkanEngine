
#include <optional>
#include "VETable.h"

namespace vve {

	template<typename T, size_t L, bool S>
	T* VeTable<T, L, S>::address(size_t n) noexcept {
		assert(n < m_size && m_size != 0);
		return &m_segments[n >> L]->m_data[n & BIT_MASK];
	}

	template<typename T, size_t L, bool S>
	size_t VeTable<T, L, S>::push_back3(T&& value) noexcept {
		size_t idx = m_size;
		*address(idx) = value;
		m_size++;
		return idx;
	}

	template<typename T, size_t L, bool S>
	size_t VeTable<T, L, S>::push_back2(T&& value) noexcept {
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex_append); //block other writers

		if (m_size < m_segments.size() * N) {	//append within last segment
			return push_back3(value);
		}

		if (m_size < m_segments.capacity() * N) { //append new segment, no reallocation
			m_segments.push_back({});
			return push_back3(value);
		}

		std::lock_guard<std::shared_timed_mutex> writerLock2(m_mutex); //reallocation -> block readers
		m_segments.push_back({});	//reallocate vector
		return push_back3(value);
	}

	//---------------------------------------------------------------------------

	template<typename T, size_t L, bool S>
	std::optional<T> VeTable<T, L, S>::at(size_t n) noexcept {
		static_assert(L>0);
		std::shared_lock<std::shared_timed_mutex> readerLock(m_mutex);
		if (n >= m_size) return {};
		return {*address(n)};
	}

	template<typename T, size_t L, bool S>
	void VeTable<T, L, S>::set(size_t n, T&& value) noexcept {
		if (m_read_only) return;
		std::shared_lock<std::shared_timed_mutex> readerLock(m_mutex);
		if (n >= m_size) return;
		*address(n) = value;
	}

	template<typename T, size_t L, bool S>
	std::optional<size_t> VeTable<T, L, S>::push_back(T&& value) noexcept {
		if (m_read_only) return {};
		return { push_back2(value) };
	}

	template<typename T, size_t L, bool S>
	std::optional<T> VeTable<T, L, S>::pop_back() noexcept {
		if (m_read_only) return;
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);
		if (m_size == 0) return {};
		auto element = *address(m_size);
		--m_size;
		if constexpr (S) {
			if (m_size < (m_segments.size() - 1) * N) m_segments.pop_back();
		}
		return element;
	}

	template<typename T, size_t L, bool S>
	void VeTable<T, L, S>::erase(size_t n) noexcept {
		if (m_read_only) return;
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);
		if (n >= m_size || m_size == 0) return;
		if( n < m_size-1) *address(n) = *address(m_size-1);
		--m_size;
		if constexpr (S) {
			if (m_size < (m_segments.size() - 1) * N) m_segments.pop_back();
		}
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
		while(m_segments.size() * N < n) m_segments.push_back({});
	}
}

