#pragma once

/**
*
* \file 
* \brief 
*
* Details
*
*/


namespace vve {



	//------------------------------------------------------------------------------------------------------

	/**
	*
	* \brief
	*
	*
	*/
	class VeDirectory {
	protected:

		struct VeDirectoryEntry {
			VeIndex	m_guid = VE_NULL_INDEX;
			VeIndex	m_table_index = VE_NULL_INDEX;	///< index into the entry table
			VeIndex	m_next_free = VE_NULL_INDEX;	///< index of next free entry in directory

			VeDirectoryEntry() : m_guid(VE_NULL_INDEX), m_table_index(VE_NULL_INDEX), m_next_free(VE_NULL_INDEX) {};

			VeDirectoryEntry(VeIndex guid, VeIndex table_index, VeIndex next_free) :
				m_guid(guid), m_table_index(table_index), m_next_free(next_free) {};

			VeDirectoryEntry(const VeDirectoryEntry& entry) :
				m_guid(entry.m_guid), m_table_index(entry.m_table_index), m_next_free(entry.m_next_free) {};

			VeDirectoryEntry& operator=(const VeDirectoryEntry& entry) {
				m_guid = entry.m_guid;
				m_table_index = entry.m_table_index;
				m_next_free = entry.m_next_free;
				return *this;
			};
		};

		VeCount							m_auto_counter = VeCount(0);	///< 
		std::vector<VeDirectoryEntry>	m_dir_entries;					///< 1 level of indirection, idx into the data table
		VeIndex							m_first_free = VE_NULL_INDEX;	///< index of first free entry in directory

		VeHandle addNewEntry(VeIndex table_index ) {
			VeIndex guid = (VeIndex)m_auto_counter; 
			++m_auto_counter;
			VeIndex dir_index = (VeIndex)m_dir_entries.size();
			m_dir_entries.emplace_back( guid, table_index, VE_NULL_INDEX  );
			return getHandle(dir_index);
		}

		VeHandle writeOverOldEntry(VeIndex table_index) {
			VeCount guid				= m_auto_counter;
			++m_auto_counter;
			VeIndex dir_index			= m_first_free;
			VeIndex next_free			= m_dir_entries[dir_index].m_next_free;
			m_dir_entries[dir_index]	= { (VeIndex)guid, table_index, VE_NULL_INDEX };
			m_first_free				= next_free;
			return getHandle(dir_index);
		}

	public:
		VeDirectory() {};
		~VeDirectory() {};

		void operator=( const VeDirectory& dir ) {
			m_auto_counter	= dir.m_auto_counter;
			m_dir_entries	= dir.m_dir_entries;
			m_first_free	= dir.m_first_free;
		}

		void clear() {
			m_dir_entries.clear();
			m_first_free = VE_NULL_INDEX;
		}

		VeHandle addEntry( VeIndex table_index ) {
			if (m_first_free == VE_NULL_INDEX) 
				return addNewEntry(table_index);
			return writeOverOldEntry(table_index);
		};

		VeDirectoryEntry& getEntry( VeIndex dir_index) { 
			assert(isValid(dir_index));
			return m_dir_entries[dir_index];  
		};

		VeHandle getHandle(VeIndex dir_index) { 
			if (!isValid(dir_index)) 
				return VE_NULL_HANDLE;
			return VeHandle( m_dir_entries[dir_index].m_guid | ((uint64_t)dir_index << 32) );  
		};

		static::std::tuple<VeIndex,VeIndex> splitHandle(VeHandle key ) { 
			return { VeIndex(key) & 0xFFFFFFFF , (VeIndex)((uint64_t)key >> 32) };
		};

		bool isValid(VeIndex dir_index) {
			if (dir_index >= m_dir_entries.size() || m_dir_entries[dir_index].m_next_free != VE_NULL_INDEX )
				return false;
			return true;
		}

		bool isValid(VeHandle handle) {
			if (handle == VE_NULL_HANDLE) return false;
			auto [guid, dir_index] = splitHandle(handle);
			if (!isValid(dir_index) || guid != m_dir_entries[dir_index].m_guid) 
				return false;
			return true;
		}

