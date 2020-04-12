#pragma once



namespace vve {


	//------------------------------------------------------------------------------------------------------


#define VECTOR VeVector<T>
#define VECTORPAR memcopy,align,capacity


	/**
	*
	* \brief
	*
	*
	*/
	template <typename T>
	class VeFixedSizeTable : public VeTable {
	protected:
		std::vector<VeMap*>		m_maps;				///vector of maps for quickly finding or sorting entries
		VeSlotMap				m_directory;		///
		VECTOR					m_data;				///growable entry data table
		std::vector<VeIndex>	m_idx2dir;

	public:

		VeFixedSizeTable(std::string name, bool memcopy = false, bool clear_on_swap = false, VeIndex align = 16, VeIndex capacity = 16) :
			VeTable(name, clear_on_swap), m_data(VECTORPAR) {
		};

		VeFixedSizeTable(std::string name, std::vector<VeMap*>& maps, bool memcopy = false, bool clear_on_swap = false, VeIndex align = 16, VeIndex capacity = 16) :
			VeTable(name, clear_on_swap), m_maps(maps), m_data(VECTORPAR) {
		};

		VeFixedSizeTable(VeFixedSizeTable<T>& table) :
			VeTable(table), m_data(table.m_data), m_directory(table.m_directory), m_idx2dir(table.m_idx2dir) {
			for (auto map : table.m_maps) {
				m_maps.emplace_back(map->clone());
			}
		};

		virtual ~VeFixedSizeTable() {
			for (uint32_t i = 0; i < m_maps.size(); ++i)
				delete m_maps[i];
		};

		//neutral operations
		void addMap(VeMap* pmap) { in(); m_maps.emplace_back(pmap); out(); };

		//write operations - must be run in a job when multithreaded
		void operator=(VeFixedSizeTable<T>& table);
		virtual void operator=(VeTable& table);
		void		swap(VeHandle h1, VeHandle h2);
		virtual void clear();
		void		sort(VeIndex num_map);
		VeHandle	insert(T entry, std::promise<VeHandle>* pPromise);
		VeHandle	insert(T entry) { return insert(entry, nullptr); };
		bool		update(VeHandle key, T entry);
		bool		erase(VeHandle key);

		// read operations
		bool		isValid(VeHandle handle);
		const VECTOR& data() { return m_data; };
		VeMap* getMap(VeIndex num_map) override { return m_maps[num_map]; };
		VeSlotMap* getDirectory() override { return &m_directory; };
		std::vector<VeIndex>& getTable2dir() { return m_idx2dir; };
		VeCount		size() { return (VeCount)m_data.size(); };
		bool		getEntry(VeHandle key, T& entry);
		VeIndex		getIndexFromHandle(VeHandle key);
		VeHandle	getHandleFromIndex(VeIndex table_index);
		VeCount		getAllHandles(std::vector<VeHandle, custom_alloc<VeHandle>>& result);
		VeCount		getAllHandlesFromMap(VeIndex num_map, std::vector<VeHandle, custom_alloc<VeHandle>>& result);	//makes sense for map/multimap

		template <typename K> VeHandle find(K key, VeIndex num_map);	//use this in map
		template <typename K> VeCount getHandlesEqual(K key, VeIndex num_map, std::vector<VeHandle, custom_alloc<VeHandle>>& result);	//use this in multimap
		template <typename K> VeCount getHandlesRange(K lower, K upper, VeIndex num_map, std::vector<VeHandle, custom_alloc<VeHandle>>& result); //do not use in unordered map/multimap

		VeCount leftJoin(VeIndex own_map, VeTable* other, VeIndex other_map, std::vector<VeHandlePair, custom_alloc<VeHandlePair>>& result);
		template <typename K>
		VeCount leftJoin(VeIndex own_map, K key, VeTable* other, VeIndex other_map, std::vector<VeHandlePair, custom_alloc<VeHandlePair>>& result);
	};

	//--------------------------------------------------------------------------------------------------------------------------

