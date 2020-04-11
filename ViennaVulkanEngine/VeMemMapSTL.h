#pragma once



namespace vve {

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
	class VeSTLMap : public VeMap {
	protected:
		I	m_offset;		///
		I	m_num_bytes;	///
		M	m_map;			///key value pairs - value is the index of the entry in the table

		VeSTLMap<M, K, I>* clone() {
			VeSTLMap<M, K, I>* map = new VeSTLMap<M, K, I>(*this);
			return map;
		};

	public:

		VeSTLMap(I offset, I num_bytes) : 
			VeMap(), m_map(custom_alloc<std::pair<K, VeIndex>>(&m_heap)), m_offset(offset), m_num_bytes(num_bytes) {};

		VeSTLMap(const VeSTLMap<M, K, I>& map) : 
			VeMap(), m_map(custom_alloc<std::pair<K, VeIndex>>(&m_heap)), m_offset(map.m_offset), m_num_bytes(map.m_num_bytes) {};

		virtual	~VeSTLMap() {};

		virtual void operator=(const VeMap& basemap) {
			VeSTLMap<M, K, I>* map = &((VeSTLMap<M, K, I>&)basemap);
			m_offset = map->m_offset;
			m_num_bytes = map->m_num_bytes;
			m_map = map->m_map;
		};

		virtual void clear() {
			m_map.clear();
		}

		VeCount size() override {
			return (VeCount)m_map.size();
		};

		void print() override {
		};


		virtual VeValue find(K key) override {
			VeValue index = VE_NULL_VALUE;
			if constexpr (std::is_same_v< M, std::multimap<K, VeValue > > || std::is_same_v< M, std::unordered_multimap<K, VeValue > >) {
				assert(false);
				return index;
			}
			else {
				auto search = m_map.find(key);
				if (search != m_map.end())
					index = search->second;
			}
			return index;
		}

		virtual VeCount equal_range(K key, std::vector<VeValue, custom_alloc<VeValue>>& result) override {
			VeCount num = 0_Cnt;

			auto range = m_map.equal_range(key);
			for (auto it = range.first; it != range.second; ++it, ++num)
				result.emplace_back(it->second);
			return num;
		};

		virtual VeCount range(K lower, K upper, std::vector<VeValue, custom_alloc<VeValue>>& result) override {
			VeCount num = 0_Cnt;

			if constexpr (std::is_same_v< M, std::unordered_map<K, VeValue> > || std::is_same_v< M, std::unordered_multimap<K, VeValue > > ||
				std::is_same_v < M, std::unordered_map < K, VeValue, std::hash<K>, std::equal_to<K>, custom_alloc < std::pair<const K, VeValue> > > > ||
				std::is_same_v < M, std::unordered_multimap < K, VeValue, std::hash<K>, std::equal_to<K>, custom_alloc < std::pair<const K, VeValue> > > >) {
				assert(false);
				return 0_Cnt;
			}
			else {
				auto range = m_map.lower_bound(lower);
				for (auto it = range; it != m_map.end(); ++it, ++num)
					result.emplace_back(it->second);
			}
			return num;
		};

		virtual VeCount getAllValues(std::vector<VeValue, custom_alloc<VeValue>>& result) override {
			for (auto entry : m_map)
				result.emplace_back(entry.second);
			return (VeCount)m_map.size();
		};

		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<K, VeValue>, custom_alloc<std::pair<K, VeValue>>>& result) override {
			return 0_Cnt;
		};

		/*uint32_t leftJoin(K key, VeTypedMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) {
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
		}*/

		virtual bool insert(void* entry, VeValue value) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);

			if constexpr (std::is_same_v< M, std::map<K, VeIndex > > || std::is_same_v< M, std::unordered_map<K, VeIndex > >) {
				auto [it, success] = m_map.try_emplace(key, value);
				assert(success);
				return success;
			}
			else {
				auto it = m_map.emplace(key, value);
			}
			return true;
		};

		virtual VeCount erase(void* entry, VeValue value) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);

			VeCount num = 0_Cnt;
			auto range = m_map.equal_range(key);
			for (auto it = range.first; it != range.second; ) {
				if (it->second == value) {
					it = m_map.erase(it);
					++num;
				}
				else ++it;
			}
			return num;
		};

	};

};



