#pragma once



namespace mem {

	//------------------------------------------------------------------------------------------------------

	class VeMap {
	protected:

	public:
		VeMap() {};
		virtual ~VeMap() {};
		virtual bool		getMappedIndex(		VeHandle& key, VeIndex& index) { assert(false); return false; };
		virtual uint32_t	getMappedIndices(	VeHandle& key, std::vector<VeIndex>& result) { assert(false); return 0; };
		virtual bool		getMappedIndex(		std::pair<VeHandle, VeHandle> &key, VeIndex& index) { assert(false); return false; };
		virtual uint32_t	getMappedndices(	std::pair<VeHandle, VeHandle> &key, std::vector<VeIndex>& result) { assert(false); return 0; };
		virtual bool		getMappedIndex(		std::string& key, VeIndex& index) { assert(false); return false; };
		virtual uint32_t	getMappedIndices(	std::string& key, std::vector<VeIndex>& result) { assert(false); return 0; };
		virtual uint32_t	getAllIndices(		std::vector<VeIndex>& result) { assert(false); return 0; };
		virtual void		insertIntoMap(		void *entry, VeIndex &dir_index ) { assert(false); };
		virtual uint32_t	deleteFromMap(		void* entry, VeIndex& dir_index) { assert(false); return 0; };

		VeHandle getIntFromEntry(void* entry, VeIndex offset, VeIndex num_bytes ) {
			uint8_t* ptr = (uint8_t*)entry + offset;

			if (num_bytes == 4) {
				uint32_t* k1 = (uint32_t*)ptr;
				return (VeHandle)*k1;
			}
			uint64_t* k2 = (uint64_t*)ptr;
			return (VeHandle)*k2;
		};

		void getKey( void* entry, VeIndex offset, VeIndex num_bytes, VeHandle& key) {
			key = getIntFromEntry( entry, offset, num_bytes );
		};

		void getKey(void* entry, std::pair<VeIndex, VeIndex> offset, 
						std::pair<VeIndex, VeIndex> num_bytes, std::pair<VeHandle, VeHandle>& key) {

			key = std::pair<VeHandle, VeHandle>(getIntFromEntry(entry, offset.first,  num_bytes.first), 
												getIntFromEntry(entry, offset.second, num_bytes.second));
		}

		void getKey( void* entry, VeIndex offset, VeIndex num_bytes, std::string &key) {
			uint8_t* ptr = (uint8_t*)entry + offset;
			std::string* pstring = (std::string*)ptr;
			key = *pstring;
		}

	};


	template <typename M, typename K, typename I>
	class VeTypedMap : public VeMap {
	protected:

		I	m_offset;		///
		I	m_num_bytes;	///
		M	m_map;			///key value pairs - value is the index of the entry in the table

	public:
		VeTypedMap(	I offset, I num_bytes ) : VeMap(), m_offset(offset), m_num_bytes(num_bytes) {};
		virtual ~VeTypedMap() {};

		virtual bool getMappedIndex( K & key, VeIndex &index ) override {
			auto search = m_map.find(key);
			if (search == m_map.end()) return false;
			index = search->second;
			return true;
		}

		virtual uint32_t getMappedIndices(K & key, std::vector<VeIndex>& result) override {
			uint32_t num = 0;
			auto range = m_map.equal_range( key );
			for (auto it = range.first; it != range.second; ++it, ++num) result.emplace_back( it->second );
			return num;
		};

		virtual uint32_t getAllIndices( std::vector<VeIndex>& result) override {
			for (auto entry : m_map) result.emplace_back(entry.second); 
			return (uint32_t)m_map.size();
		}

