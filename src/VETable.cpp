
#include <optional>
#include "VETable.h"

namespace vve {

	template<typename T, size_t L>
	T* VeTable<T, L>::address(size_t n) noexcept {
		assert(n >= 0 && n < m_size);
		auto segment = m_segments[n >> L];
		auto index = n & BIT_MASK;
		return &segment.m_data[index];
	}

	template<typename T, size_t L>
	size_t VeTable<T, L>::push_back2(T&& value) noexcept {
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);

		if (m_size < m_segments.size() * N) {
			*address(m_size) = value;
			m_size++;
			return m_size - 1;
		}

		m_segments.push_back({});
		*address(m_size) = value;
		m_size++;
		return m_size - 1;
	}

	//---------------------------------------------------------------------------

	template<typename T, size_t L>
	std::optional<T> VeTable<T,L>::at(size_t n) noexcept {
		static_assert(L>0);
		std::shared_lock<std::shared_timed_mutex> readerLock(m_mutex);
		if (n < 0 || n >= m_size) return {};
		return {*address(n)};
	}

	template<typename T, size_t L>
	void VeTable<T, L>::set(size_t n, T& value) noexcept {
		if (m_read_only) return;
		std::shared_lock<std::shared_timed_mutex> readerLock(m_mutex);
		if (n < 0 || n >= m_size) return;
		*address(n) = value;
	}

	template<typename T, size_t L>
	std::optional<size_t> VeTable<T, L>::push_back(T&& value) noexcept {
		if (m_read_only) return {};
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);

		if (m_size < m_segments.size() * N) {
			*address(m_size) = value;
			m_size++;
			return { m_size - 1 };
		}

		m_segments.push_back({});
		*address(m_size) = value;
		m_size++;
		return { m_size - 1 };
	}

	template<typename T, size_t L>
	std::optional<T> VeTable<T, L>::pop_back() noexcept {
		if (m_read_only) return;
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);
		if (m_size == 0) return {};
		auto element = *address(m_size);
		--m_size;
		return element;
	}

	template<typename T, size_t L>
	void VeTable<T, L>::erase(size_t n) noexcept {
		if (m_read_only) return;
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);
		if (n <= 0 || m_size == 0) return;
		if( n != m_size-1) std::swap(*address(n), *address(m_size-1));
		--m_size;
	}

	template<typename T, size_t L>
	void VeTable<T, L>::swap(size_t n1, size_t n2) noexcept {
		if (m_read_only) return;
		std::lock_guard<std::shared_timed_mutex> writerLock(m_mutex);
		if (n1 < 0 || n1 >= m_size || n2 < 0 || n2 >= m_size) return;
		return std::swap(*address(n1), *address(n2));
	}
}

