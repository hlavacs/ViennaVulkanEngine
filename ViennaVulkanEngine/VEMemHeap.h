#pragma once

/**
*
* \file
* \brief Declares and defines the VeHeapMemory class and a custom allocator for STL containers.
*
*/


namespace vve {

	/**
	* \brief This class manages fast cache memory for temporary data, e.g. handling handles in searches
	*
	* VeHeapMemory organizes memory allocations intrinsically, i.e. the management information
	* is part of the allocated memory. Memory id allocated from system heap as large chunks called pools.
	* In each pool, there can be suballocations as blocks of contiguous memory. At the head of each block,
	* there is one VeMemBlock struct. Freed blocks are linked into a free list and can be reused in O(1).
	* If there is no free space in a pool, then first the pool runs a defragmentation operations,
	* merging neigbouring free blocks into larger free blocks. If there is still no free space then
	* the next pool is tried. If there is no memory available, then eventually a new pool is allocated
	* that holds enough space.
	* VeHeaps do not release their memory, but keep it until the end of the program.
	* 
	*/
	class VeHeapMemory {

		static const VeIndex m_stdSize = 1 << 12;	///< Default size of one block

		/**
		* \brief One contiguous block of memory.
		*/
		struct VeMemBlock {
			bool	m_free = true;				///< if true then this block is free
			VeIndex m_size = 0;					///< size of the block
			VeIndex m_next = VE_NULL_INDEX;		///< if free, then index to the next free block
		};

		/**
		* \brief A memory pool containing N suballocated blocks
		*/
		struct VePool {
			std::vector<uint8_t>	m_pool;			///< bare memory made of unsigned bytes
			VeIndex					m_first_free;	///< first block in the free list
			VeIndex					m_free_bytes;	///< Number of free bytes in the pool

			/**
			* \brief VePool class constructor
			*
			* \param[in] size Size of the pool to be allocated
			* 
			*/
			VePool(std::size_t size = m_stdSize) : m_pool(size), m_first_free(0), m_free_bytes((VeIndex)size - sizeof(VeMemBlock)) {
				//std::cout << "new pool size " << size << std::endl;
				VeMemBlock* block = (VeMemBlock*)m_pool.data();					///< points to the pool memory
				block->m_free = true;											// create one single free block at the start
				block->m_size = (VeIndex)(m_pool.size() - sizeof(VeMemBlock));
				block->m_next = VE_NULL_INDEX;
			};

			/**
			* \brief Turn an index into a pool into a VeMemBlock pointer
			*
			* \param[in] index Index of the pool 
			* \returns a pointer to the location pointed at by index of type VeMemBlock*
			*
			*/
			VeMemBlock* getPtr(VeIndex index) {
				return (VeMemBlock*)&m_pool[index];
			};

			/**
			* \brief Print debug information
			*/
			void print() {
				VeIndex size = 0;
				std::cout << "first free " << m_first_free << std::endl;
				for (VeIndex current_block = 0; current_block < m_pool.size(); ) {
					VeMemBlock* pCurrent = getPtr(current_block);
					std::cout << current_block << " " << pCurrent->m_free << " " << pCurrent->m_size << " next " << pCurrent->m_next << std::endl;
					size += pCurrent->m_size + sizeof(VeMemBlock);
					current_block += pCurrent->m_size + sizeof(VeMemBlock);
				}
				std::cout << "total size " << size << std::endl << std::endl;
			};

			/**
			* \brief Alocate a block with given size from a free block that is large enough
			*
			* \param[in] prev_block Pointer to previous free block (that was not large enough), used for relinking the free list
			* \param[in] free_block A block from the free list that is large enough
			* \param[in] size Size of the block to allocate, turn the rest into a free block
			* \returns a pointer to the allocated memory
			*
			*/
			void* makeAllocation(VeIndex prev_block, VeIndex free_block, std::size_t size) {
				VeMemBlock* pFree = getPtr(free_block);		///< Pointer to this free block
				VeIndex next = pFree->m_next;				///< Index of next free block
				if (prev_block == VE_NULL_INDEX) {			//if first free block in free list
					m_first_free = next;					//relink with free list start
				}
				else {
					getPtr(prev_block)->m_next = next;		//relink with previous free block
				}

				pFree->m_free = false;				//block is no longer free
				m_free_bytes -= pFree->m_size;		//reduce number of free bytes through allocation of this block

				if (pFree->m_size - size >= sizeof(VeMemBlock)) {	//if larger than size split rest and make it a new free block 
					VeIndex leftOver = (VeIndex)(free_block + sizeof(VeMemBlock) + size);	//index of new block containing left over bytes

					VeMemBlock* block = (VeMemBlock*)getPtr(leftOver);						//new free block
					block->m_free = true;													//is free
					block->m_size = (VeIndex)(pFree->m_size - size - sizeof(VeMemBlock));	//size is left over
					block->m_next = m_first_free;											//relink with start
					m_first_free = leftOver;												//relink with start
					m_free_bytes += block->m_size;											//account for free bytes

					pFree->m_size = (VeIndex)size;	//set size of newly allocated block
				}
				else {
					//leave the size as it is
				}

				void* result = (uint8_t*)pFree + sizeof(VeMemBlock);	//actual memory if start of block + header size
				return result;
			};