		virtual void insertIntoMap( void* entry, VeIndex& dir_index ) override {
			K key;
			getKey( entry, m_offset, m_num_bytes, key);
			m_map.try_emplace( key, dir_index );
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



	//------------------------------------------------------------------------------------------------------

	class VeDirectory {
	protected:

		struct VeDirectoryEntry {
			VeIndex	m_auto_id		= VE_NULL_INDEX;
			VeIndex	m_table_index	= VE_NULL_INDEX;	///index into the entry table
			VeIndex	m_next_free		= VE_NULL_INDEX;	///index of next free entry in directory

			VeDirectoryEntry( VeIndex auto_id, VeIndex table_index, VeIndex next_free) : 
				m_auto_id(auto_id), m_table_index(table_index), m_next_free(next_free) {}
		};

		VeIndex							m_auto_counter = 0;				///
		std::vector<VeDirectoryEntry>	m_dir_entries;					///1 level of indirection, idx into the entry table
		VeIndex							m_first_free = VE_NULL_INDEX;	///index of first free entry in directory

		VeHandle addNewEntry(VeIndex table_index ) {
			VeIndex auto_id = ++m_auto_counter;
			VeIndex dir_index = (VeIndex)m_dir_entries.size();
			m_dir_entries.emplace_back( auto_id, table_index, VE_NULL_INDEX);
			return getHandle(dir_index);
		}

		VeHandle writeOverOldEntry(VeIndex table_index) {
			VeIndex auto_id				= ++m_auto_counter;
			VeIndex dir_index			= m_first_free;
			VeIndex next_free			= m_dir_entries[dir_index].m_next_free;
			m_dir_entries[dir_index]	= { auto_id, table_index, VE_NULL_INDEX };
			m_first_free				= next_free;
			return getHandle(dir_index);
		}

	public:
		VeDirectory() {};
		~VeDirectory() {};

		VeHandle addEntry( VeIndex table_index ) {
			if (m_first_free == VE_NULL_INDEX) return addNewEntry(table_index);
			return writeOverOldEntry(table_index);
		};

		VeDirectoryEntry& getEntry( VeIndex dir_index) { return m_dir_entries[dir_index];  };

		VeHandle getHandle(VeIndex dir_index) { return (VeHandle)m_dir_entries[dir_index].m_auto_id << 32 & dir_index; };

		void updateTableIndex(VeIndex dir_index, VeIndex table_index) { m_dir_entries[dir_index].m_table_index = table_index; }

		void deleteEntry( VeIndex dir_index ) {
			m_dir_entries[m_first_free].m_next_free = m_first_free;
			m_first_free = dir_index;
		}
	};


	//------------------------------------------------------------------------------------------------------

	class VeTable {
	protected:
		VeIndex	m_thread_id;	///id of thread that accesses to this table are scheduled to
		bool	m_read_only = false;

	public:
		VeTable( VeIndex thread_id = 0 ) : m_thread_id(thread_id) {};
		virtual ~VeTable() {};
		void	setThreadId(VeIndex id) { m_thread_id = id; };
		VeIndex	getThreadId() { return m_thread_id; };
		void	setReadOnly(bool ro) { m_read_only = ro; };
		bool	getReadOnly() { return m_read_only;  };
	};


	//------------------------------------------------------------------------------------------------------

	template <typename T>
	class VeFixedSizeTypedTable : public VeTable {
	protected:
		std::vector<VeMap*>		m_maps;				///vector of maps for quickly finding or sorting entries
		VeDirectory				m_directory;		///
		std::vector<T>			m_data;				///growable entry data table
		std::vector<VeIndex>	m_tbl2dir;

		void swapEntriesByHandle( VeHandle h1, VeHandle h2 ) {
			if (h1 == h2) return;
			VeIndex first  = getIndexFromHandle(h1);
			VeIndex second = getIndexFromHandle(h2);
			std::swap(m_data[first],	m_data[second]);
			std::swap(m_tbl2dir[first], m_tbl2dir[second]);
			m_directory.updateTableIndex(m_tbl2dir[first], first);
			m_directory.updateTableIndex(m_tbl2dir[second], second);
		};

		VeIndex initVecLen() { return (VeIndex)m_data.size() / 3; };

	public:

		VeFixedSizeTypedTable(VeIndex thread_id = VE_NULL_INDEX) : VeTable( thread_id) {};

		VeFixedSizeTypedTable( std::vector<VeMap*> &&maps, VeIndex thread_id = VE_NULL_INDEX) : VeTable( thread_id ) {
			m_maps = std::move(maps);	
		};

		VeFixedSizeTypedTable(std::vector<VeMap*>& maps, VeIndex thread_id = VE_NULL_INDEX) : VeTable(thread_id) {
			m_maps = maps;
		};

		~VeFixedSizeTypedTable() { for (uint32_t i = 0; i < m_maps.size(); ++i ) delete m_maps[i]; };

		void		addMap(VeMap* pmap) { m_maps.emplace_back(pmap); };
		std::vector<T>& getData() { return m_data; };
		void		sortTableByMap( VeIndex num_map );

		VeHandle	addEntry(T& te);
		bool		getEntryFromHandle(VeHandle key, T& entry);
		VeIndex		getIndexFromHandle(VeHandle key);
		VeHandle	getHandleFromIndex(VeIndex table_index);
		bool		deleteEntryByHandle(VeHandle key);

		bool		getEntryFromMap(VeIndex num_map, VeHandle key, T& entry);
		bool		getEntryFromMap(VeIndex num_map, std::pair<VeHandle,VeHandle> &key, T& entry);
		bool		getEntryFromMap(VeIndex num_map, std::string key, T& entry);

		uint32_t	getEntriesFromMap(VeIndex num_map, VeHandle key, std::vector<T>& result);
		uint32_t	getEntriesFromMap(VeIndex num_map, std::pair<VeHandle, VeHandle>& key, std::vector<T>& result);
		uint32_t	getEntriesFromMap(VeIndex num_map, std::string& key, std::vector<T>& result);

		uint32_t	getHandlesFromMap(VeIndex num_map, VeHandle key, std::vector<VeHandle>& result);
		uint32_t	getHandlesFromMap(VeIndex num_map, std::pair<VeHandle, VeHandle>& key, std::vector<VeHandle>& result);
		uint32_t	getHandlesFromMap(VeIndex num_map, std::string& key, std::vector<VeHandle>& result);

		uint32_t	getAllIndicesFromMap(VeIndex num_map, std::vector<VeIndex>& result);
		uint32_t	getAllEntriesFromMap(VeIndex num_map, std::vector<T>& result);
		uint32_t	getAllHandlesFromMap(VeIndex num_map, std::vector<VeHandle>& result);

		uint32_t	deleteEntriesByMap(VeIndex num_map, VeHandle key );
		uint32_t	deleteEntriesByMap(VeIndex num_map, std::pair<VeHandle, VeHandle>& key);
		uint32_t	deleteEntriesByMap(VeIndex num_map, std::string& key);
	};

	//--------------------------------------------------------------------------------------------------------------------------

	template<typename T> inline void VeFixedSizeTypedTable<T>::sortTableByMap(VeIndex num_map) {
		assert(!m_read_only);
		std::vector<VeHandle> handles( m_data.size() );
		getAllHandlesFromMap(num_map, handles);
		for (uint32_t i = 0; i < m_data.size(); ++i) swapEntriesByHandle( getHandleFromIndex(i), handles[i]);
	}

	template<typename T> inline VeHandle VeFixedSizeTypedTable<T>::addEntry(T& te) {
		assert(!m_read_only);
		VeIndex table_index = (VeIndex)m_data.size();
		m_data.emplace_back(te);

		VeHandle handle		= m_directory.addEntry( table_index );
		VeIndex dir_index	= handle & VE_NULL_INDEX;
		m_tbl2dir.emplace_back(dir_index);
		for (auto map : m_maps) map->insertIntoMap( (void*)&te, dir_index );
		return handle;
	};

	template<typename T> inline bool VeFixedSizeTypedTable<T>::getEntryFromHandle( VeHandle key, T& entry ) {
		VeIndex dir_index	= (VeIndex)(key & VE_NULL_INDEX);
		VeIndex auto_id		= (VeIndex)(key >> 32);
		auto dir_entry		= m_directory.getEntry(dir_index);

		if ( auto_id != dir_entry.m_auto_id ) return false;
		entry = m_data[dir_entry.m_table_index];
		return true;
	};

	template<typename T> inline VeIndex VeFixedSizeTypedTable<T>::getIndexFromHandle(VeHandle key) {
		VeIndex dir_index	= (VeIndex)(key & VE_NULL_INDEX);
		VeIndex auto_id		= (VeIndex)(key >> 32);
		auto dir_entry		= m_directory.getEntry(dir_index);

		if (auto_id != dir_entry.m_auto_id) return VE_NULL_INDEX;
		return m_data[dir_entry.m_table_index];
	};

	template<typename T> inline VeHandle VeFixedSizeTypedTable<T>::getHandleFromIndex(VeIndex table_index) {
		VeIndex dir_index	= m_tbl2dir[table_index];
		VeIndex auto_id		= m_directory.getEntry(dir_index).m_auto_id;
		return m_directory.getHandle(dir_index);
	};

	template<typename T> inline bool VeFixedSizeTypedTable<T>::deleteEntryByHandle(VeHandle key) {
		assert(!m_read_only);
		VeIndex dir_index	= (VeIndex)(key & VE_NULL_INDEX);
		VeIndex auto_id		= (VeIndex)(key >> 32);
		auto dir_entry		= m_directory.getEntry(dir_index);
		if (auto_id != dir_entry.m_auto_id) return false;

		VeIndex table_index = dir_entry.m_table_index;
		swapEntriesByIndex( table_index, m_data.size() - 1);

		for (auto map : m_maps) map->deleteFromMap((void*)&m_data[table_index], dir_index);
		m_directory.deleteEntry(dir_entry);		
		m_data.pop_back();
		m_tbl2dir.pop_back();
		return true;
	};

	//--------------------------------------------------------------------------------------------------------------------------

	template<typename T> inline bool VeFixedSizeTypedTable<T>::getEntryFromMap(VeIndex num_map, VeHandle key, T& entry) {
		VeIndex dir_index;
		if (!m_maps[num_map]->getMappedIndex(key, dir_index)) return false;
		entry = m_data[m_directory.getEntry(dir_index).m_table_index];
		return true;
	};

	template<typename T> inline bool VeFixedSizeTypedTable<T>::getEntryFromMap(VeIndex num_map, std::pair<VeHandle, VeHandle>& key, T& entry) {
		VeIndex dir_index;
		if (!m_maps[num_map]->getMappedIndex(key, dir_index)) return false;
		entry = m_data[m_directory.getEntry(dir_index).m_table_index];
		return true;
	};

	template<typename T> inline	bool VeFixedSizeTypedTable<T>::getEntryFromMap(VeIndex num_map, std::string key, T& entry) {
		VeIndex dir_index;
		if (!m_maps[num_map]->getMappedIndex(key, dir_index)) return false;
		entry = m_data[m_directory.getEntry(dir_index).m_table_index];
		return true;
	};

	//--------------------------------------------------------------------------------------------------------------------------

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getEntriesFromMap(VeIndex num_map, VeHandle key, std::vector<T>& result) {
		std::vector<VeIndex> dir_indices(initVecLen());
		uint32_t num = m_maps[num_map]->getMappedIndices(key, dir_indices);
		for (auto dir_index : dir_indices) result.emplace_back( m_data[m_directory.getEntry(dir_index).m_table_index] );
		return num;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getEntriesFromMap(	VeIndex num_map,
																						std::pair<VeHandle, VeHandle>& key, 
																						std::vector<T>& result) {
		std::vector<VeIndex> dir_indices(initVecLen());
		uint32_t num = m_maps[num_map]->getMappedIndices(key, dir_indices);
		for (auto dir_index : dir_indices) result.emplace_back(m_data[m_directory.getEntry(dir_index).m_table_index]);
		return num;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getEntriesFromMap(VeIndex num_map, std::string& key, std::vector<T>& result) {
		std::vector<VeIndex> dir_indices(initVecLen());
		uint32_t num = m_maps[num_map]->getMappedIndices(key, dir_indices);
		for (auto dir_index : dir_indices) result.emplace_back(m_data[m_directory.getEntry(dir_index).m_table_index]);
		return num;
	};

	//--------------------------------------------------------------------------------------------------------------------------

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getHandlesFromMap(	VeIndex num_map, 
																						VeHandle key, 
																						std::vector<VeHandle>& result) {
		std::vector<VeIndex> dir_indices(initVecLen());
		uint32_t num = m_maps[num_map]->getMappedIndices(key, dir_indices);
		for (auto dir_index : dir_indices) result.emplace_back( m_directory.getHandle(dir_index) );
		return num;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getHandlesFromMap(	VeIndex num_map,
																						std::pair<VeHandle, VeHandle>& key,
																						std::vector<VeHandle>& result) {
		std::vector<VeIndex> dir_indices(initVecLen());
		uint32_t num = m_maps[num_map]->getMappedIndices(key, dir_indices);
		for (auto dir_index : dir_indices) result.emplace_back(m_directory.getHandle(dir_index));
		return num;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getHandlesFromMap(	VeIndex num_map, 
																						std::string& key, 
																						std::vector<VeHandle>& result) {
		std::vector<VeIndex> dir_indices(initVecLen());
		uint32_t num = m_maps[num_map]->getMappedIndices(key, dir_indices);
		for (auto dir_index : dir_indices) result.emplace_back(m_directory.getHandle(dir_index));
		return num;
	};


	//--------------------------------------------------------------------------------------------------------------------------

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getAllIndicesFromMap( VeIndex num_map, std::vector<VeIndex>& result) {
		std::vector<VeIndex> dir_indices(m_data.size());
		uint32_t num = m_maps[num_map]->getAllIndices(dir_indices);
		for (auto dir_index : dir_indices) result.emplace_back( m_directory.getEntry(dir_index).m_table_index );
		return num;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getAllEntriesFromMap(VeIndex num_map, std::vector<T>& result) {
		std::vector<VeIndex> dir_indices(m_data.size());
		uint32_t num = m_maps[num_map]->getAllIndices(dir_indices);
		for (auto dir_index : dir_indices) result.emplace_back( m_data[m_directory.getEntry(dir_index).m_table_index] );
		return num;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getAllHandlesFromMap(VeIndex num_map, std::vector<VeHandle>& result) {
		std::vector<VeIndex> dir_indices(m_data.size());
		uint32_t num = m_maps[num_map]->getAllIndices(dir_indices);
		for (auto dir_index : dir_indices) result.emplace_back(m_directory.getHandle(dir_index));
		return num;
	};

	//--------------------------------------------------------------------------------------------------------------------------

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::deleteEntriesByMap(VeIndex num_map, VeHandle key) {
		assert(!m_read_only);
		std::vector<VeHandle> handles(initVecLen());
		uint32_t res = 0;
		if (getHandlesFromMap( num_map, key, handles ) > 0) { 
			for (auto handle : handles) if (deleteEntryByHandle(handle)) ++res;
		}
		return res;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::deleteEntriesByMap(VeIndex num_map, std::pair<VeHandle, VeHandle>& key) {
		assert(!m_read_only);
		std::vector<VeHandle> handles(initVecLen());
		uint32_t res = 0;
		if (getHandlesFromMap( num_map, key, handles ) > 0) {
			for (auto handle : handles) if (deleteEntryByHandle(handle)) ++res;
		}
		return res;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::deleteEntriesByMap(VeIndex num_map, std::string& key) {
		assert(!m_read_only);
		std::vector<VeHandle> handles(initVecLen());
		uint32_t res = 0;
		if (getHandlesFromMap( num_map, key, handles ) > 0) {
			for (auto handle : handles) if (deleteEntryByHandle(handle)) ++res;
		}
		return res;
	};



	///-------------------------------------------------------------------------------

	class VariableSizeTable : public VeTable {
	protected:

		struct VeDirectoryEntry {
			VeIndex m_auto_id;
			VeIndex m_start;
			VeIndex m_size;

			VeDirectoryEntry(VeIndex start, VeIndex size) : m_start(start), m_size(size) {};
		};
 
		VeIndex								m_auto_counter = 0;
		std::map<VeIndex, VeDirectoryEntry> m_occupied;
		std::map<VeIndex, VeDirectoryEntry> m_free;
		std::vector<uint8_t>				m_data;

	public:
		VariableSizeTable(VeIndex size, VeIndex thread_id = 0 ) : VeTable( thread_id ) {
			m_data.resize(size);
			VeDirectoryEntry entry{ 0, size };
			m_free.try_emplace(0, entry);
		}

		~VariableSizeTable() {};

		bool insertBlob( uint8_t* ptr, VeIndex size ) {
			for (auto it = m_free.begin(); it != m_free.end(); ) {
				if (it->second.m_size >= size) {
					VeDirectoryEntry new_entry{ it->second.m_start, size };
					m_occupied.try_emplace(it->second.m_start, new_entry);
					memcpy(&m_data[it->second.m_start], ptr, size);

					it->second.m_size -= size;
					if (it->second.m_size == 0) m_free.erase(it);
				}
				else ++it;
			}
			return false;
		}

		bool deleteBlob(VeIndex start) {
			auto search = m_occupied.find(start);
			if (search == m_occupied.end()) return false;
			VeDirectoryEntry new_entry{ search->second.m_start, search->second.m_size };
			m_free.try_emplace(search->second.m_start, new_entry);
			m_occupied.erase(search);
			return true;
		}

	};


}


