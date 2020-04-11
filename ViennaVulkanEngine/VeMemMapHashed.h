#pragma once


namespace vve {

	//----------------------------------------------------------------------------------

	template <typename K, typename I>
	class VeHashedMultimap : public VeMap {
	protected:

		struct VeMapEntry {
			K		m_key;
			VeValue	m_value = VE_NULL_VALUE;
			VeIndex	m_next = VE_NULL_INDEX;		///<next node with same hash value or free

			VeMapEntry() : m_key() {
				m_value = VE_NULL_VALUE;
				m_next = VE_NULL_INDEX;
			};

			VeMapEntry(K key, VeValue value) : m_key(key), m_value(value) {
				m_next = VE_NULL_INDEX;
			};

			void operator=(const VeMapEntry& entry) {
				m_key = entry.m_key;
				m_value = entry.m_value;
				m_next = entry.m_next;
			};

			bool operator==(const VeMapEntry& entry) {
				if (m_value != VE_NULL_HANDLE && entry.m_value != VE_NULL_HANDLE)
					return m_key == entry.m_key && m_value == entry.m_value;

				return m_key == entry.m_key;
			};

			void print(uint32_t node, uint32_t level = 1) {
				std::cout << "L " << level << " IDX " << node;
				std::cout << " KEY " << m_key << " VAL " << m_value + 0 << " NEXT " << m_next << std::endl;
			};
		};

		I						m_offset;			///
		I						m_num_bytes;		///
		VeCount					m_num_entries = 0;
		VeVector<VeMapEntry>	m_entries;			///
		VeIndex					m_first_free_entry = VE_NULL_INDEX;
		VeVector<VeIndex>		m_map;


		bool insertIntoMap(VeIndex map_idx, VeIndex entry_idx) {
			if (m_map[map_idx] == VE_NULL_INDEX) {
				m_map[map_idx] = entry_idx;
				return true;
			}
			m_entries[entry_idx].m_next = m_map[map_idx];
			m_map[map_idx] = entry_idx;
			return true;
		};


		bool resizeMap() {
			std::vector<VeIndex, custom_alloc<VeIndex>> result(&m_heap);
			result.reserve((uint64_t)m_num_entries);

			std::for_each(m_map.begin(), m_map.end(), [&](VeIndex index) {
				while (index != VE_NULL_INDEX) {
					auto next = m_entries[index].m_next;
					m_entries[index].m_next = VE_NULL_INDEX;
					result.emplace_back(index);
					index = next;
				};
				});

			m_map.resize(2 * m_map.size());
			for (uint32_t i = 0; i < m_map.size(); ++i)
				m_map[i] = VE_NULL_INDEX;

			for (auto entry_idx : result) {
				K key;
				getKey(&m_entries[entry_idx], m_offset, m_num_bytes, key);
				VeIndex map_idx = std::hash<K>()(key) % (VeIndex)m_map.size();
				insertIntoMap(map_idx, entry_idx);
			};
			return true;
		};

		float fill() {
			return (uint64_t)m_num_entries / (float)m_map.size();
		};

		VeHashedMultimap<K, I>* clone() {
			VeHashedMultimap<K, I>* map = new VeHashedMultimap<K, I>(*this);
			return map;
		};

	public:

		VeHashedMultimap(I offset, I num_bytes, bool memcopy = false) :
			VeMap(), m_num_entries(VeCount(0)), m_map(memcopy), m_offset(offset), m_num_bytes(num_bytes) {
			VeIndex size = 1 << 10;
			m_entries.reserve(size);
			m_map.reserve(size);
			for (VeIndex i = 0; i < size; ++i)
				m_map.push_back((VeIndex)VE_NULL_INDEX);
		};

		VeHashedMultimap(const VeHashedMultimap<K, I>& map) :
			VeMap(), m_num_entries(map.m_num_entries), m_entries(map.m_entries), m_first_free_entry(map.m_first_free_entry),
			m_map(map.m_map), m_offset(map.m_offset), m_num_bytes(map.m_num_bytes) {
		};

		virtual	~VeHashedMultimap() {};

		virtual void operator=(const VeMap& basemap) {
			VeHashedMultimap<K, I>* map = &((VeHashedMultimap<K, I>&)basemap);
			m_offset = map->m_offset;
			m_num_bytes = map->m_num_bytes;
			m_num_entries = map->m_num_entries;
			m_entries = map->m_entries;
			m_first_free_entry = map->m_first_free_entry;
			m_map = map->m_map;
		};

		void clear() override {
			m_entries.clear();
			m_first_free_entry = VE_NULL_INDEX;
			m_num_entries = VeCount(0);
			for (VeIndex i = 0; i < m_map.size(); ++i)
				m_map[i] = VE_NULL_INDEX;
		};

		VeCount size() override {
			return m_num_entries;
		};

		void print() override {
			std::vector<std::pair<K, VeValue>, custom_alloc<std::pair<K, VeValue>>> result(&m_heap);
			getAllKeyValuePairs(result);
			for (auto p : result) {
				std::cout << "KEY " << p.first << " VAL " << p.second << std::endl;
			}
			std::cout << std::endl;
		};