	template<typename T> inline void VeFixedSizeTable<T>::operator=(VeFixedSizeTable<T>& table) {
		in();
		m_directory = table.m_directory;
		m_data = table.m_data;
		for (uint32_t i = 0; i < table.m_maps.size(); ++i)
			*(m_maps[i]) = *(table.m_maps[i]);
		m_idx2dir = table.m_idx2dir;
		out();
	}

	template<typename T> inline void VeFixedSizeTable<T>::operator=(VeTable& table) {
		in();
		VeFixedSizeTable<T>* other = (VeFixedSizeTable<T>*) & table;
		m_directory = other->m_directory;
		m_data = other->m_data;
		for (uint32_t i = 0; i < other->m_maps.size(); ++i)
			*(m_maps[i]) = *(other->m_maps[i]);
		m_idx2dir = other->m_idx2dir;
		out();
	}

	//--------------------------------------------------------------------------------------------------------------------------
	//write operations

	template<typename T> inline void VeFixedSizeTable<T>::sort(VeIndex num_map) {
		in();
		JSETT(sort(num_map), vgjs::TID(m_thread_idx, JLABEL), return);
		WITHNEXTSTATE(sort(num_map));
		assert(num_map < m_maps.size());

		m_dirty = true;
		std::vector<VeHandle, custom_alloc<VeHandle>> handles(&m_heap);
		getAllHandlesFromMap(num_map, handles);
		for (uint32_t i = 0; i < m_data.size(); ++i)
			swap(getHandleFromIndex(i), handles[i]);
		out();
	}

	template<typename T> inline VeHandle VeFixedSizeTable<T>::insert(T entry, std::promise<VeHandle>* pPromise) {
		in();
		JSETT(insert(entry, pPromise), vgjs::TID(m_thread_idx, JLABEL), return VE_NULL_HANDLE);
		WITHNEXTSTATE(insert(entry, pPromise));

		m_dirty = true;
		VeIndex table_index = (VeIndex)m_data.size();
		m_data.emplace_back(entry);

		VeHandle handle = m_directory.addEntry(table_index);
		auto [guid, dir_index] = VeSlotMap::splitHandle(handle);
		m_idx2dir.emplace_back(dir_index);

		bool success = true;
		for (auto map : m_maps) {
			success = success && map->insert((void*)&entry, VeValue(dir_index));
		}
		if (!success) {
			erase(handle);
			handle = VE_NULL_HANDLE;
		}

		out();
		if (pPromise != nullptr)
			pPromise->set_value(handle);
		return handle;
	};

	template<typename T> inline bool VeFixedSizeTable<T>::update(VeHandle handle, T entry) {
		in();
		JSETT(update(handle, entry), vgjs::TID(m_thread_idx, JLABEL), return false);
		WITHNEXTSTATE(update(handle, entry));

		m_dirty = true;
		if (!isValid(handle) || m_data.empty()) {
			out();
			return false;
		}

		auto [guid, dir_index] = m_directory.splitHandle(handle);
		auto dir_entry = m_directory.getEntry(dir_index);
		if (guid != dir_entry.m_guid) {
			out();
			return false;
		}

		for (auto map : m_maps)
			map->erase((void*)&m_data[dir_entry.m_table_index], VeValue(dir_index));
		m_data[dir_entry.m_table_index] = entry;
		for (auto map : m_maps)
			map->insert((void*)&entry, VeValue(dir_index));
		out();
		return true;
	};

	template<typename T> inline void VeFixedSizeTable<T>::swap(VeHandle h1, VeHandle h2) {
		in();
		JSETT(swap(h1, h2), vgjs::TID(m_thread_idx, JLABEL), return);
		WITHNEXTSTATE(swap(h1, h2));

		m_dirty = true;
		if (h1 == h2 || !isValid(h1) || !isValid(h2)) {
			out();
			return;
		}

		VeIndex first = getIndexFromHandle(h1);
		VeIndex second = getIndexFromHandle(h2);
		if (first == VE_NULL_INDEX || second == VE_NULL_INDEX) {
			out();
			return;
		}
		std::swap(m_data[first], m_data[second]);
		std::swap(m_idx2dir[first], m_idx2dir[second]);
		m_directory.updateTableIndex(m_idx2dir[first], first);
		m_directory.updateTableIndex(m_idx2dir[second], second);
		out();
	};

