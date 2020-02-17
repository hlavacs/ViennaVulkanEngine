#pragma once



namespace mem {

	//------------------------------------------------------------------------------------------------------

	using VeTableKeyInt = VeHandle;
	using VeTableKeyIntPair = std::pair<VeTableKeyInt, VeTableKeyInt>;
	using VeTableKeyString = std::string;
	using VeTableIndex = VeIndex;
	using VeTableIndexPair = std::pair<VeTableIndex, VeTableIndex>;


	void testTables();


	/**
	*
	* \brief 
	*
	* \param[in] eye New position of the entity
	* \param[in] point Entity looks at this point (= new local z axis)
	* \param[in] up Up vector pointing up
	*
	*/
	class VeMap {
	protected:

	public:
		VeMap() {};
		virtual ~VeMap() {};
		virtual bool		getMappedIndex(		VeTableKeyInt& key, VeIndex& index) { assert(false); return false; };
		virtual uint32_t	getMappedIndices(	VeTableKeyInt& key, std::vector<VeIndex>& result) { assert(false); return 0; };
		virtual bool		getMappedIndex(		VeTableKeyIntPair& key, VeIndex& index) { assert(false); return false; };
		virtual uint32_t	getMappedIndices(	VeTableKeyIntPair& key, std::vector<VeIndex>& result) { assert(false); return 0; };
		virtual bool		getMappedIndex(		VeTableKeyString& key, VeIndex& index) { assert(false); return false; };
		virtual uint32_t	getMappedIndices(	VeTableKeyString& key, std::vector<VeIndex>& result) { assert(false); return 0; };
		virtual uint32_t	getAllIndices(		std::vector<VeIndex>& result) { assert(false); return 0; };
		virtual void		insertIntoMap(		void *entry, VeIndex &dir_index ) { assert(false); };
		virtual uint32_t	deleteFromMap(		void* entry, VeIndex& dir_index) { assert(false); return 0; };

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

		void getKey(void* entry, VeTableIndexPair offset, VeTableIndexPair num_bytes, VeTableKeyIntPair& key) {

			key = VeTableKeyIntPair(getIntFromEntry(entry, offset.first,  num_bytes.first),
									getIntFromEntry(entry, offset.second, num_bytes.second));
		}

		void getKey( void* entry, VeIndex offset, VeIndex num_bytes, std::string &key) {
			uint8_t* ptr = (uint8_t*)entry + offset;
			std::string* pstring = (std::string*)ptr;
			key = *pstring;
		}

	};


	///M is the struct defining a row of the table
	///I is the offset/length type, is either VeIndex or std::pair<VeIndex,VeIndex>
	///K is the map key, is either VeHandle or std::string
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

		virtual uint32_t getMappedIndices(K& key, std::vector<VeIndex>& result) override {
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
			auto it = m_map.emplace( key, dir_index );
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

		VeHandle getHandle(VeIndex dir_index) { return ((VeHandle)m_dir_entries[dir_index].m_auto_id << 32) | (VeHandle)dir_index;  };

		static::std::tuple<VeIndex,VeIndex> splitHandle(VeHandle key ) { return { (VeIndex)(key >> 32), (VeIndex)(key & VE_NULL_INDEX) }; };

		void updateTableIndex(VeIndex dir_index, VeIndex table_index) { m_dir_entries[dir_index].m_table_index = table_index; }

		void deleteEntry( VeIndex dir_index ) {
			m_dir_entries[m_first_free].m_next_free = m_first_free;
			m_first_free = dir_index;
		}
	};


	//------------------------------------------------------------------------------------------------------

	class VeTable {
	protected:
		VeIndex	m_thread_id;			///id of thread that accesses to this table are scheduled to
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
			if ( h1 == h2 || h1 == VE_NULL_HANDLE || h2 == VE_NULL_HANDLE ) return;
			VeIndex first  = getIndexFromHandle(h1);
			VeIndex second = getIndexFromHandle(h2);
			if (first == VE_NULL_INDEX || second == VE_NULL_INDEX) return;
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
		VeIndex		getSize() { return (VeIndex)m_data.size(); };
		const std::vector<T>& getData() { return m_data; };
		void		sortTableByMap( VeIndex num_map );
		void		forAllEntries( VeIndex num_index, std::function<void(VeHandle)>& func );
		void		forAllEntries( VeIndex num_index, std::function<void(VeHandle)>&& func ) { forAllEntries( num_index, func ); };
		void		forAllEntries( std::function<void(VeHandle)>& func)  { forAllEntries( VE_NULL_INDEX, func ); };
		void		forAllEntries( std::function<void(VeHandle)>&& func) { forAllEntries( VE_NULL_INDEX, func ); };

