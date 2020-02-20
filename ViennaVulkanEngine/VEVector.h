#pragma once

namespace vve {

	namespace vec {
		void testVector();
	}

	uint64_t alignBoundary(uint64_t size, VeIndex alignment);


	template <typename T>
	class VeVector {

	protected:

		uint8_t* m_memptr;		///<address of the memory space that was allocated or nullptr 
		uint8_t* m_startptr;	///<address where the first entry lies, or nullptr. Is memory aligned.
		VeIndex  m_entrySize;	///<size of a entry, or more for alignment
		VeIndex  m_size;		///<number of entries that are currently in the vector
		VeIndex  m_capacity;	///<max number of entries that can be stored in the vector
		VeIndex  m_alignment;	///<memory alignment of each entry, default is 16
		bool	 m_memcpy;		///<true...no need for constructors, can use memcpy, false...need constructors

		/**
		*
		*	\brief When a vector of entries is created, the constructor must be called for all entries
		*	in the vector. This calls construct() for all entries.
		*
		*/
		void construct() {
			construct(m_startptr, m_capacity);	//Call with whole range of entries
		}

		/**
		*
		*	\brief When a vector of entries is created, the constructor must be called for all entries
		*	in the vector. This function goes through a number of entries and calls their constructor.
		*
		*	\param[in] startptr Points to the first entry
		*	\param[in] capacity Number of entries that are constructed 
		*
		*/
		void construct( uint8_t* startptr, VeIndex capacity ) {
			for (uint32_t i = 0; i < capacity; ++i) {		//go through all entries
				uint8_t* ptr = startptr + i * m_entrySize;	//point to the entry address
				T* tptr = new(ptr) T();						//call empty constructor on it
			}
		}

		/**
		*
		*	\brief Constructs a set of entries by copying data from source entries of same type
		*
		*	\param[in] 
		*	\param[in]
		*	\param[in]
		*	\param[in]
		*	\param[in]
		*
		*/
		void construct(uint8_t* start_dst, VeIndex entry_size_dst, uint8_t *start_src, VeIndex entry_size_src, VeIndex size_src) {
			for (uint32_t i = 0; i < size_src; ++i) {
				uint8_t* ptrd = start_dst + i * entry_size_dst;
				uint8_t* ptrs = start_src + i * entry_size_src;
				T* tptrd = new(ptrd) T( (T&) *ptrs );
			}
		}

		/**
		*
		*	\brief
		*
		*	\param[in]
		*	\param[in]
		*	\param[in]
		*	\param[in]
		*	\param[in]
		*
		*/
		void copy(uint8_t* start_dst, VeIndex entry_size_dst, uint8_t* start_src, VeIndex entry_size_src, VeIndex size_src) {
			for (uint32_t i = 0; i < size_src; ++i) {
				uint8_t* ptrd = start_dst + i * entry_size_dst;
				uint8_t* ptrs = start_src + i * entry_size_src;
				*(T*)ptrd = *(T*)ptrs;
			}
			m_size = size_src;
		}

		/**
		*
		*	\brief 
		*
		*
		*
		*/
		void destruct() {
			if (m_memptr == nullptr || m_startptr == nullptr ) return;
			for (uint32_t i = 0; !m_memcpy && i < m_capacity; ++i) {
				T* tptr = (T*)(m_startptr + i * m_entrySize);
				tptr->~T();
			}
			delete[] m_memptr;
			m_memptr = nullptr;
			m_startptr = nullptr;
		}

		/**
		*
		*	\brief
		*
		*	\param[in]
		*	\param[in]
		*	\param[in]
		*	\param[in]
		*	\param[in]
		*
		*/
		void setNewCapacity( VeIndex newcapacity, uint8_t *start_src, VeIndex entry_size_src, VeIndex size_src ) {
			newcapacity = std::max( newcapacity, size_src);
			uint8_t* newmemptr = new uint8_t[newcapacity * m_entrySize + m_alignment];
			uint8_t* newstartptr = (uint8_t*)alignBoundary((uint64_t)newmemptr, m_alignment);

			if (m_memcpy && m_entrySize == entry_size_src) {
				memcpy(newstartptr, start_src, size_src * entry_size_src);
			}
			else {
				construct(newstartptr, m_entrySize, start_src, entry_size_src, size_src);
				construct(newstartptr + size_src * m_entrySize, newcapacity - size_src);
			}
			destruct();

			m_memptr = newmemptr;
			m_startptr = newstartptr;
			m_capacity = newcapacity;
			m_size = size_src;
		}

		/**
		*
		*	\brief
		*
		*
		*
		*/
		void doubleCapacity() {
			setNewCapacity(2 * m_capacity, m_startptr, m_entrySize, m_size );
		}

	public:

		typedef VeIndex size_type;

		/**
		*
		*	\brief
		*
		*
		*
		*/
		class iterator {
		public:
			typedef iterator self_type;
			typedef T value_type;
			typedef T& reference;
			typedef T* pointer;
			typedef std::forward_iterator_tag iterator_category;
			typedef int difference_type;
			iterator(uint8_t* ptr, VeIndex entrySize) : m_ptr((T*)ptr), m_entrySize(entrySize) { }
			self_type operator++() { self_type i = *this; m_ptr = (T*) ((uint8_t*)m_ptr + m_entrySize); return i; }
			self_type operator++(int junk) { m_ptr = (T*)((uint8_t*)m_ptr + m_entrySize); return *this; }
			reference operator*() { return *m_ptr; }
			pointer operator->() { return m_ptr; }
			bool operator==(const self_type& rhs) { return m_ptr == rhs.m_ptr; }
			bool operator!=(const self_type& rhs) { return m_ptr != rhs.m_ptr; }
		private:
			pointer m_ptr;
			VeIndex m_entrySize;
		};