		virtual VeValue find(K key) override {
			VeIndex idx = std::hash<K>()(key) % (VeIndex)m_map.size();
			for (VeIndex index = m_map[idx]; index != VE_NULL_INDEX; index = m_entries[index].m_next) {
				if (m_entries[index].m_key == key) {
					return m_entries[index].m_value;
				}
			}
			return VE_NULL_VALUE;
		};

		virtual VeCount equal_range(K key, std::vector<VeValue, custom_alloc<VeValue>>& result) override {
			VeCount num = VeCount(0);
			VeIndex idx = std::hash<K>()(key) % (VeIndex)m_map.size();
			for (VeIndex index = m_map[idx]; index != VE_NULL_INDEX; index = m_entries[index].m_next) {
				if (m_entries[index].m_key == key) {
					result.push_back(m_entries[index].m_value);
					++num;
				}
			}
			return num;
		};

		virtual VeCount getAllValues(std::vector<VeValue, custom_alloc<VeValue>>& result) override {
			VeCount num = VeCount(0);
			std::for_each(m_map.begin(), m_map.end(), [&](VeIndex index) {
				while (index != VE_NULL_INDEX) {
					result.emplace_back(m_entries[index].m_value);
					++num;
					index = m_entries[index].m_next;
				};
				});
			return num;
		};

		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<K, VeValue>, custom_alloc<std::pair<K, VeValue>>>& result) override {
			VeCount num = VeCount(0);
			std::for_each(m_map.begin(), m_map.end(), [&](VeIndex index) {
				while (index != VE_NULL_INDEX) {
					result.emplace_back(m_entries[index].m_key, m_entries[index].m_value);
					++num;
					index = m_entries[index].m_next;
				};
				});
			return num;;
		};

		/*virtual VeCount leftJoin(K key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) override {
			std::vector < VeIndex, custom_alloc<VeIndex> > result1(&m_heap);
			equal_range(key, result1);

			std::vector < VeIndex, custom_alloc<VeIndex> > result2(&m_heap);
			other.equal_range(key, result2);

			for (uint32_t i = 0; i < result1.size(); ++i) {
				for (uint32_t j = 0; j < result2.size(); ++j) {
					result.emplace_back( result1[i], result2[j] );
				}
			}
			return (VeCount)(result1.size() * result2.size());
		};

		virtual VeCount leftJoin(VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) override {
			if (size() == VeCount(0) || other.size() == VeCount(0))
				return VeCount(0);

			std::vector<std::pair<K, VeIndex>, custom_alloc<std::pair<K, VeIndex>> > map1(&m_heap);
			map1.reserve((uint64_t)size());
			getAllKeyValuePairs(map1);

			std::set<K, std::less<K>, custom_alloc<K>> keys(&m_heap);
			for (auto kvp : map1) {
				keys.insert(kvp.first);
			}

			VeCount num = VeCount(0);
			for (auto key : keys) {
				num = num + leftJoin(key, other, result);
			}

			return num;
		};*/

		virtual bool insert(void* entry, VeValue value) override {
			if (fill() > 0.9f)
				resizeMap();

			K key;
			getKey(entry, m_offset, m_num_bytes, key);
			VeIndex idx = std::hash<K>()(key) % (VeIndex)m_map.size();

			VeMapEntry nentry(key, value);
			VeIndex new_index = m_first_free_entry;
			if (m_first_free_entry != VE_NULL_INDEX) {
				m_first_free_entry = m_entries[new_index].m_next;
				m_entries[new_index] = nentry;
			}
			else {
				m_entries.emplace_back(nentry);
				new_index = (VeIndex)m_entries.size() - 1;
			}

			++m_num_entries;
			return insertIntoMap(idx, new_index);
		};

		virtual VeCount erase(void* entry, VeValue value) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);
			VeIndex hidx = std::hash<K>()(key) % (VeIndex)m_map.size();
			VeIndex index = m_map[hidx];
			VeIndex last = VE_NULL_INDEX;
			while (index != VE_NULL_INDEX) {
				if (m_entries[index].m_key == key && m_entries[index].m_value == value) {
					if (index == m_map[hidx]) {
						m_map[hidx] = m_entries[index].m_next;
					}
					else {
						m_entries[last].m_next = m_entries[index].m_next;
					}
					m_entries[index].m_next = m_first_free_entry;
					m_first_free_entry = index;
					--m_num_entries;
					return VeCount(1);
				}
				last = index;
				index = m_entries[index].m_next;
			};
			return VeCount(0);
		};
	};

	//----------------------------------------------------------------------------------

	template <typename K, typename I>
	class VeHashedMap : public VeHashedMultimap<K, I> {
	public:
		VeHashedMap(I offset, I num_bytes, bool memcopy = false) : VeHashedMultimap<K, I>(offset, num_bytes, memcopy) {};
		VeHashedMap(const VeHashedMap<K, I>& map) : VeHashedMultimap<K, I>((const VeHashedMultimap<K, I>&)map) {};
		virtual	~VeHashedMap() {};

		virtual bool insert(void* entry, VeValue value) override {
			K key;
			this->getKey(entry, this->m_offset, this->m_num_bytes, key);
			if (this->find(key) == VE_NULL_VALUE)
				return VeHashedMultimap<K, I>::insert(entry, value);
			return false;
		}
	};

	namespace map {
		void testMap();
	};


};