		void updateTableIndex(VeIndex dir_index, VeIndex table_index) { 
			assert(isValid(dir_index));
			m_dir_entries[dir_index].m_table_index = table_index; 
		}

		void deleteEntry( VeIndex dir_index ) {
			assert(isValid(dir_index));
			if (m_first_free != VE_NULL_INDEX) 
				m_dir_entries[dir_index].m_next_free = m_first_free;
			m_first_free = dir_index;
		}
	};


	//------------------------------------------------------------------------------------------------------

	/**
	*
	* \brief
	*
	*
	*/
	class VeTable {
	protected:
		VeHeapMemory			m_heap;
		vgjs::VgjsThreadIndex	m_thread_idx = 0;			///id of thread that accesses to this table are scheduled to
		bool					m_read_only = false;
		VeTable	*				m_companion_table = nullptr;
		bool					m_clear_on_swap = false;
		std::atomic<bool>		m_swapping = false;
		bool					m_dirty = false;

		//for debugging
		uint32_t	m_table_nr = 0;
		std::string m_name;
		VeClock		m_clock;

		inline void in() {};
		inline void out() {};

	public:
		VeTable(std::string name, bool clear_on_swap = false) : 
			m_name(name), m_heap(), m_clear_on_swap(clear_on_swap), m_clock("table clock", 100) {};

		VeTable(VeTable& table) : m_heap(), m_thread_idx(table.m_thread_idx), 
			m_read_only(!table.m_read_only), m_companion_table(&table), 
			m_clear_on_swap(table.m_clear_on_swap), 
			m_name(table.m_name), m_clock(table.m_clock) {
			table.m_companion_table = this;
			m_table_nr = table.m_table_nr + 1;
		};

		virtual ~VeTable() {};
		virtual void operator=(VeTable& tab) { assert(false);  return; };
		virtual void clear() { assert(false);  return; };
		virtual VeMap* getMap(VeIndex num_map) { assert(false);  return nullptr; };
		virtual VeDirectory* getDirectory() { assert(false);  return nullptr; };

		void setThreadIdx(vgjs::VgjsThreadIndex id) {
			if (m_thread_idx == id)
				return;

			m_thread_idx = id;
			VeTable* companion_table = getCompanionTable();
			if (companion_table != nullptr)
				companion_table->setThreadIdx(id);
		};
		vgjs::VgjsThreadIndex	getThreadIdx() { return m_thread_idx; };

		void setName( std::string name, bool set_companion = true ) {
			m_name = name;
			if (set_companion && getCompanionTable() != nullptr) {
				getCompanionTable()->setName( name, false );
			}
		};
		std::string getName() {
			return m_name;
		}

		virtual void setReadOnly(bool ro) { 
			m_read_only = ro; 
		};
		bool	getReadOnly() { return m_read_only;  };

		VeTable* getReadTablePtr() { 
			if (m_companion_table == nullptr)
				return this;

			VeTable* table = this;
			if (!m_read_only) {
				table = m_companion_table;
			}
			assert(table->getReadOnly());
			return table;
		};

		VeTable* getWriteTablePtr() { 
			if (m_companion_table == nullptr)
				return this;

			VeTable* table = this;
			if (m_read_only) {
				table = m_companion_table;
			}
			assert(!table->getReadOnly());
			return table;

		};

		virtual void swapTables() {
			in();

			//std::cout << "table " << m_name << " old read " << getReadTablePtr()->m_table_nr << " old write " << getWriteTablePtr()->m_table_nr << std::endl;

			if (m_companion_table == nullptr) {
				out();
				return;
			}

			m_swapping = true;
			m_companion_table->m_swapping = true;

			setReadOnly(!getReadOnly());
			m_companion_table->setReadOnly(!m_companion_table->getReadOnly());

			//std::cout << "table " << m_name << " new read " << getReadTablePtr()->m_table_nr << " new write " << getWriteTablePtr()->m_table_nr << std::endl;

			if (m_clear_on_swap) {
				getWriteTablePtr()->clear();
			}
			else if(getReadTablePtr()->m_dirty ) {
				*getWriteTablePtr() = *getReadTablePtr();
			}
			m_dirty = false;

			m_swapping = false;
			m_companion_table->m_swapping = false;

			out();
		};