		/**
		*
		*	\brief
		*
		*
		*
		*/
		class const_iterator {
		public:
			typedef const_iterator self_type;
			typedef T value_type;
			typedef T& reference;
			typedef T* pointer;
			typedef int difference_type;
			typedef std::forward_iterator_tag iterator_category;
			const_iterator(uint8_t *ptr, VeIndex entrySize) : m_ptr((T*)ptr), m_entrySize(entrySize) { }
			self_type operator++() { self_type i = *this; m_ptr = (T*)((uint8_t*)m_ptr + m_entrySize); return i; }
			self_type operator++(int junk) { m_ptr = (T*)((uint8_t*)m_ptr + m_entrySize); return *this; }
			const reference operator*() { return *m_ptr; }
			const pointer operator->() { return m_ptr; }
			bool operator==(const self_type& rhs) { return m_ptr == rhs.m_ptr; }
			bool operator!=(const self_type& rhs) { return m_ptr != rhs.m_ptr; }
		private:
			pointer m_ptr;
			VeIndex m_entrySize;
		};

		VeVector(bool memcopy = false, VeIndex align = 16, VeIndex capacity = 16 );	///<Main vector constructor creates an empty vector
		VeVector(const VeVector& vec);							///<Copy constructor
		VeVector(const VeVector&& vec);							///<Copy constructor
		~VeVector() { destruct(); };							///<Destructor destructs all structs, then deletes the memory

		void operator=( const VeVector & vec );					///<assignment operator

		size_type size() { return m_size; };					///<returns number of entries in the vector
		T& operator[](size_type index );						///<operator to access entry
		const T& operator[](size_type index) const;				///<const access operator
		iterator begin() { return iterator(m_startptr, m_entrySize ); }	///<begin for iterator
		iterator end() { return iterator( m_startptr + m_size * m_entrySize, m_entrySize); }	///<end for iterator
		const_iterator begin() const { return const_iterator(m_startptr, m_entrySize); }		///<const iterator begin
		const_iterator end() const { return const_iterator(m_startptr + m_size * m_entrySize, m_entrySize);  } ///<const iterator end

		void emplace_back(T &entry);			///<add entry at end of vector
		void emplace_back(T &&entry);			///<add entry at end of vector
		void push_back(T& entry);				///<add entry at end of vector
		void push_back(T&& entry);				///<add entry at end of vector
		void pop_back();						///<remove last entry from back
		void swap( VeIndex a, VeIndex b);		///<swap two entries
		bool empty() { return m_size == 0; };	///<test whether vector is empty
	};

	template<typename T> inline VeVector<T>::VeVector(bool memcopy, VeIndex align, VeIndex capacity ) {
		m_capacity	= std::max(capacity, (VeIndex)16); 
		m_alignment = std::max(align, (VeIndex)16);
		m_entrySize = (VeIndex)alignBoundary( sizeof(T), m_alignment);
		m_memptr	= new uint8_t[m_capacity * m_entrySize + m_alignment];
		m_startptr	= (uint8_t*) alignBoundary( (uint64_t)m_memptr, m_alignment);
		m_memcpy	= memcopy;
		if( !m_memcpy ) construct();
	}

	template<typename T> inline VeVector<T>::VeVector(const VeVector& vec) {
		m_capacity	= vec.m_capacity;
		m_size		= vec.m_size;
		m_alignment = vec.m_alignment;
		m_entrySize = vec.m_entrySize;
		m_memcpy	= vec.m_memcpy;
		m_memptr	= new uint8_t[m_capacity * m_entrySize + m_alignment];
		m_startptr	= (uint8_t*)alignBoundary((uint64_t)m_memptr, m_alignment);
		if (m_memcpy) {
			memcpy(m_startptr, vec.m_startptr, m_size * m_entrySize);
		}
		else {
			construct(m_startptr, m_entrySize, vec.m_startptr, vec.m_entrySize, vec.m_size);
			construct(m_startptr + m_size * m_entrySize, m_capacity - m_size);
		}
	}

	template<typename T> inline VeVector<T>::VeVector( const VeVector&& vec) {
		VeVector(vec);
	}

	template<typename T> inline void VeVector<T>::operator=(const VeVector& vec) {
		if (m_capacity >= vec.m_size) {
			if (m_memcpy && m_entrySize == vec.m_entrySize) memcpy(m_startptr, vec.m_startptr, vec.m_size * vec.m_entrySize);
			else copy(m_startptr, m_entrySize, vec.m_startptr, vec.m_entrySize, vec.m_size);
			return;
		}
		setNewCapacity(vec.m_capacity, vec.m_startptr, vec.m_entrySize, vec.m_size);		
	}

	template<typename T> inline void VeVector<T>::emplace_back(T &entry) {
		if (m_size == m_capacity) doubleCapacity();
		uint8_t* ptr = m_startptr + m_size * m_entrySize;
		*(T*)ptr = entry;
		++m_size;
	}

	template<typename T> inline void VeVector<T>::emplace_back(T&& entry) {
		if (m_size == m_capacity) doubleCapacity();
		uint8_t* ptr = m_startptr + m_size * m_entrySize;
		*(T*)ptr = std::move(entry);
		++m_size;
	}

	template<typename T> inline void VeVector<T>::push_back(T& entry) {
		emplace_back(entry);
	}

	template<typename T> inline void VeVector<T>::push_back(T&& entry) {
		emplace_back(std::forward<T>(entry));
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



