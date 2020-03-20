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

		virtual bool		getMappedIndexEqual(VeHandle key, VeIndex& index) { assert(false); return false; };
		virtual bool		getMappedIndexEqual(VeHandlePair key, VeIndex& index) { assert(false); return false; };
		virtual bool		getMappedIndexEqual(VeHandleTriple key, VeIndex& index) { assert(false); return false; };
		virtual bool		getMappedIndexEqual(std::string key, VeIndex& index) { assert(false); return false; };

		virtual uint32_t	getMappedIndicesEqual(VeHandle key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual uint32_t	getMappedIndicesEqual(VeHandlePair key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual uint32_t	getMappedIndicesEqual(VeHandleTriple key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual uint32_t	getMappedIndicesEqual(std::string key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };

		virtual uint32_t	getMappedIndicesRange(VeHandle lower, VeHandle upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual uint32_t	getMappedIndicesRange(VeHandlePair lower, VeHandlePair upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual uint32_t	getMappedIndicesRange(VeHandleTriple lower, VeHandleTriple upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual uint32_t	getMappedIndicesRange(std::string lower, std::string upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };

		virtual uint32_t	getAllIndices(std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual bool		insertIntoMap(void* entry, VeIndex dir_index) { assert(false); return false; };
		virtual uint32_t	deleteFromMap(void* entry, VeIndex dir_index) { assert(false); return 0; };


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


	/**
	*
	* \brief
	*
	*	M is either std::map or std::unordered_map
	*	I is the offset/length type, is either VeIndex or VeIndexPair
	*	K is the map key, is either VeHandle, VeHandlePair, VeHandleTriple, or std::string
	*
	*/
	template <typename M, typename K, typename I>
	class VeTypedMap : public VeMap {
	protected:
		I	m_offset;		///
		I	m_num_bytes;	///
		M	m_map;			///key value pairs - value is the index of the entry in the table

		VeTypedMap<M, K, I>* clone() {
			VeTypedMap<M, K, I>* map = new VeTypedMap<M, K, I>(*this);
			return map;
		};

	public:

		VeTypedMap(I offset, I num_bytes) : VeMap(), m_map(custom_alloc<std::pair<K, VeIndex>>(&m_heap)), m_offset(offset), m_num_bytes(num_bytes) {};
		VeTypedMap(const VeTypedMap<M, K, I>& map) : VeMap(), m_map(custom_alloc<std::pair<K, VeIndex>>(&m_heap)), m_offset(map.m_offset), m_num_bytes(map.m_num_bytes) {};
		virtual	~VeTypedMap() {};

		virtual void operator=(const VeMap& basemap) {
			VeTypedMap<M, K, I>* map = &((VeTypedMap<M, K, I>&)basemap);
			m_offset = map->m_offset;
			m_num_bytes = map->m_num_bytes;
			m_map = map->m_map;
		};

		virtual void clear() {
			m_map.clear();
		}

		virtual bool getMappedIndexEqual(K key, VeIndex& index) override {
			if constexpr (std::is_same_v< M, std::multimap<K, VeIndex > > || std::is_same_v< M, std::unordered_multimap<K, VeIndex > >) {
				assert(false);
				return false;
			}
			else {
				auto search = m_map.find(key);
				if (search == m_map.end())
					return false;
				index = search->second;
			}
			return true;
		}

		virtual uint32_t getMappedIndicesEqual(K key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			uint32_t num = 0;

			auto range = m_map.equal_range(key);
			for (auto it = range.first; it != range.second; ++it, ++num)
				result.emplace_back(it->second);
			return num;
		};

		virtual uint32_t getMappedIndicesRange(K lower, K upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			uint32_t num = 0;

			if constexpr (std::is_same_v< M, std::unordered_map<K, VeIndex> > || std::is_same_v< M, std::unordered_multimap<K, VeIndex > > ||
				std::is_same_v < M, std::unordered_map < K, VeIndex, std::hash<K>, std::equal_to<K>, custom_alloc < std::pair<const K, VeIndex> > > > ||
				std::is_same_v < M, std::unordered_multimap < K, VeIndex, std::hash<K>, std::equal_to<K>, custom_alloc < std::pair<const K, VeIndex> > > >) {
				assert(false);
				return false;
			}
			else {
				auto range = m_map.lower_bound(lower);
				for (auto it = range; it != m_map.end(); ++it, ++num)
					result.emplace_back(it->second);
			}
			return num;
		};

		virtual uint32_t getAllIndices(std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			for (auto entry : m_map)
				result.emplace_back(entry.second);
			return (uint32_t)m_map.size();
		}

		uint32_t leftJoin(K key, VeTypedMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) {
			if (m_map.size() == 0 || other.m_map.size() == 0) return 0;
			std::vector<VeIndex, custom_alloc<VeIndex>> res_list1(&m_heap);
			std::vector<VeIndex, custom_alloc<VeIndex>> res_list2(&m_heap);
			getMappedIndicesEqual(key, res_list1);
			other.getMappedIndicesEqual(key, res_list2);
			for (auto i1 : res_list1) {
				for (auto i2 : res_list2) {
					result.push_back(VeIndexPair(i1, i2));
				}
			}
			return (uint32_t)(res_list1.size() * res_list2.size());
		}

		uint32_t leftJoin(VeTypedMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) {
			if (m_map.size() == 0 || other.m_map.size() == 0) return 0;
			uint32_t num = 0;
			K key = m_map.begin()->first;
			num += leftJoin(key, other, result);
			for (auto entry : m_map) {
				if (key != entry.first) {
					key = entry.first;
					num += leftJoin(key, other, result);
				}
			}
			return num;
		}

		virtual bool insertIntoMap(void* entry, VeIndex dir_index) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);

			if constexpr (std::is_same_v< M, std::map<K, VeIndex > > || std::is_same_v< M, std::unordered_map<K, VeIndex > >) {
				auto [it, success] = m_map.try_emplace(key, dir_index);
				assert(success);
				return success;
			}
			else {
				auto it = m_map.emplace(key, dir_index);
			}
			return true;
		};

		virtual uint32_t deleteFromMap(void* entry, VeIndex dir_index) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);

			uint32_t num = 0;
			auto range = m_map.equal_range(key);
			for (auto it = range.first; it != range.second; ) {
				if (it->second == dir_index) {
					it = m_map.erase(it);
					++num;
				}
				else ++it;
			}
			return num;
		};

	};


	using VeMapVeHandle = std::map<      VeHandle, VeIndex, std::less<VeHandle>, custom_alloc<VeHandleIndexPair> >;
	using VeMultimapVeHandle = std::multimap< VeHandle, VeIndex, std::less<VeHandle>, custom_alloc<VeHandleIndexPair> >;
	using VeMapVeHandlePair = std::map<      VeHandlePair, VeIndex, std::less<VeHandlePair>, custom_alloc<std::pair< const VeHandlePair, VeIndex >> >;
	using VeMultimapVeHandlePair = std::multimap< VeHandlePair, VeIndex, std::less<VeHandlePair>, custom_alloc<std::pair< const VeHandlePair, VeIndex >> >;
	using VeMapVeHandleTriple = std::map<      VeHandleTriple, VeIndex, std::less<VeHandleTriple>, custom_alloc< std::pair<const VeHandleTriple, VeIndex>> >;
	using VeMultimapVeHandleTriple = std::multimap< VeHandleTriple, VeIndex, std::less<VeHandleTriple>, custom_alloc< std::pair<const VeHandleTriple, VeIndex>> >;

	using VeUMapVeHandle = std::unordered_map<      VeHandle, VeIndex, std::hash<VeHandle>, std::equal_to<VeHandle>, custom_alloc<VeHandleIndexPair> >;
	using VeUMultimapVeHandle = std::unordered_multimap< VeHandle, VeIndex, std::hash<VeHandle>, std::equal_to<VeHandle>, custom_alloc<VeHandleIndexPair> >;
	using VeUMapVeHandlePair = std::unordered_map<      VeHandlePair, VeIndex, std::hash<VeHandlePair>, std::equal_to<VeHandlePair>, custom_alloc<std::pair<const VeHandlePair, VeIndex>> >;
	using VeUMultimapVeHandlePair = std::unordered_multimap< VeHandlePair, VeIndex, std::hash<VeHandlePair>, std::equal_to<VeHandlePair>, custom_alloc<std::pair<const VeHandlePair, VeIndex>> >;
	using VeUMapVeHandleTriple = std::unordered_map<      VeHandleTriple, VeIndex, std::hash<VeHandleTriple>, std::equal_to<VeHandleTriple>, custom_alloc<std::pair<const VeHandleTriple, VeIndex>> >;
	using VeUMultimapVeHandleTriple = std::unordered_multimap< VeHandleTriple, VeIndex, std::hash<VeHandleTriple>, std::equal_to<VeHandleTriple>, custom_alloc<std::pair<const VeHandleTriple, VeIndex>> >;

	using VeMapString = std::map<     std::string, VeIndex, std::less<std::string>, custom_alloc<VeStringIndexPair> >;
	using VeMultimapString = std::multimap<std::string, VeIndex, std::less<std::string>, custom_alloc<VeStringIndexPair> >;
	using VeUMapString = std::unordered_map<     std::string, VeIndex, std::hash<std::string>, std::equal_to<std::string>, custom_alloc<VeStringIndexPair> >;
	using VeUMultimapString = std::unordered_multimap<std::string, VeIndex, std::hash<std::string>, std::equal_to<std::string>, custom_alloc<VeStringIndexPair> >;




	//----------------------------------------------------------------------------------

	template <typename K, typename I>
	class VeOrderedMultimap : public VeMap {

		struct VeMapEntry {
			K			m_key;
			VeIndex		m_value = VE_NULL_INDEX;
			VeIndex		m_next = VE_NULL_INDEX;		///<index of next sibling with the same key
			VeIndex		m_parent = VE_NULL_INDEX;	///<index of parent
			VeIndex		m_left = VE_NULL_INDEX;		///<index of first child with lower key
			VeIndex		m_right = VE_NULL_INDEX;	///<index of first child with larger key

			VeMapEntry() : m_key() {
				m_value = VE_NULL_INDEX;
				m_next = VE_NULL_INDEX;		///<index of next sibling with the same key
				m_parent = VE_NULL_INDEX;	///<index of parent
				m_left = VE_NULL_INDEX;		///<index of first child with lower key
				m_right = VE_NULL_INDEX;	///<index of first child with larger key
			};

			VeMapEntry( K key, VeIndex value ) : m_key(key), m_value(value) {
				m_next = VE_NULL_INDEX;		///<index of next sibling with the same key
				m_parent = VE_NULL_INDEX;	///<index of parent
				m_left = VE_NULL_INDEX;		///<index of first child with lower key
				m_right = VE_NULL_INDEX;	///<index of first child with larger key
			};

			void print(uint32_t i) {
				std::cout << "IDX " << i;
				std::cout << " KEY " << m_key << " VAL " << m_value << std::endl;
				std::cout << " NEXT " << m_next << " PAR " << m_parent << " LEFT " << m_left << " RIGHT " << m_right << std::endl << std::endl;
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


		void insert( VeIndex root, VeIndex index ) {
			if (root == VE_NULL_INDEX) {
				m_root = index;
				return;
			}

			if (m_map[index].m_key == m_map[root].m_key) {
				m_map[index].m_next = m_map[root].m_next;
				m_map[root].m_next = index;
				return;
			}

			if (m_map[index].m_key < m_map[root].m_key) {
				if (m_map[root].m_left != VE_NULL_INDEX) {
					return insert(m_map[root].m_left, index);
				}
				m_map[root].m_left = index;
				m_map[index].m_parent = root;
				return;
			}

			if (m_map[root].m_right != VE_NULL_INDEX) {
				return insert(m_map[root].m_right, index);
			}
			m_map[root].m_right = index;
			m_map[index].m_parent = root;
		};


		void copyTree( VeIndex index, std::vector<VeMapEntry, custom_alloc<VeMapEntry> >&result) {
			if (index == VE_NULL_INDEX)
				return;
			copyTree(m_map[index].m_left, result);
			VeIndex next = index;
			while (next != VE_NULL_INDEX) {
				result.push_back(m_map[next]);
				next = m_map[next].m_next;
			};
			copyTree(m_map[index].m_right, result);
		}


		void insertRange(int first, int last, std::vector<VeMapEntry, custom_alloc<VeMapEntry> > &map) {
			if (first > last)
				return;

			VeIndex middle = (first + last) / 2;

			m_map.emplace_back({ map[middle].m_key, map[middle].m_value });
			insert(m_root, (VeIndex)m_map.size()-1);

			insertRange( first, middle - 1, map);
			insertRange( middle + 1, last, map);
		};


		VeIndex find(K& key, VeIndex root, VeIndex& last) {
			if (root == VE_NULL_INDEX)
				return VE_NULL_INDEX;

			last = root;

			if (m_map[root].m_key == key) {
				return root;
			}
			if (key < m_map[root].m_key) {
				return find(key, m_map[root].m_left, last);
			}
			return find(key, m_map[root].m_right, last);
		};


		VeIndex find(K& key, VeIndex root) {
			VeIndex last = VE_NULL_INDEX;
			return find(key, m_root, last);
		};


		VeIndex find(K& key) {
			return find(key, m_root);
		};


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

				if (parent == VE_NULL_INDEX && left == VE_NULL_INDEX && right == VE_NULL_INDEX) { // a sibling
					VeIndex kind = find(m_map[index].m_key);
					while (kind != VE_NULL_INDEX) {
						if (m_map[kind].m_next == last) {
							m_map[kind].m_next = index;
							break;
						}
						kind = m_map[kind].m_next;
					}
				}
				else {
					replaceChild(parent, last, index);
					replaceChild(index, left, left);
					replaceChild(index, right, right);
				}
			}
			m_map.pop_back();
		};
	

		//first: number deleted
		//second: index of first survivor
		std::pair<VeIndex,VeIndex> deleteIndexValuePair(VeIndex index, VeIndex value) {
			if (index == VE_NULL_INDEX)
				return {0, VE_NULL_INDEX };

			assert(m_map.size() > index);

			VeIndex first = index;
			VeIndex last = VE_NULL_INDEX;

			while (index != VE_NULL_INDEX) {
				VeIndex next = m_map[index].m_next;
				
				if (m_map[index].m_value == value) { //key, value pair found

					//first, and no next - keep it
					if (first == index && next == VE_NULL_INDEX)
						return {0, first};

					//first, and there are next , or not first - delete it
					if (first == index && next != VE_NULL_INDEX)
						first = next;

					if (last != VE_NULL_INDEX)
						m_map[last].m_next = next;

					deleteIndex(index);
					return { 1, first };
				} else {
					last = index;
				}

				index = next;
			}
			return { 0, first };
		};


		VeIndex findMin(VeIndex root) {
			assert(root != VE_NULL_INDEX);

			VeIndex min_idx = root;
			VeIndex left = m_map[root].m_left;
			VeIndex right = m_map[root].m_right;

			if (left != VE_NULL_INDEX) {
				VeIndex min_left = findMin(left);
				if (m_map[min_left].m_key < m_map[min_idx].m_key)
					min_idx = left;
			}
			if (right != VE_NULL_INDEX) {
				VeIndex min_right = findMin(left);
				if (m_map[min_right].m_key < m_map[min_idx].m_key)
					min_idx = right;
			}
			return min_idx;
		};


		VeIndex copyMin(VeIndex index) {
			VeIndex min_idx = findMin(m_map[index].m_right);
			m_map[index].m_key = m_map[min_idx].m_key;
			m_map[index].m_value = m_map[min_idx].m_value;
			m_map[index].m_next = m_map[min_idx].m_next;
			m_map[min_idx].m_next = VE_NULL_INDEX;
			return min_idx;
		};


		VeIndex deleteKeyValuePair( K & key, VeIndex value, VeIndex root ) {
			VeIndex index = find(key, root);

			if (index != VE_NULL_INDEX) {
				VeIndex parent = m_map[index].m_parent;
				VeIndex left = m_map[index].m_left;
				VeIndex right = m_map[index].m_right;

				auto [num, first] = deleteIndexValuePair(index, value);

				m_map[first].m_parent = parent;
				m_map[first].m_left = left;
				m_map[first].m_right = right;

				if ( m_map[first].m_value != value )	//one left with another value - leave it
					return num;

				//if m_value == value then this is the last entry with this key - remove from tree
				if (left != VE_NULL_INDEX && right != VE_NULL_INDEX) {
					VeIndex min_idx = copyMin(index);
					deleteKeyValuePair(m_map[min_idx].m_key, value, right);
					return num + 1;
				} 

				if (left == VE_NULL_INDEX && right == VE_NULL_INDEX)
					replaceChild(m_map[index].m_parent, index, VE_NULL_INDEX);

				if (left != VE_NULL_INDEX)
					replaceChild(m_map[index].m_parent, index, left);

				if (right != VE_NULL_INDEX)
					replaceChild(m_map[index].m_parent, index, right);

				deleteIndex(index);
				return num + 1;
			}
			return 0;
		};


		VeIndex deleteKeyValuePair(K& key, VeIndex value) {
			return deleteKeyValuePair(key, value, m_root);
		};


		VeIndex getAllIndices(VeIndex root, std::vector<VeIndex, custom_alloc<VeIndex>>& result) {
			if (root == VE_NULL_INDEX)
				return 0;

			VeIndex num = getAllIndices(m_map[root].m_left, result);
			result.push_back(m_map[root].m_value);
			num += getAllIndices(m_map[root].m_right, result);
			return num + 1;
		};


		VeIndex getMappedIndicesRange(VeIndex root, K& lower, K& upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) {
			if (root == VE_NULL_INDEX)
				return 0;

			if (m_map[root].m_key < lower || m_map[root].m_key > upper)
				return 0;

			VeIndex num = 0;

			if (lower < m_map[root].m_key)
				num += getMappedIndicesRange(m_map[root].m_left, lower, upper, result);

			if (lower <= m_map[root].m_key && m_map[root].m_key <= upper) {
				result.push_back(m_map[root].m_value);
				++num;
			}

			if (upper > m_map[root].m_key)
				num += getMappedIndicesRange(m_map[root].m_right, lower, upper, result);

			return num;
		};


		void printTree(VeIndex root, VeIndex level) {
			if (root == VE_NULL_INDEX)
				return;

			printTree(m_map[root].m_left, level + 1);

			std::cout << std::setw(level) << " ";
			VeIndex next = root;
			while (next != VE_NULL_INDEX) {
				std::cout << "KEY " << m_map[next].m_key << " VAL " << m_map[next].m_value << " ";
				next = m_map[next].m_next;
			}
			std::cout << std::endl;
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

		virtual uint32_t getMappedIndicesEqual(K key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			VeIndex index = find(key);
			VeIndex num = 0;
			while (index != VE_NULL_INDEX) {
				result.push_back(m_map[index].m_value);
				index = m_map[index].m_next;
				++num;
			};
			return num;
		};

		virtual uint32_t getMappedIndicesRange(K lower, K upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			return getMappedIndicesRange( m_root, lower, upper, result );
		};

		virtual uint32_t getAllIndices(std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			return getAllIndices( m_root, result );
		}

		uint32_t leftJoin(K key, VeOrderedMultimap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) {
			return 0;
		};

		uint32_t leftJoin(VeOrderedMultimap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) {
			uint32_t num = 0;
			return num;
		};

		virtual bool insertIntoMap(void* entry, VeIndex dir_index) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);
			VeIndex last = m_map.size();
			m_map.emplace_back({ key, dir_index });
			insert(m_root, last);
			return true;
		};

		virtual uint32_t deleteFromMap(void* entry, VeIndex dir_index) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);
			return deleteKeyValuePair( key, dir_index );
		};

	};


	namespace map {
		void testMap();
	}

}


