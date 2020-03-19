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
		virtual bool		insertIntoMap(void* entry, VeIndex& dir_index) { assert(false); return false; };
		virtual uint32_t	deleteFromMap(void* entry, VeIndex& dir_index) { assert(false); return 0; };


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

		virtual bool insertIntoMap(void* entry, VeIndex& dir_index) override {
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

		virtual uint32_t deleteFromMap(void* entry, VeIndex& dir_index) override {
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
	class VeSortedMap : public VeMap {

		struct VeMapEntry {
			K			m_key;
			VeIndex		m_value = VE_NULL_INDEX;
			VeIndex		m_parent = VE_NULL_INDEX;	///<index of parent
			VeIndex		m_next = VE_NULL_INDEX;		///<index of next sibling with the same key
			VeIndex		m_left = VE_NULL_INDEX;		///<index of first child with lower key
			VeIndex		m_right = VE_NULL_INDEX;	///<index of first child with larger key

			VeMapEntry() : m_key() {
				m_value = VE_NULL_INDEX;
				m_parent = VE_NULL_INDEX;	///<index of parent
				m_next = VE_NULL_INDEX;		///<index of next sibling with the same key
				m_left = VE_NULL_INDEX;		///<index of first child with lower key
				m_right = VE_NULL_INDEX;	///<index of first child with larger key
			};

			VeMapEntry( K key, VeIndex value ) : m_key(key), m_value(value) {
				m_parent = VE_NULL_INDEX;	///<index of parent
				m_next = VE_NULL_INDEX;		///<index of next sibling with the same key
				m_left = VE_NULL_INDEX;		///<index of first child with lower key
				m_right = VE_NULL_INDEX;	///<index of first child with larger key
			};
		};

		VeSortedMap<K, I>* clone() {
			VeSortedMap<K, I>* map = new VeSortedMap<K, I>(*this);
			return map;
		};

	protected:
		I						m_offset;			///
		I						m_num_bytes;		///
		VeVector<VeMapEntry>	m_map;				///
		VeIndex					m_root = 0;


		void insert( VeIndex root, VeIndex index ) {
			assert(root != VE_NULL_INDEX);

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

		VeIndex findLast(K& key, VeIndex root, VeIndex& last) {
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


		void deleteIndex(VeIndex index) {
			if (index == VE_NULL_INDEX)
				return;

			assert(m_map.size() > index);

			VeIndex last = m_map.size() - 1;
			if (index < last) {
				m_map.swap(index, last);
				VeIndex parent = m_map[index].m_parent;

				if (m_root == last) {
					m_root = index;
				}

				if (parent != VE_NULL_INDEX) {
					if (m_map[parent].m_left == last) {
						m_map[parent].m_left = index;
					} else {
						m_map[parent].m_right = index;
					}
				}

				if (m_map[index].m_left != VE_NULL_INDEX)
					m_map[m_map[index].m_left].m_parent = index;
				if (m_map[index].m_right != VE_NULL_INDEX)
					m_map[m_map[index].m_right].m_parent = index;
			}

			VeIndex next = m_map[last].m_next;
			m_map.pop_back();
			deleteIndex(next);
		};


		void deleteKey( K & key ) {
			VeIndex index = find(key, m_root);

			if (index != VE_NULL_INDEX) {


				deleteIndex(index);
			}
		};


	public:

		VeSortedMap(I offset, I num_bytes, bool memcopy = false) : VeMap(), m_map(memcopy), m_offset(offset), m_num_bytes(num_bytes) {};
		VeSortedMap(const VeSortedMap<K, I>& map) : VeMap(), m_map(map.m_map), m_offset(map.m_offset), m_num_bytes(map.m_num_bytes) {};
		virtual	~VeSortedMap() {};

		virtual void operator=(const VeMap& basemap) {
			VeSortedMap<K, I>* map = &((VeSortedMap<K, I>&)basemap);
			m_offset = map->m_offset;
			m_num_bytes = map->m_num_bytes;
			m_map = map->m_map;
		};

		virtual void clear() {
			m_root = VE_NULL_INDEX;
			m_map.clear();
		};

		virtual bool getMappedIndexEqual(K key, VeIndex& index) override {

			return true;
		};

		virtual uint32_t getMappedIndicesEqual(K key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			return 0;
		};

		virtual uint32_t getMappedIndicesRange(K lower, K upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			uint32_t num = 0;

			return num;
		};

		virtual uint32_t getAllIndices(std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			return 0;
		}

		uint32_t leftJoin(K key, VeSortedMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) {
			return 0;
		};

		uint32_t leftJoin(VeSortedMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) {
			uint32_t num = 0;
			return num;
		};

		virtual bool insertIntoMap(void* entry, VeIndex& dir_index) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);

			VeIndex index = m_map.size();
			m_map.emplace_back({key, dir_index});

			if (m_root == VE_NULL_INDEX) {
				m_root = index;
				return true;
			}

			insert(m_root, index);

			return true;
		};

		virtual uint32_t deleteFromMap(void* entry, VeIndex& dir_index) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);

			uint32_t num = 0;
			

			return num;
		};


	};



}