	template<typename T> inline bool VeFixedSizeTable<T>::erase(VeHandle key) {
		in();
		JSETT(erase(key), vgjs::TID(m_thread_idx, JLABEL), return false);
		WITHNEXTSTATE(erase(key));

		m_dirty = true;
		if (key == VE_NULL_HANDLE || m_data.empty()) {
			out();
			return false;
		}

		auto [guid, dir_index] = m_directory.splitHandle(key);
		auto dir_entry = m_directory.getEntry(dir_index);
		if (guid != dir_entry.m_guid) {
			out();
			return false;
		}

		VeIndex table_index = dir_entry.m_table_index;
		swap(key, getHandleFromIndex((VeIndex)m_data.size() - 1));

		for (auto map : m_maps)
			map->erase((void*)&m_data[(VeIndex)m_data.size() - 1], VeValue(dir_index));
		m_directory.deleteEntry(dir_index);
		m_data.pop_back();
		m_idx2dir.pop_back();
		out();
		return true;
	};

	template<typename T> inline void VeFixedSizeTable<T>::clear() {
		in();
		JSETT(clear(), vgjs::TID(m_thread_idx, JLABEL), return);
		WITHNEXTSTATE(clear());

		for (auto map : m_maps)
			map->clear();

		m_data.clear();
		m_directory.clear();
		m_idx2dir.clear();
		m_heap.clear();
		out();
	}



	//----------------------------------------------------------------------------------------
	//read operations

	template<typename T> inline bool VeFixedSizeTable<T>::isValid(VeHandle handle) {
		WITHCURRENTSTATE(isValid(handle));
		return m_directory.isValid(handle);
	}

	template<typename T> inline VeIndex VeFixedSizeTable<T>::getIndexFromHandle(VeHandle key) {
		in();
		WITHCURRENTSTATE(getIndexFromHandle(key));

		if (key == VE_NULL_HANDLE) {
			out();
			return VE_NULL_INDEX;
		}

		auto [guid, dir_index] = m_directory.splitHandle(key);
		auto dir_entry = m_directory.getEntry(dir_index);

		VeIndex result = dir_entry.m_table_index;
		if (guid != dir_entry.m_guid)
			result = VE_NULL_INDEX;
		out();
		return result;
	};

	template<typename T> inline bool VeFixedSizeTable<T>::getEntry(VeHandle key, T& entry) {
		in();
		WITHCURRENTSTATE(getEntry(key, entry));

		if (key == VE_NULL_HANDLE || m_data.empty()) {
			out();
			return false;
		}

		auto [guid, dir_index] = m_directory.splitHandle(key);
		auto dir_entry = m_directory.getEntry(dir_index);
		if (guid != dir_entry.m_guid)
			return false;

		entry = m_data[dir_entry.m_table_index];
		out();
		return true;
	};

	template<typename T> inline VeHandle VeFixedSizeTable<T>::getHandleFromIndex(VeIndex table_index) {
		in();
		WITHCURRENTSTATE(getHandleFromIndex(table_index));

		assert(table_index < m_idx2dir.size());
		VeIndex dir_index = m_idx2dir[table_index];
		VeIndex guid = m_directory.getEntry(dir_index).m_guid;
		VeHandle handle = m_directory.getHandle(dir_index);
		out();
		return handle;
	};


	template<typename T>
	VeCount VeFixedSizeTable<T>::leftJoin(VeIndex own_map, VeTable* other, VeIndex other_map, std::vector<VeHandlePair, custom_alloc<VeHandlePair>>& result) {
		in();
		WITHCURRENTSTATE(leftJoin(own_map, other, other_map, result));

		VeMap* l = (VeMap*)m_maps[own_map];
		VeMap* r = (VeMap*)other->getMap(other_map);

		VeCount num = 0;
		std::vector<VeIndexPair, custom_alloc<VeIndexPair>> dir_indices(&m_heap);
		l->leftJoin(*r, dir_indices);
		for (auto [first, second] : dir_indices) {
			result.emplace_back(m_directory.getHandle(first), other->getDirectory()->getHandle(second));
			++num;
		}
		out();
		return num;
	}


