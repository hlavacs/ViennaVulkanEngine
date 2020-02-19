#pragma once

namespace vve {

	void testVector();
	uint64_t alignBoundary(uint64_t size, VeIndex alignment);


	template <typename T>
	class VeVector {

	protected:

		uint8_t* m_memptr;
		uint8_t* m_startptr;
		VeIndex  m_entrySize;
		VeIndex  m_size;
		VeIndex  m_capacity;
		VeIndex  m_alignment;

		void construct() {
			construct(m_startptr, m_capacity);
		}

		void construct( uint8_t* startptr, VeIndex capacity ) {
			for (uint32_t i = 0; i < capacity; ++i) {
				uint8_t* ptr = startptr + i * m_entrySize;
				T* tptr = new(ptr) T();
			}
		}

		void construct(uint8_t* start_dst, VeIndex entry_size_dst, uint8_t *start_src, VeIndex entry_size_src, VeIndex size_src) {
			for (uint32_t i = 0; i < size_src; ++i) {
				uint8_t* ptrd = start_dst + i * entry_size_dst;
				uint8_t* ptrs = start_src + i * entry_size_src;
				T* tptrd = new(ptrd) T( (T&) *ptrs );
			}
		}

		void copy(uint8_t* start_dst, VeIndex entry_size_dst, uint8_t* start_src, VeIndex entry_size_src, VeIndex size_src) {
			for (uint32_t i = 0; i < size_src; ++i) {
				uint8_t* ptrd = start_dst + i * entry_size_dst;
				uint8_t* ptrs = start_src + i * entry_size_src;
				*(T*)ptrd = *(T*)ptrs;
			}
		}

		void destruct() {
			if (m_memptr == nullptr || m_startptr == nullptr ) return;
			for (uint32_t i = 0; i < m_capacity; ++i) {
				T* tptr = (T*)(m_startptr + i * m_entrySize);
				tptr->~T();
			}
			delete[] m_memptr;
			m_memptr = nullptr;
			m_startptr = nullptr;
		}

		void setNewCapacity( VeIndex newcapacity, uint8_t *start_src, VeIndex entry_size_src, VeIndex size_src ) {
			newcapacity = newcapacity > size_src ? newcapacity : size_src;
			uint8_t* newmemptr = new uint8_t[newcapacity * m_entrySize + m_alignment];
			uint8_t* newstartptr = (uint8_t*)alignBoundary((uint64_t)newmemptr, m_alignment);

			construct(newstartptr, m_entrySize, start_src, entry_size_src, size_src );
			construct(newstartptr + size_src * m_entrySize, newcapacity - size_src);
			destruct();

			m_memptr = newmemptr;
			m_startptr = newstartptr;
			m_capacity = newcapacity;
		}

		void doubleCapacity() {
			setNewCapacity(2 * m_capacity, m_startptr, m_entrySize, m_size );
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
		VeVector(const VeVector& vec);
		VeVector(const VeVector&& vec);
		~VeVector() { destruct(); };

		void operator=( const VeVector & vec );

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
		m_capacity	= capacity > 16 ? capacity : 16;
		m_alignment = align > 16 ? align : 16;
		m_entrySize = (VeIndex)alignBoundary( sizeof(T), m_alignment);
		m_memptr	= new uint8_t[m_capacity * m_entrySize + m_alignment];
		m_startptr	= (uint8_t*) alignBoundary( (uint64_t)m_memptr, m_alignment);
		construct();
	}

	template<typename T> inline VeVector<T>::VeVector(const VeVector& vec) {
		m_capacity	= vec.m_capacity;
		m_size		= vec.m_size;
		m_alignment = vec.m_alignment;
		m_entrySize = vec.m_entrySize;
		m_memptr	= new uint8_t[m_capacity * m_entrySize + m_alignment];
		m_startptr	= (uint8_t*)alignBoundary((uint64_t)m_memptr, m_alignment);
		construct(m_startptr, m_entrySize, vec.m_startptr, vec.m_entrySize, vec.m_size);
		construct(m_startptr + m_size * m_entrySize, m_capacity - m_size );
	}

	template<typename T> inline VeVector<T>::VeVector( const VeVector&& vec) {
		VeVector(vec);
	}

	template<typename T> inline void VeVector<T>::operator=(const VeVector& vec) {
		if (m_capacity >= vec.m_size) {
			copy(m_startptr, m_entrySize, vec.m_startptr, vec.m_entrySize, vec.m_size);
			return;
		}
		setNewCapacity(vec.m_capacity, vec.m_startptr, vec.m_entrySize, vec.m_size );
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



