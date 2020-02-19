#pragma once

namespace vve {

	void testVector();


	template <typename T>
	class VeVector {

	protected:

		uint8_t* m_memptr;
		uint8_t* m_startptr;
		VeIndex  m_entrySize;
		VeIndex  m_size;
		VeIndex  m_capacity;
		VeIndex  m_alignment;

		uint64_t alignBoundary(uint64_t size, VeIndex alignment) {
			uint64_t result = size;
			if (alignment > 1) {
				uint64_t mod = result & (alignment - 1);
				if (mod > 0) result += alignment - mod;
			}
			return result;
		}

		void construct() {
			for (uint32_t i = 0; i < m_capacity; ++i) {
				uint8_t* ptr = m_startptr + i * m_entrySize;
				T* tptr = new(ptr) T();
			}
		}

		void destruct() {
			for (uint32_t i = 0; i < m_capacity; ++i) {
				T* tptr = (T*)(m_startptr + i * m_entrySize);
				tptr->~T();
			}
		}

		void doubleCapacity() {
			VeIndex newcapacity = 2*m_capacity;
			uint8_t* newmemptr = new uint8_t[newcapacity * m_entrySize + m_alignment];
			uint8_t* newstartptr = (uint8_t*)alignBoundary((uint64_t)newmemptr, m_alignment);

			memcpy(newstartptr, m_startptr, (std::size_t) m_size * m_entrySize);
			destruct();
			delete[] m_memptr;
			m_memptr = newmemptr;
			m_startptr = newstartptr;
			m_capacity = newcapacity;
			construct();
		}

	public:

		typedef VeIndex size_type;

		class iterator {
		public:
			typedef iterator self_type;
			typedef T value_type;
			typedef T& reference;
			typedef T* pointer;
			typedef std::forward_iterator_tag iterator_category;
			typedef int difference_type;
			iterator(uint8_t* ptr) : ptr_((T*)ptr) { }
			self_type operator++() { self_type i = *this; ptr_++; return i; }
			self_type operator++(int junk) { ptr_++; return *this; }
			reference operator*() { return *ptr_; }
			pointer operator->() { return ptr_; }
			bool operator==(const self_type& rhs) { return ptr_ == rhs.ptr_; }
			bool operator!=(const self_type& rhs) { return ptr_ != rhs.ptr_; }
		private:
			pointer ptr_;
		};

		class const_iterator {
		public:
			typedef const_iterator self_type;
			typedef T value_type;
			typedef T& reference;
			typedef T* pointer;
			typedef int difference_type;
			typedef std::forward_iterator_tag iterator_category;
			const_iterator(uint8_t *ptr) : ptr_((T*)ptr) { }
			self_type operator++() { self_type i = *this; ptr_++; return i; }
			self_type operator++(int junk) { ptr_++; return *this; }
			const reference operator*() { return *ptr_; }
			const pointer operator->() { return ptr_; }
			bool operator==(const self_type& rhs) { return ptr_ == rhs.ptr_; }
			bool operator!=(const self_type& rhs) { return ptr_ != rhs.ptr_; }
		private:
			pointer ptr_;
		};


		VeVector(VeIndex align = 16, VeIndex capacity = 16 );
		~VeVector() { destruct(); delete[] m_memptr; };

		size_type size() { return m_size; };
		T& operator[](size_type index );
		const T& operator[](size_type index) const;
		iterator begin() { return iterator(m_startptr); }
		iterator end() { return iterator( m_startptr + m_size * m_entrySize ); }
		const_iterator begin() const { return const_iterator(m_startptr); }
		const_iterator end() const { return const_iterator(m_startptr + m_size * m_entrySize);  }

		void emplace_back(T &entry);
		void emplace_back(T &&entry);
		void push_back(T& entry);
		void push_back(T&& entry);
		void pop_back();
		void swap( VeIndex a, VeIndex b);
		bool empty() { return m_size == 0; };
	};

	template<typename T> inline VeVector<T>::VeVector( VeIndex align, VeIndex capacity ) {
		m_capacity = capacity > 16 ? capacity : 16;
		m_alignment = align > 16 ? align : 16;
		m_entrySize = (VeIndex)alignBoundary( sizeof(T), m_alignment);
		m_memptr = new uint8_t[m_capacity * m_entrySize + m_alignment];
		m_startptr = (uint8_t*) alignBoundary( (uint64_t)m_memptr, align );
		construct();
	}

	template<typename T> inline void VeVector<T>::emplace_back(T &entry) {
		if (m_size == m_capacity) doubleCapacity();
		uint8_t* ptr = m_startptr + m_size * m_entrySize;
		*(T*)ptr = entry;
		++m_size;
	}

	template<typename T> inline void VeVector<T>::emplace_back(T&& entry) {
		emplace_back(entry);
	}

	template<typename T> inline void VeVector<T>::push_back(T& entry) {
		emplace_back(entry);
	}

	template<typename T> inline void VeVector<T>::push_back(T&& entry) {
		emplace_back(entry);
	}

	template<typename T> inline void VeVector<T>::pop_back() {
		assert(m_size > 0);
		if (m_size > 0) --m_size;
	}

	template<typename T> inline T& VeVector<T>::operator[](size_type index) {
		assert(index < m_size);
		uint8_t* ptr;

		if (index < m_size) ptr = m_startptr + index * m_entrySize;
		else ptr = m_startptr;
		return *(T*)ptr;
	}

	template<typename T> inline const T &  VeVector<T>::operator[](size_type index) const {
		assert(index < m_size);
		uint8_t* ptr;

		if (index < m_size) ptr = m_startptr + index * m_entrySize;
		else ptr = m_startptr;
		return *(T*)ptr;
	}


	template<typename T> inline void VeVector<T>::swap(VeIndex a, VeIndex b) {
		uint8_t* ptra = m_startptr + a * m_entrySize;
		uint8_t* ptrb = m_startptr + b * m_entrySize;

		T buf = *(T*)ptrb;
		*(T*)ptrb = *(T*)ptra;
		*(T*)ptra = buf;
	}

}



