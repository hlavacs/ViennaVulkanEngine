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
		virtual void		mapHandleToIndex(	VeHandle & auto_id, void *entry, VeIndex &dir_index ) { assert(false); };

		VeHandle getHandle( void *entry, VeIndex offset ) {

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
			for (auto it = range.first; it != range.second; ++it) result.push_back( it->second );
			return num;
		};

		virtual void mapHandleToIndex( VeHandle& auto_counter, void* entry, VeIndex& dir_index ) override {

			m_map.try_emplace( key, dir_index );
		};
	};



	//------------------------------------------------------------------------------------------------------

	class VeDirectory {
	protected:

		struct VeDirectoryEntry {
			VeHandle	m_auto_id = VE_NULL_HANDLE;
			VeIndex		m_table_index = VE_NULL_INDEX;	///index into the entry table
			VeIndex		m_next_free = VE_NULL_INDEX;	///index of next free entry in directory

			VeDirectoryEntry( VeHandle auto_id, VeIndex table_index, VeIndex next_free) : 
				m_auto_id(auto_id), m_table_index(table_index), m_next_free(next_free) {}
		};

		VeHandle						m_auto_counter = 0;				///
		std::vector<VeDirectoryEntry>	m_dir_entries;					///1 level of indirection, idx into the entry table
		VeIndex							m_first_free = VE_NULL_INDEX;	///index of first free entry in directory

		void addNewEntry(VeIndex table_index, VeIndex& dir_index, VeHandle& auto_id) {
			auto_id = ++m_auto_counter;
			m_dir_entries.emplace_back( auto_id, table_index, m_first_free );
			dir_index = (VeIndex)m_dir_entries.size() - 1;
		}

		void writeOverOldEntry(VeIndex table_index, VeIndex& dir_index, VeHandle& auto_id) {
			auto_id						= ++m_auto_counter;
			dir_index					= m_first_free;
			VeIndex next_free			= m_dir_entries[dir_index].m_next_free;
			m_dir_entries[dir_index]	= { auto_id, table_index, VE_NULL_INDEX };
			m_first_free				= next_free;
		}

	public:
		VeDirectory() {};
		~VeDirectory() {};

		void addEntry( VeIndex table_index, VeIndex &dir_index, VeHandle &auto_id ) {
			if (m_first_free == VE_NULL_INDEX) addNewEntry(table_index, dir_index, auto_id);
			else writeOverOldEntry(table_index, dir_index, auto_id);
		};

		VeDirectoryEntry& getEntry( VeIndex dir_index) { return m_dir_entries[dir_index];  };

		void removeEntry( VeIndex dir_index ) {
			m_dir_entries[m_first_free].m_next_free = m_first_free;
			m_first_free = dir_index;
		}

		void enableAutoCounter() { m_auto_counter = 0; };
	};


	//------------------------------------------------------------------------------------------------------

	class VeFixedSizeTable {
	protected:
		VeIndex	m_thread_id;	///id of thread that accesses to this table are scheduled to

	public:
		VeFixedSizeTable( VeIndex thread_id ) : m_thread_id(thread_id) {};
		virtual ~VeFixedSizeTable() {};
		VeIndex	getThreadId() { return m_thread_id; };
	};


	//------------------------------------------------------------------------------------------------------

	template <typename T>
	class VeFixedSizeTypedTable : public VeFixedSizeTable {
	protected:
		std::vector<VeMap*>		m_maps;				///vector of maps for quickly finding or sorting entries
		VeDirectory				m_directory;		///
		std::vector<T>			m_data;				///growable entry data table
		std::vector<VeIndex>	m_tbl2dir;

	public:

		VeFixedSizeTypedTable( std::vector<VeMap*> &&maps, VeIndex thread_id = VE_NULL_INDEX) :
			VeFixedSizeTable( thread_id ) {

			if (maps.size() > 0) m_maps = std::move(maps);
			else {
				m_maps.emplace_back( (VeMap*) new std::unordered_map<VeHandle, VeIndex >() );
				m_directory.enableAutoCounter();
			}
		
		};

		~VeFixedSizeTypedTable() { for (uint32_t i = 0; i < m_maps.size(); ++i ) delete m_maps[i]; };

		std::vector<T>& getData() { return m_data; };

		void addEntry(T& te);
		bool getEntry(VeIndex num_map, VeHandle key, T& entry);
		bool getEntry(VeIndex num_map, std::pair<VeHandle,VeHandle> &key, T& entry);
		bool getEntry(VeIndex num_map, std::string key, T& entry);

		uint32_t getEntries(VeIndex num_map, VeHandle key, std::vector<T>& result);
		uint32_t getEntries(VeIndex num_map, std::pair<VeHandle, VeHandle>& key, std::vector<T>& result);
		uint32_t getEntries(VeIndex num_map, std::string& key, std::vector<T>& result);

		uint32_t getSortedEntries(VeIndex num_map, std::vector<T>& result);

		uint32_t deleteEntries(VeIndex num_map, VeHandle key );
		uint32_t deleteEntries(VeIndex num_map, std::pair<VeHandle, VeHandle>& key);
		uint32_t deleteEntries(VeIndex num_map, std::string& key);
	};

	template<typename T> inline void VeFixedSizeTypedTable<T>::addEntry(T& te) {
		VeIndex table_index = (VeIndex)m_data.size();
		m_data.emplace_back(te);

		VeIndex dir_index;
		VeHandle auto_id;
		m_directory.addEntry(table_index, dir_index, auto_id);

		m_tbl2dir.emplace_back(dir_index);

		for (auto map : m_maps) map->mapHandleToIndex(auto_id, (void*)&te, dir_index);
	};

	template<typename T> inline bool VeFixedSizeTypedTable<T>::getEntry(VeIndex num_map, VeHandle key, T& entry) {
		VeIndex dir_index;
		if (!m_maps[num_map]->getMappedIndex(key, dir_index)) return false;
		entry = m_data[m_directory.getEntry(dir_index).m_table_index];
		return true;
	};

	template<typename T> inline bool VeFixedSizeTypedTable<T>::getEntry(VeIndex num_map, std::pair<VeHandle, VeHandle>& key, T& entry) {
		VeIndex dir_index;
		if (!m_maps[num_map]->getMappedIndex(key, dir_index)) return false;
		entry = m_data[m_directory.getEntry(dir_index).m_table_index];
		return true;
	};

	template<typename T> inline	bool VeFixedSizeTypedTable<T>::getEntry(VeIndex num_map, std::string key, T& entry) {
		VeIndex dir_index;
		if (!m_maps[num_map]->getMappedIndex(key, dir_index)) return false;
		entry = m_data[m_directory.getEntry(dir_index).m_table_index];
		return true;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getEntries(VeIndex num_map, VeHandle key, std::vector<T>& result) {
		return 0;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getEntries(VeIndex num_map, std::pair<VeHandle, VeHandle>& key, std::vector<T>& result) {
		return 0;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getEntries(VeIndex num_map, std::string& key, std::vector<T>& result) {
		return 0;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::getSortedEntries(VeIndex num_map, std::vector<T>& result) {
		return 0;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::deleteEntries(VeIndex num_map, VeHandle key) {

		return 0;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::deleteEntries(VeIndex num_map, std::pair<VeHandle, VeHandle>& key) {
		return 0;
	};

	template<typename T> inline	uint32_t VeFixedSizeTypedTable<T>::deleteEntries(VeIndex num_map, std::string& key) {
		return 0;
	};



	///-------------------------------------------------------------------------------
	struct VariableSizeTable {
		//TableSortIndex		m_indices;
		std::vector<uint8_t>	m_data;
	};


}