		VeHandle	addEntry(T& entry);
		VeHandle	addEntry(T&& entry) { return addEntry(entry); };
		bool		getEntry(VeHandle key, T& entry);
		bool		updateEntry(VeHandle key, T& entry);
		VeIndex		getIndexFromHandle(VeHandle key);
		VeHandle	getHandleFromIndex(VeIndex table_index);
		bool		deleteEntry(VeHandle key);

		uint32_t	getHandlesFromMap(VeIndex num_map, VeTableKeyInt key, std::vector<VeHandle>& result);
		uint32_t	getHandlesFromMap(VeIndex num_map, VeTableKeyIntPair& key, std::vector<VeHandle>& result);
		uint32_t	getHandlesFromMap(VeIndex num_map, VeTableKeyString& key, std::vector<VeHandle>& result);

		uint32_t	getAllHandlesFromMap(VeIndex num_map, std::vector<VeHandle>& result);
	};

	//--------------------------------------------------------------------------------------------------------------------------

	template<typename T> inline void VeFixedSizeTypedTable<T>::sortTableByMap(VeIndex num_map) {
		assert(!m_read_only);
		assert(num_map < m_maps.size());
		std::vector<VeHandle> handles; handles.reserve(m_maps.size());
		getAllHandlesFromMap(num_map, handles);
		for (uint32_t i = 0; i < m_data.size(); ++i) swapEntriesByHandle( getHandleFromIndex(i), handles[i]);
	}

	template<typename T> inline void VeFixedSizeTypedTable<T>::forAllEntries(VeIndex num_map, std::function<void(VeHandle)>& func) {
		assert(num_map < m_maps.size() || num_map == VE_NULL_INDEX );
		if (m_data.empty()) return;

		std::vector<VeHandle> handles; handles.reserve( m_data.size() + 1 );
		getAllHandlesFromMap(num_map, handles);
		for (auto handle : handles) func(handle);
	}

	template<typename T> inline VeHandle VeFixedSizeTypedTable<T>::addEntry(T& entry) {
		assert(!m_read_only);
		VeIndex table_index = (VeIndex)m_data.size();
		m_data.emplace_back(entry);

		VeHandle handle		= m_directory.addEntry( table_index );
		VeIndex dir_index	= handle & VE_NULL_INDEX;
		m_tbl2dir.emplace_back(dir_index);
		for (auto map : m_maps) map->insertIntoMap( (void*)&entry, dir_index );
		return handle;
	};

	template<typename T> inline bool VeFixedSizeTypedTable<T>::getEntry( VeHandle key, T& entry ) {
		if(key==VE_NULL_HANDLE) return false;
		if (m_data.empty()) return false;

		auto [auto_id, dir_index]	= m_directory.splitHandle( key );
		auto dir_entry				= m_directory.getEntry(dir_index);
		if ( auto_id != dir_entry.m_auto_id ) return false;

		entry = m_data[dir_entry.m_table_index];
		return true;
	};

	template<typename T> inline bool VeFixedSizeTypedTable<T>::updateEntry(VeHandle key, T& entry) {
		if (key == VE_NULL_HANDLE) return false;
		if (m_data.empty()) return false;

		auto [auto_id, dir_index]	= m_directory.splitHandle(key);
		auto dir_entry				= m_directory.getEntry(dir_index);
		if (auto_id != dir_entry.m_auto_id) return false;

		for (auto map : m_maps) map->deleteFromMap((void*)&m_data[dir_entry.m_table_index], dir_index);
		m_data[dir_entry.m_table_index] = entry;
		for (auto map : m_maps) map->insertIntoMap((void*)&entry, dir_index);

		return true;
	};

