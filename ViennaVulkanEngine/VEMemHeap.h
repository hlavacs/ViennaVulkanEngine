#pragma once



//----------------------------------------------------------------------------------


namespace vve {

	class VeHeapMemory {

		static const VeIndex m_stdSize = 1 << 13;

		struct VeMemBlock {
			bool	m_free = true;
			VeIndex m_size = 0;
			VeIndex m_next = VE_NULL_INDEX;
		};

		struct VePool {
			std::vector<uint8_t>	m_pool;
			VeIndex					m_first_free;

			VePool(std::size_t size = m_stdSize) : m_pool(size), m_first_free(0) {
				//std::cout << "new pool size " << size << std::endl;
				VeMemBlock* block = (VeMemBlock*)m_pool.data();
				block->m_free = true;
				block->m_size = (VeIndex)(m_pool.size() - sizeof(VeMemBlock));
				block->m_next = VE_NULL_INDEX;
			};

			VeMemBlock* getPtr(VeIndex index) {
				return (VeMemBlock*)&m_pool[index];
			};

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

			void* makeAllocation(VeIndex prev_block, VeIndex free_block, std::size_t size) {
				VeMemBlock* pFree = getPtr(free_block);
				VeIndex next = pFree->m_next;
				if (prev_block == VE_NULL_INDEX) {
					m_first_free = next;
				}
				else {
					getPtr(prev_block)->m_next = next;
				}

				pFree->m_free = false;

				if (pFree->m_size - size >= sizeof(VeMemBlock)) {
					VeIndex leftOver = (VeIndex)(free_block + sizeof(VeMemBlock) + size);

					VeMemBlock* block = (VeMemBlock*)getPtr(leftOver);
					block->m_free = true;
					block->m_size = (VeIndex)(pFree->m_size - size - sizeof(VeMemBlock));
					block->m_next = m_first_free;
					m_first_free = leftOver;

					pFree->m_size = (VeIndex)size;
				}
				else {
					//leave the size as it is
				}

				void* result = (uint8_t*)pFree + sizeof(VeMemBlock);
				return result;
			};

			void* allocate(std::size_t size) {
				//std::cout << "allocate " << size << std::endl;
				VeIndex prev_free = VE_NULL_INDEX;
				for (VeIndex next_free = m_first_free; next_free != VE_NULL_INDEX; ) {
					VeMemBlock* free_block = getPtr(next_free);
					assert(free_block->m_free);

					if (free_block->m_size >= size) {
						//print();
						void* ptr = makeAllocation(prev_free, next_free, size);
						//print();
						return ptr;
					}
					prev_free = next_free;
					next_free = free_block->m_next;
				}
				return nullptr;
			};

			void deallocate(void* p, std::size_t size) {
				//std::cout << "deallocate " << size << std::endl;
				VeMemBlock* block = (VeMemBlock*)((uint8_t*)p - sizeof(VeMemBlock));
				assert(!block->m_free);
				block->m_free = true;
				block->m_next = m_first_free;
				m_first_free = (VeIndex)((uint8_t*)p - m_pool.data() - sizeof(VeMemBlock));
				//print();
			};

			void defragment() {
				m_first_free = VE_NULL_INDEX;
				VeMemBlock* pPrev = nullptr;
				for (VeIndex current_block = 0; current_block < m_pool.size(); ) {
					VeMemBlock* pCurrent = getPtr(current_block);
					VeIndex next_block = current_block + pCurrent->m_size + sizeof(VeMemBlock);

					if (pCurrent->m_free) {

						if (m_first_free == VE_NULL_INDEX) {
							m_first_free = current_block;
						}
						else if (pPrev != nullptr) {
							pPrev->m_next = current_block;
						}

						pCurrent->m_next = VE_NULL_INDEX;

						if (next_block < m_pool.size()) {
							VeMemBlock* pNext = getPtr(next_block);
							if (pNext->m_free) {
								pCurrent->m_size += pNext->m_size + sizeof(VeMemBlock);
								pCurrent = pPrev;
								next_block = current_block;
							}
						}
						pPrev = pCurrent;
					}
					current_block = next_block;
				}
			};

			void reset() {
				VeMemBlock* block = (VeMemBlock*)m_pool.data();
				block->m_free = true;
				block->m_size = (VeIndex)(m_pool.size() - sizeof(VeMemBlock));
				block->m_next = VE_NULL_INDEX;
				m_first_free = 0;
			}
		};

		std::vector<VePool>	m_pools;

	public:
		VeHeapMemory() : m_pools() {
			m_pools.emplace_back(m_stdSize);
		};

		~VeHeapMemory() {};

		void print() {
			for (uint32_t i = 0; i < m_pools.size(); ++i) {
				std::cout << "pool " << i << " size " << m_pools[i].m_pool.size() << std::endl;
				m_pools[i].print();
			}
		}

		void reset() {
			for (auto& pool : m_pools)
				pool.reset();
		};

		void* allocate(std::size_t size) {
			for (auto& pool : m_pools) {
				void* ptr = pool.allocate(size);
				if (ptr != nullptr)
					return ptr;

				pool.defragment();

				ptr = pool.allocate(size);
				if (ptr != nullptr)
					return ptr;
			}
			std::size_t ps = m_stdSize;
			if (ps < size + sizeof(VeMemBlock)) {
#ifdef _WIN32
				ps = 2 * size + size / 2 + 2 * sizeof(VeMemBlock);
#else
				ps = 3 * size + 2 * sizeof(VeMemBlock);
#endif
			}

			m_pools.emplace_back(ps);
			return m_pools[m_pools.size() - 1].allocate(size);
		};

		void deallocate(void* p, std::size_t size) {
			for (auto& pool : m_pools) {
				if (pool.m_pool.data() <= p && p < pool.m_pool.data() + pool.m_pool.size()) {
					pool.deallocate(p, size);
				}
			}
		}
	};


	template <class T>
	class custom_alloc : public std::allocator<T> {
	public:
		VeHeapMemory* m_heap;

	public:
		typedef size_t		size_type;
		typedef ptrdiff_t	difference_type;
		typedef T* pointer;
		typedef const T* const_pointer;
		typedef T& reference;
		typedef const T& const_reference;
		typedef T			value_type;

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