	template<typename T>
	template <typename K>
	VeCount VeFixedSizeTable<T>::leftJoin(VeIndex own_map, K key, VeTable* other, VeIndex other_map, std::vector<VeHandlePair, custom_alloc<VeHandlePair>>& result) {
		in();
		WITHCURRENTSTATE(leftJoin(own_map, key, other, other_map, result));

		VeMap* l = (VeMap*)m_maps[own_map];
		VeMap* r = (VeMap*)other->getMap(other_map);

		VeCount num = 0;
		std::vector<VeIndexPair, custom_alloc<VeIndexPair>> dir_indices(&m_heap);
		l->leftJoin(key, *r, dir_indices);
		for (auto [first, second] : dir_indices) {
			result.emplace_back(m_directory.getHandle(first), other->getDirectory()->getHandle(second));
			++num;
		}
		out();
		return num;
	}


	template<typename T>
	template<typename K>
	inline VeHandle VeFixedSizeTable<T>::find(K key, VeIndex num_map) {
		in();
		WITHCURRENTSTATE(find(key, num_map));
		assert(num_map < m_maps.size());

		if (m_data.empty()) {
			out();
			return VE_NULL_HANDLE;
		}
		VeValue dir_index = m_maps[num_map]->find(key);
		VeHandle result = m_directory.getHandle(VeIndex(dir_index));
		out();
		return result;
	};

	template<typename T>
	template<typename K>
	inline VeCount VeFixedSizeTable<T>::getHandlesEqual(K key, VeIndex num_map, std::vector<VeHandle, custom_alloc<VeHandle>>& result) {
		in();
		WITHCURRENTSTATE(getHandlesEqual(key, num_map, result));
		assert(num_map < m_maps.size());

		if (m_data.empty()) {
			out();
			return VeCount(0);
		}
		VeCount num = VeCount(0);
		std::vector<VeValue, custom_alloc<VeValue>> dir_indices(&m_heap);

		num = m_maps[num_map]->equal_range(key, dir_indices);
		for (auto dir_index : dir_indices)
			result.emplace_back(m_directory.getHandle(VeIndex(dir_index)));
		out();
		return num;
	};

	template <typename T>
	template <typename K>
	inline VeCount VeFixedSizeTable<T>::getHandlesRange(K lower, K upper, VeIndex num_map, std::vector<VeHandle, custom_alloc<VeHandle>>& result) {
		in();
		WITHCURRENTSTATE(getHandlesRange(lower, upper, num_map, result));
		assert(num_map < m_maps.size());

		if (m_data.empty()) {
			out();
			return VeCount(0);
		}
		VeCount num = VeCount(0);
		std::vector<VeValue, custom_alloc<VeValue>> dir_indices(&m_heap);

		num = m_maps[num_map]->range(lower, upper, dir_indices);
		for (auto dir_index : dir_indices)
			result.emplace_back(m_directory.getHandle(VeIndex(dir_index)));
		out();
		return num;
	}

	template<typename T> inline	VeCount VeFixedSizeTable<T>::getAllHandles(std::vector<VeHandle, custom_alloc<VeHandle>>& result) {
		in();
		WITHCURRENTSTATE(getAllHandles(result));

		for (VeIndex i = 0; i < m_data.size(); ++i)
			result.emplace_back(getHandleFromIndex(i));
		out();
		return (VeCount)m_data.size();
	}

	template<typename T> inline	VeCount VeFixedSizeTable<T>::getAllHandlesFromMap(VeIndex num_map, std::vector<VeHandle, custom_alloc<VeHandle>>& result) {
		WITHCURRENTSTATE(getAllHandlesFromMap(num_map, result));

		if (m_data.empty())
			return VeCount(0);

		if (num_map == VE_NULL_INDEX)
			return getAllHandles(result);

		in();
		assert(num_map < m_maps.size());
		std::vector<VeValue, custom_alloc<VeValue>> dir_indices(&m_heap);
		VeCount num = m_maps[num_map]->getAllValues(dir_indices);
		for (auto dir_index : dir_indices)
			result.emplace_back(m_directory.getHandle(VeIndex(dir_index)));
		out();
		return num;
	};


};