		VeTable* getCompanionTable() {
			return m_companion_table;
		}
	};


	//------------------------------------------------------------------------------------------------------

	/**
	*
	* \brief
	*
	*
	*/

	#define VECTOR VeVector<T>
	#define VECTORPAR memcopy,align,capacity

	template <typename T>
	class VeFixedSizeTable : public VeTable {
	protected:
		std::vector<VeMap*>		m_maps;				///vector of maps for quickly finding or sorting entries
		VeDirectory				m_directory;		///
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
		VeDirectory* getDirectory() override { return &m_directory; };
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

	template<typename T> inline void VeFixedSizeTable<T>::operator=( VeFixedSizeTable<T>& table) {
		in();
		assert(!m_read_only);
		m_directory = table.m_directory;
		m_data		= table.m_data;
		for (uint32_t i = 0; i < table.m_maps.size(); ++i ) 
			*(m_maps[i]) = *(table.m_maps[i]);
		m_idx2dir	= table.m_idx2dir;
		out();
	}

	template<typename T> inline void VeFixedSizeTable<T>::operator=(VeTable& table) {
		in();
		assert(!m_read_only);
		VeFixedSizeTable<T>* other = (VeFixedSizeTable<T>*)&table;
		m_directory = other->m_directory;
		m_data = other->m_data;
		for (uint32_t i = 0; i < other->m_maps.size(); ++i)
			*(m_maps[i]) = *(other->m_maps[i]);
		m_idx2dir = other->m_idx2dir;
		out();
	}

	//--------------------------------------------------------------------------------------------------------------------------

	template<typename T> inline void VeFixedSizeTable<T>::sort(VeIndex num_map) {
		in();
		assert(!m_read_only && num_map < m_maps.size());
		m_dirty = true;
		std::vector<VeHandle, custom_alloc<VeHandle>> handles(&m_heap);
		getAllHandlesFromMap(num_map, handles);
		for (uint32_t i = 0; i < m_data.size(); ++i) 
			swap( getHandleFromIndex(i), handles[i]);
		out();
	}

	template<typename T> inline VeHandle VeFixedSizeTable<T>::insert(T entry, std::promise<VeHandle>* pPromise) {
		in();
		assert(!m_read_only);
		m_dirty = true;
		VeIndex table_index = (VeIndex)m_data.size();
		m_data.emplace_back(entry);

		VeHandle handle		= m_directory.addEntry( table_index );
		auto [guid, dir_index] = VeDirectory::splitHandle( handle );
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
		assert(!m_read_only);
		m_dirty = true;
		if (!isValid(handle) || m_data.empty()) {
			out();
			return false;
		}

		auto [guid, dir_index]	= m_directory.splitHandle(handle);
		auto dir_entry				= m_directory.getEntry(dir_index);
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
		m_dirty = true;
		assert(!m_read_only);
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
		m_dirty = true;
		assert(!m_read_only);
		if (key == VE_NULL_HANDLE || m_data.empty()) {
			out();
			return false;
		}

		auto [guid, dir_index]	= m_directory.splitHandle(key);
		auto dir_entry				= m_directory.getEntry(dir_index);
		if (guid != dir_entry.m_guid) {
			out();
			return false;
		}

		VeIndex table_index = dir_entry.m_table_index;
		swap( key, getHandleFromIndex((VeIndex)m_data.size() - 1) );

		for (auto map : m_maps) 
			map->erase((void*)&m_data[(VeIndex)m_data.size() - 1], VeValue(dir_index));
		m_directory.deleteEntry(dir_index);
		m_data.pop_back();
		m_idx2dir.pop_back();
		out();
		return true;
	};


	//----------------------------------------------------------------------------------------

	template<typename T> inline bool VeFixedSizeTable<T>::isValid(VeHandle handle) {
		return m_directory.isValid(handle);
	}

