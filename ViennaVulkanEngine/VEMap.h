#pragma once



namespace vve {

	template<typename S, typename T>
	struct std::hash<std::pair<S, T>>
	{
		inline size_t operator()(const std::pair<S, T>& val) const
		{
			size_t seed = 0;
			seed ^= std::hash<S>()(val.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= std::hash<T>()(val.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			return seed;
		}
	};

	template<typename S, typename T, typename U>
	struct std::hash<std::tuple<S, T, U>>
	{
		inline size_t operator()(const std::tuple<S, T, U>& val) const
		{
			size_t seed = 0;
			seed ^= std::hash<S>()(std::get<0>(val)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= std::hash<T>()(std::get<1>(val)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= std::hash<T>()(std::get<2>(val)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			return seed;
		}
	};


	/**
	*
	* \brief
	*
	*
	*/
	class VeMap {
	protected:
		VeHeapMemory		m_heap;
		VeClock				m_clock;

	public:
		VeMap() : m_heap(), m_clock("Map Clock", 100) {};
		virtual ~VeMap() {};
		virtual void		operator=(const VeMap& map) { assert(false); return; };
		virtual void		clear() { assert(false); return; };
		virtual VeMap* clone() { assert(false); return nullptr; };

		virtual bool	getMappedIndexEqual(VeHandle key, VeIndex& index) { assert(false); return false; };
		virtual bool	getMappedIndexEqual(VeHandlePair key, VeIndex& index) { assert(false); return false; };
		virtual bool	getMappedIndexEqual(VeHandleTriple key, VeIndex& index) { assert(false); return false; };
		virtual bool	getMappedIndexEqual(std::string key, VeIndex& index) { assert(false); return false; };

		virtual VeCount	getMappedIndicesEqual(VeHandle key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual VeCount	getMappedIndicesEqual(VeHandlePair key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual VeCount	getMappedIndicesEqual(VeHandleTriple key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual VeCount	getMappedIndicesEqual(std::string key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };

		virtual VeCount	getMappedIndicesRange(VeHandle lower, VeHandle upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual VeCount	getMappedIndicesRange(VeHandlePair lower, VeHandlePair upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual VeCount	getMappedIndicesRange(VeHandleTriple lower, VeHandleTriple upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual VeCount	getMappedIndicesRange(std::string lower, std::string upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };

		virtual VeCount	getAllIndices(std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual bool	insertIntoMap(void* entry, VeIndex dir_index) { assert(false); return false; };
		virtual VeCount	deleteFromMap(void* entry, VeIndex dir_index) { assert(false); return 0; };

		virtual VeCount leftJoin(VeHandle key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return 0; };
		virtual VeCount leftJoin(VeHandlePair key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return 0; };
		virtual VeCount leftJoin(VeHandleTriple key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return 0; };
		virtual VeCount leftJoin(VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return 0; };


		/**
		*
		* \brief
		*
		* \param[in] entry
		* \param[in] offset
		* \param[in] numbytes
		* \returns
		*
		*/
		VeHandle getIntFromEntry(void* entry, VeIndex offset, VeIndex num_bytes) {
			uint8_t* ptr = (uint8_t*)entry + offset;

			if (num_bytes == 4) {
				uint32_t* k1 = (uint32_t*)ptr;
				return (VeHandle)*k1;
			}
			uint64_t* k2 = (uint64_t*)ptr;
			return (VeHandle)*k2;
		};

		/**
		*
		* \brief
		*
		* \param[in] entry
		* \param[in] offset
		* \param[in] numbytes
		* \param[out] key
		*
		*/
		void getKey(void* entry, VeIndex offset, VeIndex num_bytes, VeHandle& key) {
			key = getIntFromEntry(entry, offset, num_bytes);
		};

		/**
		*
		* \brief
		*
		* \param[in] entry
		* \param[in] offset
		* \param[in] numbytes
		* \returns
		*
		*/
		void getKey(void* entry, VeIndexPair offset, VeIndexPair num_bytes, VeHandlePair& key) {
			key = VeHandlePair(getIntFromEntry(entry, offset.first, num_bytes.first),
				getIntFromEntry(entry, offset.second, num_bytes.second));
		}

		/**
		*
		* \brief
		*
		* \param[in] entry
		* \param[in] offset
		* \param[in] numbytes
		* \returns
		*
		*/
		void getKey(void* entry, VeIndexTriple offset, VeIndexTriple num_bytes, VeHandleTriple& key) {
			key = VeHandleTriple(getIntFromEntry(entry, std::get<0>(offset), std::get<0>(num_bytes)),
				getIntFromEntry(entry, std::get<1>(offset), std::get<1>(num_bytes)),
				getIntFromEntry(entry, std::get<2>(offset), std::get<2>(num_bytes)));
		}

		/**
		*
		* \brief
		*
		* \param[in] entry
		* \param[in] offset
		* \param[in] numbytes
		* \returns
		*
		*/
		void getKey(void* entry, VeIndex offset, VeIndex num_bytes, std::string& key) {
			uint8_t* ptr = (uint8_t*)entry + offset;
			std::string* pstring = (std::string*)ptr;
			key = *pstring;
		}

	};


	//----------------------------------------------------------------------------------

	template <typename K, typename I>
	class VeOrderedMultimap : public VeMap {

		struct VeMapEntry {
			K			m_key;
			VeIndex		m_value = VE_NULL_INDEX;
			VeIndex		m_parent = VE_NULL_INDEX;	///<index of parent
			VeIndex		m_left = VE_NULL_INDEX;		///<index of first child with lower key
			VeIndex		m_right = VE_NULL_INDEX;	///<index of first child with larger key

			VeMapEntry() : m_key() {
				m_value = VE_NULL_INDEX;
				m_parent = VE_NULL_INDEX;	///<index of parent
				m_left = VE_NULL_INDEX;		///<index of first child with lower key
				m_right = VE_NULL_INDEX;	///<index of first child with larger key
			};

			VeMapEntry( K key, VeIndex value ) : m_key(key), m_value(value) {
				m_parent = VE_NULL_INDEX;	///<index of parent
				m_left = VE_NULL_INDEX;		///<index of first child with lower key
				m_right = VE_NULL_INDEX;	///<index of first child with larger key
			};

			void operator=(const VeMapEntry& entry) {
				m_key = entry.m_key;
				m_value = entry.m_value;
				m_parent = entry.m_parent;
				m_left = entry.m_left;
				m_right = entry.m_right;
			};

			bool operator==( const VeMapEntry& entry) {
				if( m_value != VE_NULL_INDEX && entry.m_value != VE_NULL_INDEX  )
					return m_key == entry.m_key && m_value == entry.m_value;

				return m_key == entry.m_key;
			};

			bool operator<(const VeMapEntry& entry) {
				if (m_value != VE_NULL_INDEX && entry.m_value != VE_NULL_INDEX)
					return m_key < entry.m_key || (m_key == entry.m_key && m_value < entry.m_value);

				if (m_value == VE_NULL_INDEX && entry.m_value != VE_NULL_INDEX) 
					return m_key == entry.m_key || m_key < entry.m_key;

				return m_key < entry.m_key;
			};

			bool operator<=(const VeMapEntry& entry) {
				return *this < entry || *this == entry;
			};

			bool operator>(const VeMapEntry& entry) {
				return !(*this == entry || *this < entry);
			};

			void print(uint32_t i) {
				std::cout << "IDX " << i;
				std::cout << " KEY " << m_key << " VAL " << m_value << std::endl;
				std::cout << " PAR " << m_parent << " LEFT " << m_left << " RIGHT " << m_right << std::endl << std::endl;
			};
		};

		VeOrderedMultimap<K, I>* clone() {
			VeOrderedMultimap<K, I>* map = new VeOrderedMultimap<K, I>(*this);
			return map;
		};

	protected:
		I						m_offset;					///
		I						m_num_bytes;				///
		VeVector<VeMapEntry>	m_map;						///
		VeIndex					m_root = VE_NULL_INDEX;
		VeCount					m_change_count = 0;


		//---------------------------------------------------------------------------
		//insert

		bool insert( VeIndex root, VeIndex index ) {
			if (m_root == VE_NULL_INDEX) {
				m_root = index;
				return false;
			}

			if (m_map[index] == m_map[root]) {
				return false;
			}

			if (m_map[index] < m_map[root]) {
				if (m_map[root].m_left != VE_NULL_INDEX) {
					return insert(m_map[root].m_left, index);
				}
				m_map[root].m_left = index;
				m_map[index].m_parent = root;
				return true;
			}

			if (m_map[root].m_right != VE_NULL_INDEX) {
				return insert(m_map[root].m_right, index);
			}
			m_map[root].m_right = index;
			m_map[index].m_parent = root;
			return true;
		};

		//---------------------------------------------------------------------------
		//rebalance

		void copyTree(VeIndex index, std::vector<VeMapEntry, custom_alloc<VeMapEntry> >& result) {
			if (index == VE_NULL_INDEX)
				return;

			copyTree(m_map[index].m_left, result);
			result.push_back(m_map[index]);
			copyTree(m_map[index].m_right, result);
		};


		void insertRange(int first, int last, std::vector<VeMapEntry, custom_alloc<VeMapEntry> > &map) {
			if (first > last)
				return;

			VeIndex middle = (first + last) / 2;

			m_map.emplace_back({ map[middle].m_key, map[middle].m_value });
			insert(m_root, (VeIndex)m_map.size()-1);

			insertRange( first, middle - 1, map);
			insertRange( middle + 1, last, map);
		};

		//---------------------------------------------------------------------------
		//find

		VeIndex findEntry(VeMapEntry &entry, VeIndex root) {
			if (root == VE_NULL_INDEX)
				return VE_NULL_INDEX;

			if (m_map[root] == entry )
				return root;

			VeIndex left = m_map[root].m_left;

			if (entry < m_map[root]) 
				return findEntry(entry, left);

			return findEntry(entry, m_map[root].m_right);
		};


		VeIndex find(K& key, VeIndex value) {
			VeMapEntry entry{ key, value };
			return findEntry(entry, m_root);
		};


		VeIndex find(K& key) {
			return find(key, VE_NULL_INDEX);
		};


		//---------------------------------------------------------------------------
		//delete

		void replaceChild(VeIndex parent, VeIndex old_child, VeIndex new_child) {
			if (parent == VE_NULL_INDEX) {
				m_root = new_child;
				return;
			}

			if (new_child != VE_NULL_INDEX)
				m_map[new_child].m_parent = parent;

			if (old_child == VE_NULL_INDEX)
				return;

			if (m_map[parent].m_left == old_child) {
				m_map[parent].m_left = new_child;
				return;
			}
			m_map[parent].m_right = new_child;
		};


		void deleteIndex(VeIndex index) {
			VeIndex last = m_map.size() - 1;

			if (index < last) {
				m_map.swap(index, last);

				VeIndex parent = m_map[index].m_parent;
				VeIndex left = m_map[index].m_left;
				VeIndex right = m_map[index].m_right;

				replaceChild(parent, last, index);
				replaceChild(index, left, left);
				replaceChild(index, right, right);
			}
			m_map.pop_back();
		};


		VeIndex findMin(VeIndex root) {
			assert(root != VE_NULL_INDEX);
			if (m_map[root].m_left == VE_NULL_INDEX) 
				return root;
			return findMin(m_map[root].m_left);
		};


		VeCount deleteEntry( VeMapEntry &entry, VeIndex root ) {
			VeIndex index = findEntry( entry, root);

			if (index != VE_NULL_INDEX ) {
				VeIndex parent = m_map[index].m_parent;
				VeIndex left = m_map[index].m_left;
				VeIndex right = m_map[index].m_right;

				if (left != VE_NULL_INDEX && right != VE_NULL_INDEX) {
					VeIndex min_idx = findMin(m_map[index].m_right);
					m_map[index].m_key = m_map[min_idx].m_key;
					m_map[index].m_value = m_map[min_idx].m_value;
					deleteEntry(m_map[min_idx], right);
					return 1;
				} 

				if (left == VE_NULL_INDEX && right == VE_NULL_INDEX)
					replaceChild(m_map[index].m_parent, index, VE_NULL_INDEX);

				if (left != VE_NULL_INDEX)
					replaceChild(parent, index, left);

				if (right != VE_NULL_INDEX)
					replaceChild(parent, index, right);

				deleteIndex(index);
				return 1;
			}
			return 0;
		};

		//---------------------------------------------------------------------------
		//get indices

		VeCount getAllIndices(VeIndex root, std::vector<VeIndex, custom_alloc<VeIndex>>& result) {
			if (root == VE_NULL_INDEX)
				return 0;

			VeCount num = getAllIndices(m_map[root].m_left, result);
			result.push_back(m_map[root].m_value);
			num += getAllIndices(m_map[root].m_right, result);
			return num + 1;
		};


		VeCount getMappedIndicesEqual(VeIndex root, VeMapEntry &entry, std::vector<VeIndex, custom_alloc<VeIndex>>& result) {
			if (root == VE_NULL_INDEX)
				return 0;

			VeCount num = 0;

			if(entry <= m_map[root])
				num += getMappedIndicesEqual(m_map[root].m_left, entry, result);

			if (entry.m_key == m_map[root].m_key) {
				result.push_back(root);
				++num;
			}

			if (m_map[root] <= entry)
				num += getMappedIndicesEqual(m_map[root].m_right, entry, result);
			return num;
		};


		VeCount getMappedIndicesRange(VeIndex root, VeMapEntry& lower, VeMapEntry upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) {
			if (root == VE_NULL_INDEX)
				return 0;

			if (m_map[root] < lower || m_map[root] > upper)
				return 0;

			VeCount num = 0;

			if (lower < m_map[root])
				num += getMappedIndicesRange(m_map[root].m_left, lower, upper, result);

			if (lower <= m_map[root] && m_map[root] <= upper) {
				result.push_back(m_map[root].m_value);
				++num;
			}

			if (m_map[root] < upper)
				num += getMappedIndicesRange(m_map[root].m_right, lower, upper, result);

			return num;
		};


		//---------------------------------------------------------------------------
		//debug

		void printTree(VeIndex root, VeIndex level) {
			if (root == VE_NULL_INDEX)
				return;

			printTree(m_map[root].m_left, level + 1);

			std::cout << std::setw(level) << " ";
			std::cout << "KEY " << m_map[root].m_key << " VAL " << m_map[root].m_value << std::endl;

			printTree(m_map[root].m_right, level + 1);
		};


	public:

		VeOrderedMultimap(I offset, I num_bytes, bool memcopy = false) : VeMap(), m_map(memcopy), m_offset(offset), m_num_bytes(num_bytes) {};
		VeOrderedMultimap(const VeOrderedMultimap<K, I>& map) : VeMap(), m_map(map.m_map), m_offset(map.m_offset), m_num_bytes(map.m_num_bytes) {};
		virtual	~VeOrderedMultimap() {};

		virtual void operator=(const VeMap& basemap) {
			VeOrderedMultimap<K, I>* map = &((VeOrderedMultimap<K, I>&)basemap);
			m_offset = map->m_offset;
			m_num_bytes = map->m_num_bytes;
			m_map = map->m_map;
		};

		virtual void clear() {
			m_root = VE_NULL_INDEX;
			m_map.clear();
		};

		void rebalanceTree() {
			std::vector<VeMapEntry, custom_alloc<VeMapEntry> > map(&m_heap);
			map.reserve(m_map.size());
			copyTree(m_root, map);
			clear();
			insertRange(0, (VeIndex)map.size() - 1, map);
		};

		void printTree() {
			std::cout << "Root " << m_root << std::endl;
			printTree(m_root, 1);
			std::cout << std::endl;
		}

		void print() {
			std::cout << "Root " << m_root << std::endl;
			for (uint32_t i = 0; i < m_map.size(); ++i) {
				m_map[i].print(i);
			}
		}

		virtual bool getMappedIndexEqual(K key, VeIndex& index) override {
			index = find(key);
			return index == VE_NULL_INDEX ? VE_NULL_INDEX : m_map[index].m_value;
		};

		virtual VeCount getMappedIndicesEqual(K key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			VeMapEntry entry(key, VE_NULL_INDEX);
			return getMappedIndicesEqual(m_root, entry, result);
		};

		virtual VeCount getMappedIndicesRange(K lower, K upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			VeMapEntry mlower(lower, VE_NULL_INDEX), mupper(upper, VE_NULL_INDEX);
			return getMappedIndicesRange(m_root, mlower, mupper, result);
		};

		virtual VeCount getAllIndices(std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			return getAllIndices( m_root, result );
		}

		VeCount leftJoin(K key, VeOrderedMultimap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) {
			std::vector < VeIndex, custom_alloc<VeIndex> > result1(&m_heap);
			getMappedIndicesEqual(key, result1);

			std::vector < VeIndex, custom_alloc<VeIndex> > result2(&m_heap);
			other.getMappedIndicesEqual(key, result2);

			for (uint32_t i = 0; i < result1.size(); ++i) {
				for (uint32_t j = 0; j < result2.size(); ++j) {
					result.emplace_back({ result1[i], result2[j] });
				}
			}
			return result1.size()*result2.size();
		};

		VeCount leftJoin(VeOrderedMultimap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) {
			if (m_map.size() == 0 ||other.m_map.size()==0)
				return 0;

			VeCount num = 0;
			std::vector<VeMapEntry, custom_alloc<VeMapEntry> > map1(&m_heap);
			map1.reserve(m_map.size());
			copyTree(m_root, map1);

			K last = map1[0].m_key;
			std::vector < VeIndex, custom_alloc<VeIndex> > result2(&m_heap);
			other.getMappedIndicesEqual(last, result2);

			for (uint32_t i = 0; i < map1.size(); ++i) {
				K key = m_map[i].m_key;
				if (key != last) {
					result2.clear();
					other.getMappedIndicesEqual(key, result2);
					last = key;
				}
				for (uint32_t j = 0; j < result2.size(); ++j) {
					result.emplace_back({ map1[i].m_value, result2[j]});
					++num;
				}
			}

			return num;
		};

		virtual bool insertIntoMap(void* entry, VeIndex dir_index) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);
			VeIndex last = m_map.size();
			m_map.emplace_back({ key, dir_index });
			insert(m_root, last);
			++m_change_count;
			return true;
		};

		virtual VeCount deleteFromMap(void* entry, VeIndex dir_index) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);
			VeMapEntry mentry(key, dir_index);
			++m_change_count;
			return deleteEntry(mentry, m_root);
		};

	};


	namespace map {
		void testMap();
	}

}