	template<typename T> inline bool VeFixedSizeTypedTable<T>::deleteEntry(VeHandle key) {
		if (key == VE_NULL_HANDLE) return false;
		if (m_data.empty()) return false;

		assert(!m_read_only);
		auto [auto_id, dir_index]	= m_directory.splitHandle(key);
		auto dir_entry				= m_directory.getEntry(dir_index);
		if (auto_id != dir_entry.m_auto_id) return false;

		VeIndex table_index = dir_entry.m_table_index;
		swapEntriesByHandle( getHandleFromIndex(table_index), getHandleFromIndex((VeIndex)m_data.size() - 1) );

		for (auto map : m_maps) map->deleteFromMap((void*)&m_data[table_index], dir_index);
		m_directory.deleteEntry(dir_index);
		m_data.pop_back();
		m_tbl2dir.pop_back();
		return true;
	};


	template<typename T> inline VeIndex VeFixedSizeTypedTable<T>::getIndexFromHandle(VeHandle key) {
		if (key == VE_NULL_HANDLE) return VE_NULL_INDEX;
		auto [auto_id, dir_index]	= m_directory.splitHandle(key);
		auto dir_entry				= m_directory.getEntry(dir_index);

		if (auto_id != dir_entry.m_auto_id) return VE_NULL_INDEX;
		return dir_entry.m_table_index;
	};


	template<typename T> inline VeHandle VeFixedSizeTypedTable<T>::getHandleFromIndex(VeIndex table_index) {
		VeIndex dir_index	= m_tbl2dir[table_index];
		VeIndex auto_id		= m_directory.getEntry(dir_index).m_auto_id;
		return m_directory.getHandle(dir_index);
	};