	template<typename T> inline VeIndex VeFixedSizeTable<T>::getIndexFromHandle(VeHandle key) {
		in();
		if (key == VE_NULL_HANDLE) {
			out();
			return VE_NULL_INDEX;
		}

		auto [guid, dir_index]	= m_directory.splitHandle(key);
		auto dir_entry				= m_directory.getEntry(dir_index);

		VeIndex result = dir_entry.m_table_index;
		if (guid != dir_entry.m_guid) 
			result = VE_NULL_INDEX;
		out();
		return result;
	};

	template<typename T> inline bool VeFixedSizeTable<T>::getEntry(VeHandle key, T& entry) {
		in();
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
		assert(table_index < m_idx2dir.size());
		VeIndex dir_index	= m_idx2dir[table_index];
		VeIndex guid		= m_directory.getEntry(dir_index).m_guid;
		VeHandle handle		= m_directory.getHandle(dir_index);
		out();
		return handle;
	};

	template<typename T> inline void VeFixedSizeTable<T>::clear() {
		in();
		for (auto map : m_maps) 
			map->clear();

		m_data.clear();
		m_directory.clear();
		m_idx2dir.clear();
		m_heap.clear();
		out();
	}


	//--------------------------------------------------------------------------------------------------------------------------

	template<typename T>
	VeCount VeFixedSizeTable<T>::leftJoin(VeIndex own_map, VeTable* other, VeIndex other_map, std::vector<VeHandlePair, custom_alloc<VeHandlePair>>& result) {
		in();
		VeMap* l = (VeMap*)m_maps[own_map];
		VeMap* r = (VeMap*)other->getMap(other_map);

		VeCount num = 0;
		std::vector<VeIndexPair, custom_alloc<VeIndexPair>> dir_indices(&m_heap);
		l->leftJoin(*r, dir_indices);
		for (auto [first, second] : dir_indices) {
			result.emplace_back(m_directory.getHandle(first), other->getDirectory()->getHandle(second) );
			++num;
		}
		out();
		return num;
	}


	template<typename T>
	template <typename K>
	VeCount VeFixedSizeTable<T>::leftJoin(VeIndex own_map, K key, VeTable* other, VeIndex other_map, std::vector<VeHandlePair, custom_alloc<VeHandlePair>>& result) {
		in();
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
	inline VeHandle VeFixedSizeTable<T>::find(K key, VeIndex num_map ) {
		in();
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
		assert(num_map < m_maps.size());
		if (m_data.empty()) {
			out();
			return VeCount(0);
		}
		VeCount num = VeCount(0);
		std::vector<VeValue, custom_alloc<VeValue>> dir_indices(&m_heap);

		num = m_maps[num_map]->equal_range( key, dir_indices);
		for (auto dir_index : dir_indices)
			result.emplace_back(m_directory.getHandle(VeIndex(dir_index)));
		out();
		return num;
	};

	template <typename T>
	template <typename K> 
	inline VeCount VeFixedSizeTable<T>::getHandlesRange(K lower, K upper, VeIndex num_map, std::vector<VeHandle, custom_alloc<VeHandle>>& result) {
		in();
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
		for (VeIndex i = 0; i < m_data.size(); ++i)
			result.emplace_back(getHandleFromIndex(i));
		out();
		return (VeCount)m_data.size();
	}

	template<typename T> inline	VeCount VeFixedSizeTable<T>::getAllHandlesFromMap(VeIndex num_map, std::vector<VeHandle, custom_alloc<VeHandle>>& result) {
		if (m_data.empty())
			return VeCount(0);

		if (num_map == VE_NULL_INDEX)
			return getAllHandles( result );

		in();
		assert(num_map < m_maps.size());
		std::vector<VeValue, custom_alloc<VeValue>> dir_indices(&m_heap);
		VeCount num = m_maps[num_map]->getAllValues(dir_indices);
		for (auto dir_index : dir_indices) 
			result.emplace_back( m_directory.getHandle(VeIndex(dir_index)) );
		out();
		return num;
	};



	//------------------------------------------------------------------------------------------------------

	/**
	*
	* \brief
	*
	*
	*/
	template <typename T>
	class VeFixedSizeTableMT : public VeFixedSizeTable<T> {

	public:

		VeFixedSizeTableMT<T>(std::string name, bool memcopy = false, bool clear_on_swap = false, VeIndex align = 16, VeIndex capacity = 16) :
			VeFixedSizeTable<T>(name, memcopy, clear_on_swap, align, capacity) {};

		VeFixedSizeTableMT<T>(std::string name, std::vector<VeMap*>& maps, bool memcopy = false, bool clear_on_swap = false, VeIndex align = 16, VeIndex capacity = 16) :
			VeFixedSizeTable<T>(name, maps, memcopy, clear_on_swap, align, capacity) {};

		VeFixedSizeTableMT<T>(VeFixedSizeTable<T>& table) : VeFixedSizeTable<T>(table) {};

		virtual ~VeFixedSizeTableMT<T>() {};

		//----------------------------------------------------------------------------

		virtual void operator=(VeFixedSizeTableMT<T>& table) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getWriteTablePtr();
			VeFixedSizeTable<T>* other = (VeFixedSizeTable<T>*)&table;
			if (this->m_swapping) {
				me->VeFixedSizeTable<T>::operator=(*other);
				return;
			}
			if( vgjs::JobSystem::isInstanceCreated() && this->m_thread_idx != JIDX)
				JADDT( me->VeFixedSizeTable<T>::operator=(*other), vgjs::TID(this->m_thread_idx));
			else 
				me->VeFixedSizeTable<T>::operator=(*other);
		};

		virtual void swap(VeHandle h1, VeHandle h2) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getWriteTablePtr();
			if (vgjs::JobSystem::isInstanceCreated() && this->m_thread_idx != JIDX)
				JADDT(me->VeFixedSizeTable<T>::swap(h1, h2), vgjs::TID(this->m_thread_idx));
			else
				me->VeFixedSizeTable<T>::swap(h1, h2);
		};