			/**
			* \brief Allocate a block with given size from the pool
			*
			* \param[in] size Size of the block to allocate
			* \returns a pointer to the newly allocated memory of given size
			*
			*/
			void* allocate(std::size_t size) {
				//std::cout << "allocate " << size << std::endl;
				VeIndex prev_free = VE_NULL_INDEX;										///< Index of previous free block
				for (VeIndex next_free = m_first_free; next_free != VE_NULL_INDEX; ) {	//iterate through all free blocks
					VeMemBlock* free_block = getPtr(next_free);							///< Pointer to next free block
					assert(free_block->m_free);

					if (free_block->m_size >= size) {	//is the block large enough?
						//print();
						void* ptr = makeAllocation(prev_free, next_free, size);	///< if yes then make an allocation 
						//print();
						return ptr;	//return result
					}
					prev_free = next_free;			//not large enough, remember last free block
					next_free = free_block->m_next;	//use next free block
				}
				return nullptr;	//none free found that is large enough - return nullptr
			};

			/**
			* \brief Deallocate a block pointed at by a pointer
			*
			* \param[in] p Pointer to the memory of a 			
			* \param[in] size Not used
			*
			*/
			void deallocate(void* p, std::size_t size) {
				//std::cout << "deallocate " << size << std::endl;
				VeMemBlock* block = (VeMemBlock*)((uint8_t*)p - sizeof(VeMemBlock));	///< pointer to block header
				assert(!block->m_free);
				block->m_free = true;			//set to free
				block->m_next = m_first_free;	//relink into free list
				m_first_free = (VeIndex)((uint8_t*)p - m_pool.data() - sizeof(VeMemBlock)); //relink into free list 
				m_free_bytes += block->m_size;	//account for the free bytes of the block
				//print();
			};

			/**
			* \brief Defragment the pool by merging neighbouring free blocks into larger free blocks
			*/
			void defragment() {
				m_first_free = VE_NULL_INDEX;	//reset the whole free list
				VeMemBlock* pPrev = nullptr;	///< points to previous block for list linking
				for (VeIndex current_block = 0; current_block < m_pool.size(); ) {	//go through all blocks linearly
					VeMemBlock* pCurrent = getPtr(current_block);					//pointer to current block
					VeIndex next_block = current_block + pCurrent->m_size + sizeof(VeMemBlock); //index of next block

					if (pCurrent->m_free) {						//is current block free?

						if (m_first_free == VE_NULL_INDEX) {	//if first free block, set as first free block of the pool
							m_first_free = current_block;
						}
						else if (pPrev != nullptr) {			//if not first block, relink with previous free block
							pPrev->m_next = current_block;	
						}

						pCurrent->m_next = VE_NULL_INDEX;		//set next free block tu NULL

						if (next_block < m_pool.size()) {			//is there a next block?
							VeMemBlock* pNext = getPtr(next_block); ///< Pointer to next block in pool
							if (pNext->m_free) {					//is the next block also free?
								pCurrent->m_size += pNext->m_size + sizeof(VeMemBlock);	//if yes then merge it into the current block
								m_free_bytes += sizeof(VeMemBlock); //just got more free memory because one less block header
								pCurrent = pPrev;				//make sure that the iterator goes to the current block again
								next_block = current_block;		//iterator will stay on this block
							}
						}
						pPrev = pCurrent;			//pointer to previous block (or same if there was a merge)
					}
					current_block = next_block;		//index of next block (or same if there was a merge)
				}
			};

			/**
			* \brief Clears all memory allocations of the pool
			*/
			void clear() {
				VeMemBlock* block = (VeMemBlock*)m_pool.data();					///< Insert one free block at the start
				block->m_free = true;											//set to free
				block->m_size = (VeIndex)(m_pool.size() - sizeof(VeMemBlock));	//size of data part
				block->m_next = VE_NULL_INDEX;									//no successor in free list
				m_first_free = 0;												//start of first free block is start of pool
				m_free_bytes = (VeIndex)m_pool.size() - sizeof(VeMemBlock);				//number of free bytes in the pool
			}
		};