	//--------------------------------------------------------------------------------------------------------------------------

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getHandlesFromMap(	VeIndex num_map, 
																						VeTableKeyInt key,
																						std::vector<VeHandle>& result) {
		assert(num_map < m_maps.size());
		if (key == VE_NULL_HANDLE) return 0;
		if (m_data.empty()) return 0;

		std::vector<VeIndex> dir_indices; dir_indices.reserve(initVecLen());
		uint32_t num = m_maps[num_map]->getMappedIndices(key, dir_indices);
		for (auto dir_index : dir_indices) result.emplace_back( m_directory.getHandle(dir_index) );
		return num;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getHandlesFromMap(	VeIndex num_map,
																						VeTableKeyIntPair& key,
																						std::vector<VeHandle>& result) {
		assert(num_map < m_maps.size());
		if (key.first == VE_NULL_HANDLE && key.second == VE_NULL_HANDLE) return 0;
		if (m_data.empty()) return 0;

		std::vector<VeIndex> dir_indices; dir_indices.reserve(initVecLen());
		uint32_t num = m_maps[num_map]->getMappedIndices(key, dir_indices);
		for (auto dir_index : dir_indices) result.emplace_back(m_directory.getHandle(dir_index));
		return num;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getHandlesFromMap(	VeIndex num_map, 
																						VeTableKeyString& key,
																						std::vector<VeHandle>& result) {
		assert(num_map < m_maps.size());
		if (key.empty() || key.size() == 0) return 0;
		if (m_data.empty()) return 0;

		std::vector<VeIndex> dir_indices; dir_indices.reserve(initVecLen());
		uint32_t num = m_maps[num_map]->getMappedIndices(key, dir_indices);
		for (auto dir_index : dir_indices) result.emplace_back(m_directory.getHandle(dir_index));
		return num;
	};


	//--------------------------------------------------------------------------------------------------------------------------


	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getAllHandlesFromMap(VeIndex num_map, std::vector<VeHandle>& result) {
		assert(num_map < m_maps.size() || num_map == VE_NULL_INDEX);
		if (m_data.empty()) return 0;

		if (num_map == VE_NULL_INDEX) {
			for (VeIndex i = 0; i < m_data.size(); ++i) result.emplace_back( getHandleFromIndex(i) );
			return (uint32_t)m_data.size();
		}

		std::vector<VeIndex> dir_indices; dir_indices.reserve(m_data.size() + 1);
		uint32_t num = m_maps[num_map]->getAllIndices(dir_indices);
		for (auto dir_index : dir_indices) result.emplace_back( m_directory.getHandle(dir_index) );
		return num;
	};



	///-------------------------------------------------------------------------------

	class VariableSizeTable : public VeTable {
	protected:

		struct VeDirectoryEntry {
			VeIndex m_start;
			VeIndex m_size;
			VeIndex m_occupied;

			VeDirectoryEntry(VeIndex start, VeIndex size, VeIndex occupied) : m_start(start), m_size(size), m_occupied(occupied) {};
			VeDirectoryEntry() : m_start(VE_NULL_INDEX), m_size(VE_NULL_INDEX), m_occupied(VE_NULL_INDEX) {};
		};

		VeFixedSizeTypedTable<VeDirectoryEntry>*	m_pdirectory = nullptr;
		std::vector<uint8_t>						m_data;


		void defragment() {
			std::vector<VeHandle> handles;  handles.reserve( m_pdirectory->getData().size() + 1 );
			m_pdirectory->getAllHandlesFromMap(2, handles);
			if (handles.size() < 2) return;

			VeDirectoryEntry entry1;
			if (!m_pdirectory->getEntry(handles[0],   entry1)) return;

			for (uint32_t i = 1; i < handles.size(); ++i ) {
				VeDirectoryEntry entry2;
				if (!m_pdirectory->getEntry(handles[i], entry2)) return;

				if (entry1.m_occupied == 0 && entry2.m_occupied == 0) {
					entry1.m_size += entry2.m_size;
					m_pdirectory->deleteEntry(handles[i]);
				}
				else { entry1 = entry2; }
			}
		}

	public:
		VariableSizeTable(VeIndex size = 1<<20, VeIndex thread_id = VE_NULL_INDEX ) : VeTable( thread_id ) {

			std::vector<mem::VeMap*> maps = {
				(mem::VeMap*) new mem::VeTypedMap< std::map<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >(
							(VeIndex)offsetof(struct VeDirectoryEntry, m_occupied), (VeIndex)sizeof(VeDirectoryEntry::m_occupied)),
				(mem::VeMap*) new mem::VeTypedMap< std::map<VeTableKeyInt, VeIndex>, VeTableKeyInt, VeTableIndex >(
							(VeIndex)offsetof(struct VeDirectoryEntry, m_start), (VeIndex)sizeof(VeDirectoryEntry::m_start)),
				(mem::VeMap*) new mem::VeTypedMap< std::map<VeTableKeyIntPair, VeTableIndex>, VeTableKeyIntPair, VeTableIndexPair > (
							VeTableIndexPair((VeIndex)offsetof(struct VeDirectoryEntry, m_occupied), 
											 (VeIndex)offsetof(struct VeDirectoryEntry, m_start)),
							VeTableIndexPair((VeIndex)sizeof(VeDirectoryEntry::m_occupied), (VeIndex)sizeof(VeDirectoryEntry::m_start)) )
			};
			m_pdirectory = new VeFixedSizeTypedTable<VeDirectoryEntry>( maps );

			m_data.resize(size);
			VeDirectoryEntry entry{ 0, size, 0 };
			m_pdirectory->addEntry(entry);
		}

		~VariableSizeTable() {
			delete m_pdirectory;
		};

		VeIndex insertBlob( uint8_t* ptr, VeIndex size, bool defrag = true ) {
			std::vector<VeHandle> result;
			m_pdirectory->getHandlesFromMap(0, 0, result );

			VeHandle h = VE_NULL_HANDLE;
			VeDirectoryEntry entry;
			for (auto handle : result) if (m_pdirectory->getEntry(handle, entry) && entry.m_size >= size) {	h = handle; break; } 
			if (h == VE_NULL_HANDLE) {
				if( !defrag ) return VE_NULL_INDEX;
				defragment();
				return insertBlob(ptr, size, false);
			}

			if (entry.m_size > size) { 
				VeDirectoryEntry newentry{ entry.m_start + size, entry.m_size - size, 0 };
				m_pdirectory->addEntry(newentry);
			}
			entry.m_size = size;
			entry.m_occupied = 1;
			m_pdirectory->updateEntry(h, entry);
			return entry.m_start;
		}

		bool deleteBlob(VeIndex start ) {
			std::vector<VeHandle> result;
			m_pdirectory->getHandlesFromMap(1, start, result);
			if (result.size() != 1) return false;

			VeDirectoryEntry entry;
			if( !m_pdirectory->getEntry(result[0], entry) ) return false;

			entry.m_occupied = 0;
			m_pdirectory->updateEntry(result[0], entry);
			return true;
		}

	};


}