		virtual void clear() {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getWriteTablePtr();
			if (this->m_swapping) {
				me->VeFixedSizeTable<T>::clear();
				return;
			}
			if (!this->m_swapping && vgjs::JobSystem::isInstanceCreated() && this->m_thread_idx != JIDX)
				JADDT(me->VeFixedSizeTable<T>::clear(), vgjs::TID(this->m_thread_idx));
			else
				me->VeFixedSizeTable<T>::clear();
		};

		virtual void sort(VeIndex num_map) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getWriteTablePtr();
			if (vgjs::JobSystem::isInstanceCreated() && this->m_thread_idx != JIDX)
				JADDT(me->VeFixedSizeTable<T>::sort(num_map), vgjs::TID(this->m_thread_idx));
			else
				me->VeFixedSizeTable<T>::sort(num_map);
		};

		virtual VeHandle insert(T entry, std::promise<VeHandle>* pPromise) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getWriteTablePtr();
			if (vgjs::JobSystem::isInstanceCreated() && this->m_thread_idx != JIDX ) {
				JADDT(me->VeFixedSizeTable<T>::insert(entry, pPromise), vgjs::TID(this->m_thread_idx));
				return VE_NULL_HANDLE;
			}
			return me->VeFixedSizeTable<T>::insert(entry, pPromise);
		};

		virtual VeHandle insert(T entry) {
			return insert(entry, nullptr);
		};

		virtual bool update(VeHandle key, T entry) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getWriteTablePtr();
			if (vgjs::JobSystem::isInstanceCreated() && this->m_thread_idx != JIDX) {
				JADDT(me->VeFixedSizeTable<T>::update(key, entry), vgjs::TID(this->m_thread_idx));
				return true;
			}
			return me->VeFixedSizeTable<T>::update(key, entry);
		};

		virtual bool erase(VeHandle key) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getWriteTablePtr();
			if (vgjs::JobSystem::isInstanceCreated() && this->m_thread_idx != JIDX) {
				JADDT(me->VeFixedSizeTable<T>::erase(key), vgjs::TID(this->m_thread_idx));
				return true;
			}
			return me->VeFixedSizeTable<T>::erase(key);
		};


		//----------------------------------------------------------------------------

		// read operations
		bool isValid(VeHandle handle) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getReadTablePtr();
			return me->VeFixedSizeTable<T>::isValid(handle);
		};

		const VECTOR& data() { 
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getReadTablePtr();
			return me->VeFixedSizeTable<T>::data();
		};

		VeMap* getMap(VeIndex num_map) { 
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getReadTablePtr();
			return me->VeFixedSizeTable<T>::getMap(num_map);
		};

		VeDirectory* getDirectory() { 
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getReadTablePtr();
			return me->VeFixedSizeTable<T>::getDirectory();
		};

		VeCount	size() {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getReadTablePtr();
			return me->VeFixedSizeTable<T>::size();
		};

		bool getEntry(VeHandle key, T& entry) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getReadTablePtr();
			return me->VeFixedSizeTable<T>::getEntry(key, entry);
		};

		VeIndex	getIndexFromHandle(VeHandle key) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getReadTablePtr();
			return me->VeFixedSizeTable<T>::getIndexFromHandle(key);
		};

		VeHandle getHandleFromIndex(VeIndex table_index) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getReadTablePtr();
			return me->VeFixedSizeTable<T>::getHandleFromIndex(table_index);
		};

		VeCount getAllHandles(std::vector<VeHandle, custom_alloc<VeHandle>>& result) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getReadTablePtr();
			return me->VeFixedSizeTable<T>::getAllHandles(result);
		};

		VeCount getAllHandlesFromMap(VeIndex num_map, std::vector<VeHandle, custom_alloc<VeHandle>>& result) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getReadTablePtr();
			return me->VeFixedSizeTable<T>::getAllHandlesFromMap(num_map, result);
		};

		template <typename K> 
		VeCount leftJoin(	VeIndex own_map, K key, VeTable* table, VeIndex other_map,
							std::vector<VeHandlePair, custom_alloc<VeHandlePair>>& result) {

			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getReadTablePtr();
			VeTable* other = table->getReadTablePtr();

			return me->VeFixedSizeTable<T>::leftJoin<K>(own_map, key, other, other_map, result);
		};

		VeCount leftJoin(	VeIndex own_map, VeTable* table, VeIndex other_map,
							std::vector<VeHandlePair, custom_alloc<VeHandlePair>>& result) {

			VeFixedSizeTableMT<T>* me  = dynamic_cast<VeFixedSizeTableMT<T>*>(this->getReadTablePtr());
			VeTable* other = (VeTable*)table->getReadTablePtr();

			return me->VeFixedSizeTable<T>::leftJoin(own_map, other, other_map, result);
		};

		template <typename K>
		VeHandle find(K key, VeIndex num_map ) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getReadTablePtr();
			return me->VeFixedSizeTable<T>::find(key, num_map );
		};

		template <typename K>
		VeCount getHandlesEqual(K key, VeIndex num_map, std::vector<VeHandle, custom_alloc<VeHandle>>& result) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getReadTablePtr();
			return me->VeFixedSizeTable<T>::getHandlesEqual(key, num_map, result);
		};

		template <typename K>
		VeCount getHandlesRange(K lower, K upper, VeIndex num_map, std::vector<VeHandle, custom_alloc<VeHandle>>& result) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->getReadTablePtr();
			return me->VeFixedSizeTable<T>::getHandlesRange(lower, upper, num_map, result);
		};

	};




	///-------------------------------------------------------------------------------

	/**
	*
	* \brief
	*
	*
	*/
	class VeVariableSizeTable : public VeTable {
	protected:

		struct VeDirectoryEntry {
			VeIndex m_start;
			VeIndex m_size;
			VeIndex m_occupied;

			VeDirectoryEntry(VeIndex start, VeIndex size, VeIndex occupied) : m_start(start), m_size(size), m_occupied(occupied) {};
			VeDirectoryEntry() : m_start(VE_NULL_INDEX), m_size(VE_NULL_INDEX), m_occupied(VE_NULL_INDEX) {};
		};

		VeFixedSizeTable<VeDirectoryEntry>	m_directory;
		std::vector<uint8_t>				m_data;
		VeIndex								m_align;
		bool								m_immediateDefrag;

		void defragment() {
			std::vector<VeHandle, custom_alloc<VeHandle>> handles(&m_heap);
			handles.reserve( (VeIndex)((uint64_t)m_directory.size() + 1) );
			m_directory.getAllHandlesFromMap(1, handles);
			if (handles.size() < 2) 
				return;

			VeDirectoryEntry entry1;
			if (!m_directory.getEntry(handles[0], entry1)) 
				return;

			for (uint32_t i = 1; i < handles.size(); ++i ) {
				VeDirectoryEntry entry2;
				if (!m_directory.getEntry(handles[i], entry2)) 
					return;

				if (entry1.m_occupied == 0 && entry2.m_occupied == 0) {
					entry1.m_size += entry2.m_size;
					m_directory.erase(handles[i]);
				}
				else 
					entry1 = entry2;
			}
		}

	public:
		VeVariableSizeTable(std::string name, VeIndex size = 1<<20, bool clear_on_swap = false, VeIndex align = 16, bool immediateDefrag = false ) :
			VeTable(name, clear_on_swap), m_directory(name), m_align(align), m_immediateDefrag(immediateDefrag) {

			m_directory.addMap(new VeOrderedMultimap< VeKey, VeIndex >(
				(VeIndex)offsetof(struct VeDirectoryEntry, m_occupied), (VeIndex)sizeof(VeDirectoryEntry::m_occupied)));

			m_directory.addMap(new VeOrderedMultimap< VeKey, VeIndex >(
				(VeIndex)offsetof(struct VeDirectoryEntry, m_start), (VeIndex)sizeof(VeDirectoryEntry::m_start)));
					   
			m_data.resize((VeIndex)size + (VeIndex)m_align);
			uint64_t start = (VeIndex) ( alignBoundary((uint64_t)m_data.data(), m_align) - (uint64_t)m_data.data() );
			VeDirectoryEntry entry{ (VeIndex)start, size, 0 };
			m_directory.insert(entry, nullptr);
		}

		VeVariableSizeTable( VeVariableSizeTable& table) :
			VeTable(table), m_directory(table.m_directory), m_data(table.m_data), m_align(table.m_align), 
			m_immediateDefrag(table.m_immediateDefrag) {
		};

		virtual ~VeVariableSizeTable() {};

		void operator=(VeTable& table) {
			VeVariableSizeTable* other = (VeVariableSizeTable*)&table;
			m_directory = other->m_directory;
			m_data = other->m_data;
		}

		void operator=(VeVariableSizeTable & table) {
			m_directory = table.m_directory;
			m_data = table.m_data;
		}

		virtual void setReadOnly(bool ro) { 
			m_read_only = ro; 
			m_directory.setReadOnly(ro);
		};

		virtual void swapTables() {
			if (m_companion_table == nullptr)
				return;

			VeVariableSizeTable::setReadOnly(!getReadOnly());
			((VeVariableSizeTable*)m_companion_table)->VeVariableSizeTable::setReadOnly(!m_companion_table->getReadOnly());

			if (m_clear_on_swap) {
				((VeVariableSizeTable*)getWriteTablePtr())->clear();
			}
			else {
				auto pWrite = (VeVariableSizeTable*)getWriteTablePtr();
				auto pRead = (VeVariableSizeTable*)getReadTablePtr();

				if (pRead->m_dirty) {
					if (pWrite->m_data.size() != pRead->m_data.size()) {
						pWrite->m_data.resize(pRead->m_data.size());
					}
					memcpy(pWrite->m_data.data(), pRead->m_data.data(), pRead->m_data.size());
					*(m_directory.getWriteTablePtr()) = *(m_directory.getReadTablePtr());
				}
				m_dirty = false;
			}
		};

		virtual void clear() {
			in();
			m_dirty = true;
			m_directory.clear();
			uint64_t start = (VeIndex)(alignBoundary((uint64_t)m_data.data(), m_align) - (uint64_t)m_data.data());
			VeDirectoryEntry entry{ (VeIndex)start, (VeIndex)(m_data.size() - m_align), 0 };
			m_directory.insert(entry, nullptr);
			out();
		}

		VeHandle insertBlob( uint8_t* ptr, VeIndex size, VeHandle *pHandle = nullptr, bool defrag = true ) {
			in();
			size = (VeIndex)alignBoundary( size, m_align );

			std::vector<VeHandle, custom_alloc<VeHandle>> result(&m_heap);
			m_directory.getHandlesEqual(0_Ke, 0, result ); //map 0, all where occupied == false

			VeHandle h = VE_NULL_HANDLE;
			VeDirectoryEntry entry;
			for (auto handle : result) {
				if (m_directory.getEntry(handle, entry) && entry.m_size >= size) {
					h = handle;
					break;
				}
			}
			if (h == VE_NULL_HANDLE) {
				if (!defrag) {
					out();
					return VE_NULL_HANDLE;
				}
				defragment();
				out();
				return insertBlob(ptr, size, pHandle, false);
			}

			m_dirty = true;
			if (entry.m_size > size) { 
				VeDirectoryEntry newentry{ entry.m_start + size, entry.m_size - size, 0 };
				m_directory.insert(newentry, nullptr);
			}
			entry.m_size = size;
			entry.m_occupied = 1;
			m_directory.update(h, entry);
			out();
			if (pHandle != nullptr) *pHandle = h;
			return h;
		}

		uint8_t* getPointer(VeHandle handle) {
			in();
			VeDirectoryEntry entry;
			if (!m_directory.getEntry(handle, entry)) {
				out();
				return nullptr;
			}
			uint8_t* result = m_data.data() + entry.m_start;
			out();
			return result;
		}

		bool deleteBlob(VeHandle handle ) {
			in();
			VeDirectoryEntry entry;
			if (!m_directory.getEntry(handle, entry)) {
				out();
				return false;
			}
			m_dirty = true;
			entry.m_occupied = 0;
			m_directory.update(handle, entry);
			if (m_immediateDefrag) 
				defragment();
			out();
			return true;
		}

	};


	///-------------------------------------------------------------------------------

	/**
	*
	* \brief
	*
	*
	*/
	class VeVariableSizeTableMT : public VeVariableSizeTable {
	protected:

	public:
		VeVariableSizeTableMT(std::string name, VeIndex size = 1 << 20, bool clear_on_swap = false, VeIndex align = 16, bool immediateDefrag = false) :
			VeVariableSizeTable(name, size, clear_on_swap, align, immediateDefrag) {};

		VeVariableSizeTableMT(VeVariableSizeTableMT& table) : VeVariableSizeTable(table) {};

		virtual ~VeVariableSizeTableMT() {};

		//---------------------------------------------------------------------------------

		void operator=(VeTable& table) {
			VeVariableSizeTable* other_tab = (VeVariableSizeTable*)&table;

			VeVariableSizeTable* me = (VeVariableSizeTable*)this->getWriteTablePtr();
			VeVariableSizeTable* other = (VeVariableSizeTable*)other_tab->getReadTablePtr();

			JADDT(me->VeVariableSizeTable::operator=(*other), vgjs::TID(this->m_thread_idx));
		}

		void operator=(VeVariableSizeTableMT& table) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->getWriteTablePtr();
			VeVariableSizeTable* other = (VeVariableSizeTable*)table.getReadTablePtr();

			JADDT( me->VeVariableSizeTable::operator=(*other), vgjs::TID(this->m_thread_idx));
		}

		virtual void setReadOnly(bool ro) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this;
			JADDT(me->VeVariableSizeTable::setReadOnly(ro), vgjs::TID(this->m_thread_idx));
		};

		virtual void clear() {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->getWriteTablePtr();
			JADDT(me->VeVariableSizeTable::clear(), vgjs::TID(this->m_thread_idx));
		}

		VeHandle insertBlob(uint8_t* ptr, VeIndex size, VeHandle* pHandle = nullptr, bool defrag = true) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->getWriteTablePtr();
			JADDT(me->VeVariableSizeTable::insertBlob(ptr, size, pHandle, defrag), vgjs::TID(this->m_thread_idx));
		}

		bool deleteBlob(VeHandle handle) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->getWriteTablePtr();
			JADDT(me->VeVariableSizeTable::deleteBlob(handle), vgjs::TID(this->m_thread_idx));
		}

		//---------------------------------------------------------------------------------

		uint8_t* getPointer(VeHandle handle) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->getReadTablePtr();
			return me->VeVariableSizeTable::getPointer(handle);
		};

	};


	namespace tab {
		void testTables();
	};


};