		std::vector<VePool>	m_pools;	///< A vector containing all existing pools for this heap

	public:
		VeHeapMemory() : m_pools() {			///< VeHeapMemory class constructor
			m_pools.emplace_back(m_stdSize);	//insert one pool with default size
		};

		~VeHeapMemory() {};	///< VeHeapMemory class destructor

		/**
		* \brief Print debug information for all pools
		*/
		void print() {
			for (uint32_t i = 0; i < m_pools.size(); ++i) {
				std::cout << "pool " << i << " size " << m_pools[i].m_pool.size() << std::endl;
				m_pools[i].print();
			}
		}

		/**
		* \brief Clears all memory allocations of all pools
		*/
		void clear() {
			for (auto& pool : m_pools)
				pool.clear();
		};

		/**
		* \brief Public interface for allocating memory of this heap
		*
		* \param[in] size Size of the memory block that should be allocated
		* \returns a pointer to the newly allocated memory
		*
		*/
		void* allocate(std::size_t size) {
			for (auto& pool : m_pools) {				//go through all pools and find a memory block large enough
				if (pool.m_free_bytes >= size) {		//are there enough free bytes in the pool?
					void* ptr = pool.allocate(size);	//ask the pool for memory
					if (ptr != nullptr)					//if found return the memory
						return ptr;

					pool.defragment();					//if not found try to defragment the pool

					ptr = pool.allocate(size);			//and try again to allocate the memory
					if (ptr != nullptr)
						return ptr;
				}
			}
			//not found a free pool, allocate a new pool
			std::size_t ps = m_stdSize;					//default pool size
			if (ps < size + sizeof(VeMemBlock)) {		//if desired memory would not fit into a standard pool -> make it larger
#ifdef _WIN32
				ps = 2 * size + size / 2 + 2 * sizeof(VeMemBlock); //std::vector reallocation strategy on Windows 
#else
				ps = 3 * size + 2 * sizeof(VeMemBlock); //std::vector reallocation strategy on Unix-like
#endif
			}

			m_pools.emplace_back(ps);							//put new pool into list of pools
			return m_pools[m_pools.size() - 1].allocate(size);	//allocate the block from the new pool
		};

		/**
		* \brief Public interface for deallocating memory
		*
		* \param[in] p Pointer to the memory to be deallocated
		* \param[in] size Not used
		*
		*/
		void deallocate(void* p, std::size_t size) {
			for (auto& pool : m_pools) {	//find pool that this pointer points to
				if (pool.m_pool.data() <= p && p < pool.m_pool.data() + pool.m_pool.size()) {
					pool.deallocate(p, size);	//if found, deallocate the memory
				}
			}
		}
	};


	/**
	* \brief custom_alloc is a custom allocator for STL containers, tha uses VeHeap
	*/
	template <class T>
	class custom_alloc : public std::allocator<T> {
	public:
		VeHeapMemory* m_heap;	//< Pointer to the heap memory to be used

	public:
		typedef size_t		size_type;			///< size type
		typedef ptrdiff_t	difference_type;	///< pointer difference type
		typedef T*			pointer;			///< pointer type
		typedef const T*	const_pointer;		///< const pointer type
		typedef T&			reference;			///< reference type
		typedef const T&	const_reference;	///< const reference type
		typedef T			value_type;			///< value type

		custom_alloc(VeHeapMemory* heap) : m_heap(heap) {}
		custom_alloc(const custom_alloc& ver) : m_heap(ver.m_heap) {}

		pointer allocate(size_type n, const void* = 0) {
			T* t = (T*)m_heap->allocate(n * sizeof(T));
			return t;
		};

		void deallocate(void* p, size_type n) {
			m_heap->deallocate(p, n * sizeof(T));
		};

		pointer           address(reference x) const { return &x; }
		const_pointer     address(const_reference x) const { return &x; }
		custom_alloc<T>& operator=(const custom_alloc&) { return *this; }
		void              construct(pointer p, const T& val) {
			new ((T*)p) T(val);
		};

		void              destroy(pointer p) { p->~T(); };

		size_type         max_size() const { return size_t(-1); }

		template <class U>
		struct rebind { typedef custom_alloc<U> other; };

		template <class U>
		custom_alloc(const custom_alloc<U>& ver) : m_heap(ver.m_heap) {}

		template <class U>
		custom_alloc& operator=(const custom_alloc<U>& ver) : m_heap(ver.m_heap) { return *this; }
	};


}

