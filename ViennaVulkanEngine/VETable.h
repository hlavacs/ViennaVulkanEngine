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

	using VeTableKeyInt = VeHandle;
	using VeTableKeyIntPair = std::pair<VeTableKeyInt, VeTableKeyInt>;
	using VeTableKeyIntTriple = std::tuple<VeTableKeyInt, VeTableKeyInt, VeTableKeyInt>;
	using VeTableKeyString = std::string;
	using VeTableIndex = VeIndex;
	using VeTableIndexPair = std::pair<VeTableIndex, VeTableIndex>;
	using VeTableIndexTriple = std::tuple<VeTableIndex, VeTableIndex, VeTableIndex>;
	using VeTableHandlePair = std::pair<VeHandle, VeHandle>;


	/**
	*
	* \brief 
	*
	*
	*/
	class VeMap {
	protected:

	public:
		VeMap() {};
		virtual ~VeMap() {};
		virtual void		operator=(const VeMap& map) { assert(false); return; };
		virtual void		clear() { assert(false); return; };
		virtual VeMap*		clone() { assert(false); return nullptr;};
		
		virtual bool		getMappedIndexEqual(	VeTableKeyInt key, VeIndex& index) { assert(false); return false; };
		virtual bool		getMappedIndexEqual(	VeTableKeyIntPair key, VeIndex& index) { assert(false); return false; };
		virtual bool		getMappedIndexEqual(	VeTableKeyIntTriple key, VeIndex& index) { assert(false); return false; };
		virtual bool		getMappedIndexEqual(	VeTableKeyString key, VeIndex& index) { assert(false); return false; };

		virtual uint32_t	getMappedIndicesEqual(	VeTableKeyInt key, std::vector<VeIndex>& result) { assert(false); return 0; };
		virtual uint32_t	getMappedIndicesEqual(	VeTableKeyIntPair key, std::vector<VeIndex>& result) { assert(false); return 0; };
		virtual uint32_t	getMappedIndicesEqual(	VeTableKeyIntTriple key, std::vector<VeIndex>& result) { assert(false); return 0; };
		virtual uint32_t	getMappedIndicesEqual(	VeTableKeyString key, std::vector<VeIndex>& result) { assert(false); return 0; };

		virtual uint32_t	getMappedIndicesRange(	VeTableKeyInt lower, VeTableKeyInt upper, std::vector<VeIndex>& result) { assert(false); return 0; };
		virtual uint32_t	getMappedIndicesRange(	VeTableKeyIntPair lower, VeTableKeyIntPair upper, std::vector<VeIndex>& result) { assert(false); return 0; };
		virtual uint32_t	getMappedIndicesRange(	VeTableKeyIntTriple lower, VeTableKeyIntTriple upper, std::vector<VeIndex>& result) { assert(false); return 0; };
		virtual uint32_t	getMappedIndicesRange(	VeTableKeyString lower, VeTableKeyString upper, std::vector<VeIndex>& result) { assert(false); return 0; };

		virtual uint32_t	getAllIndices(std::vector<VeIndex>& result) { assert(false); return 0; };
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
		VeHandle getIntFromEntry(void* entry, VeIndex offset, VeIndex num_bytes ) {
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
		void getKey( void* entry, VeIndex offset, VeIndex num_bytes, VeHandle& key) {
			key = getIntFromEntry( entry, offset, num_bytes );
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
		void getKey(void* entry, VeTableIndexPair offset, VeTableIndexPair num_bytes, VeTableKeyIntPair& key) {
			key = VeTableKeyIntPair(getIntFromEntry(entry, offset.first,  num_bytes.first),
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
		void getKey(void* entry, VeTableIndexTriple offset, VeTableIndexTriple num_bytes, VeTableKeyIntTriple& key) {
			key = VeTableKeyIntTriple(	getIntFromEntry(entry, std::get<0>(offset), std::get<0>(num_bytes)),
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
		void getKey( void* entry, VeIndex offset, VeIndex num_bytes, std::string &key) {
			uint8_t* ptr = (uint8_t*)entry + offset;
			std::string* pstring = (std::string*)ptr;
			key = *pstring;
		}

	};


	/**
	*
	* \brief
	*
	*	M is either std::map or std::unordered_map
	*	I is the offset/length type, is either VeTableIndex or VeTableIndexPair
	*	K is the map key, is either VeTableKeyInt, VeTableKeyIntPair, VeTableKeyIntTriple, or VeTableKeyString
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

		VeTypedMap(I offset, I num_bytes) : VeMap(), m_offset(offset), m_num_bytes(num_bytes) {};
		VeTypedMap(const VeTypedMap<M,K,I> & map) : VeMap(), m_offset(map.m_offset), m_num_bytes(map.m_num_bytes), m_map(map.m_map) {};
		virtual	~VeTypedMap() {};

		virtual void operator=(const VeMap& basemap) {
			VeTypedMap<M,K,I>* map = &((VeTypedMap<M, K, I> &)basemap);
			m_offset	= map->m_offset;
			m_num_bytes = map->m_num_bytes;
			m_map		= map->m_map;
		};

		virtual void clear() {
			m_map.clear();
		}

		virtual bool getMappedIndexEqual( K key, VeIndex &index ) override {
			if constexpr (std::is_same_v< M, std::multimap<K, VeIndex > > || std::is_same_v< M, std::unordered_multimap<K, VeIndex > >) {
				assert(false);
				return false;
			} else {
				auto search = m_map.find(key);
				if (search == m_map.end())
					return false;
				index = search->second;
			}
			return true;
		}

		virtual uint32_t getMappedIndicesEqual(K key, std::vector<VeIndex>& result) override {
			uint32_t num = 0;
			
			auto range = m_map.equal_range(key);
			for (auto it = range.first; it != range.second; ++it, ++num)
				result.emplace_back(it->second);
			return num;
		};

		virtual uint32_t getMappedIndicesRange(K lower, K upper, std::vector<VeIndex>& result) override {
			uint32_t num = 0;

			if constexpr (std::is_same_v< M, std::unordered_map<K, VeIndex > > || std::is_same_v< M, std::unordered_multimap<K, VeIndex > >) {
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

		virtual uint32_t getAllIndices( std::vector<VeIndex>& result) override {
			for (auto entry : m_map) 
				result.emplace_back(entry.second); 
			return (uint32_t)m_map.size();
		}

		uint32_t leftJoin(VeTypedMap& other, std::vector<std::pair<VeIndex,VeIndex>> result ) {
			if (m_map.size() == 0 || other.m_map.size() == 0) return 0;
			uint32_t num = 0;
			std::vector <VeIndex> res_list;
			auto iter = m_map.begin();
			K key = iter->first;
			other.getMappedIndicesEqual(key, res_list);

			while (iter != m_map.end()) {
				for (auto res : res_list) {
					result.emplace_back(std::pair<VeIndex, VeIndex>(iter->second, res));
					num++;
				}
				++iter;
				if (iter != m_map.end() && key != iter->first) {
					res_list.clear();
					key = iter->first;
					other.getMappedIndicesEqual(key, res_list);
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
			VeIndex	m_auto_id		= VE_NULL_INDEX;
			VeIndex	m_table_index	= VE_NULL_INDEX;	///index into the entry table
			VeIndex	m_next_free		= VE_NULL_INDEX;	///index of next free entry in directory

			VeDirectoryEntry() : m_auto_id(VE_NULL_INDEX), m_table_index(VE_NULL_INDEX), m_next_free(VE_NULL_INDEX) {}
			VeDirectoryEntry(VeIndex auto_id, VeIndex table_index, VeIndex next_free) :
				m_auto_id(auto_id), m_table_index(table_index), m_next_free(next_free) {};
			VeDirectoryEntry(const VeDirectoryEntry & entry) : 
				m_auto_id(entry.m_auto_id), m_table_index(entry.m_table_index), m_next_free(entry.m_next_free) {};
		};

		VeIndex							m_auto_counter = 0;				///
		std::vector<VeDirectoryEntry>	m_dir_entries;					///1 level of indirection, idx into the data table
		VeIndex							m_first_free = VE_NULL_INDEX;	///index of first free entry in directory

		VeHandle addNewEntry(VeIndex table_index ) {
			VeIndex auto_id = m_auto_counter; ++m_auto_counter;
			VeIndex dir_index = (VeIndex)m_dir_entries.size();
			m_dir_entries.emplace_back( auto_id, table_index, VE_NULL_INDEX  );
			return getHandle(dir_index);
		}

		VeHandle writeOverOldEntry(VeIndex table_index) {
			VeIndex auto_id				= m_auto_counter; ++m_auto_counter;
			VeIndex dir_index			= m_first_free;
			VeIndex next_free			= m_dir_entries[dir_index].m_next_free;
			m_dir_entries[dir_index]	= { auto_id, table_index, VE_NULL_INDEX };
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
			return (VeHandle)m_dir_entries[dir_index].m_auto_id | ((VeHandle)dir_index << 32);  
		};

		static::std::tuple<VeIndex,VeIndex> splitHandle(VeHandle key ) { 
			return { (VeIndex)(key & VE_NULL_INDEX ), (VeIndex)(key >> 32) };
		};

		bool isValid(VeIndex dir_index) {
			if (dir_index >= m_dir_entries.size() || m_dir_entries[dir_index].m_next_free != VE_NULL_INDEX )
				return false;
			return true;
		}

		bool isValid(VeHandle handle) {
			if (handle == VE_NULL_HANDLE) return false;
			auto [auto_id, dir_index] = splitHandle(handle);
			if (!isValid(dir_index) || auto_id != m_dir_entries[dir_index].m_auto_id) 
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
		VeIndex		m_thread_id = 0;			///id of thread that accesses to this table are scheduled to
		bool		m_read_only = false;
		VeTable*	m_read_table = this;
		VeTable*	m_write_table = this;
		bool		m_clear_on_swap = false;
		uint32_t	m_table_nr = 0;

		std::chrono::high_resolution_clock::time_point t1, t2;
		double		m_sum_time = 0.0;
		double		m_avg_time = 0.0;
		VeIndex		m_in = 0;

		inline void in() { 
			++m_in;
			if (m_in > 1) return;
			t1 = std::chrono::high_resolution_clock::now(); 
		};

		inline void out() {
			--m_in;
			if (m_in > 0) return;
			t2 = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
			m_sum_time += time_span.count();
		};

		VeTable* getCompanionTable() {
			if (m_read_table != this)
				return m_read_table;
			if (m_write_table != this)
				return m_write_table;
			return nullptr;
		}


	public:
		VeTable(bool clear_on_swap = false) : m_clear_on_swap(clear_on_swap) {};

		VeTable(VeTable& table) : m_thread_id(table.m_thread_id), m_read_only(!table.m_read_only), m_clear_on_swap(table.m_clear_on_swap) {
			m_read_table = this;
			m_write_table = &table;
			if (!m_read_only) {
				std::swap(m_read_table, m_write_table);
			}
			table.m_read_table = m_read_table;
			table.m_write_table = m_write_table;
			m_table_nr = table.m_table_nr + 1;
		};

		virtual ~VeTable() {};

		void operator=(VeTable& tab) { assert(false);  return; };
		virtual void clear() { assert(false);  return; };

		void setThreadId(VeIndex id) {
			if (m_thread_id == id)
				return;

			m_thread_id = id;
			VeTable* companion_table = getCompanionTable();
			if (companion_table != nullptr)
				companion_table->setThreadId(id);
		};
		VeIndex	getThreadId() { return m_thread_id; };

		virtual void setReadOnly(bool ro) { m_read_only = ro; };
		bool	getReadOnly() { return m_read_only;  };

		double  getAvgTime() {
			m_avg_time = 0.95 * m_avg_time + 0.05 * m_sum_time;
			m_sum_time = 0.0;
			return m_avg_time;
		};

		VeTable* getReadTablePtr() { return m_read_table; };
		VeTable* getWriteTablePtr() { return m_write_table; };

		void swapTables() {
			if (m_read_table == m_write_table)
				return;

			m_read_table->setReadOnly(false);
			m_write_table->setReadOnly(true);

			if (m_clear_on_swap) {
				m_read_table->clear();
			}
			else
				*m_read_table = *m_write_table;

			std::swap(m_read_table, m_write_table);
			std::swap(getCompanionTable()->m_read_table, getCompanionTable()->m_write_table);
		};

		//need these for leftJoin
		VeMap* getMap(VeIndex num_map) { assert(false);  return nullptr; };
		VeDirectory* getDirectory() { assert(false);  return nullptr; };

	};


	//------------------------------------------------------------------------------------------------------

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
		VeDirectory				m_directory;		///
		VeVector<T>				m_data;				///growable entry data table
		std::vector<VeIndex>	m_tbl2dir;
		VeIndex initVecLen() { return (VeIndex)m_data.size() / 3; };

	public:

		VeFixedSizeTable(bool memcopy = false, bool clear_on_swap = false, VeIndex align = 16, VeIndex capacity = 16) : 
			VeTable(clear_on_swap), m_data(memcopy, align, capacity) {};

		VeFixedSizeTable( std::vector<VeMap*> &maps, bool memcopy = false, bool clear_on_swap = false, VeIndex align = 16, VeIndex capacity = 16) :
			VeTable(clear_on_swap), m_maps(maps), m_data(memcopy, align, capacity) {};

		VeFixedSizeTable(VeFixedSizeTable<T>& table) : VeTable(table), m_data(table.m_data), m_directory(table.m_directory), m_tbl2dir(table.m_tbl2dir) {
			for (auto map : table.m_maps)
				m_maps.emplace_back(map->clone());
		};

		virtual ~VeFixedSizeTable() { 
			for (uint32_t i = 0; i < m_maps.size(); ++i ) 
				delete m_maps[i]; 
		};

		//neutral operations
		void addMap(VeMap* pmap) { in(); m_maps.emplace_back(pmap); out(); };

		//write operations - must be run in a job
		void		operator=( VeFixedSizeTable<T>& table);
		void		swapEntriesByHandle(VeHandle h1, VeHandle h2);
		virtual void clear();
		void		sortTableByMap( VeIndex num_map );
		VeHandle	addEntry(T entry, VeHandle *pHandle = nullptr );
		bool		updateEntry(VeHandle key, T entry);
		bool		deleteEntry(VeHandle key);

		// read operations
		bool		isValid(VeHandle handle);
		const VeVector<T>& getData() { return m_data; };
		VeMap*		getMap(VeIndex num_map) { return m_maps[num_map]; };
		VeDirectory* getDirectory() { return &m_directory; };
		VeIndex		getSize() { return (VeIndex)m_data.size(); };
		bool		getEntry(VeHandle key, T& entry);
		VeIndex		getIndexFromHandle(VeHandle key);
		VeHandle	getHandleFromIndex(VeIndex table_index);
		uint32_t	getAllHandles(std::vector<VeHandle>& result);
		uint32_t	getAllHandlesFromMap(VeIndex num_map, std::vector<VeHandle>& result);	//makes sense for map/multimap

		template <typename M, typename K, typename I>
		VeIndex leftJoin(VeIndex own_map, VeTable& table, VeIndex other_map, std::vector<std::pair<VeHandle, VeHandle>>& result);

		template <typename K>
		VeHandle	getHandleEqual(VeIndex num_map, K key );	//use this in map

		template <typename K>
		uint32_t	getHandlesEqual(VeIndex num_map, K key, std::vector<VeHandle>& result);	//use this in multimap

		template <typename K>
		uint32_t	getHandlesRange(VeIndex num_map, K lower, K upper, std::vector<VeHandle>& result); //do not use in unordered map/multimap

		void		forAllEntries(VeIndex num_map, std::function<void(VeHandle)>& func);
		void		forAllEntries(VeIndex num_map, std::function<void(VeHandle)>&& func) { forAllEntries(num_map, func); };
		void		forAllEntries(std::function<void(VeHandle)>& func) { forAllEntries(VE_NULL_INDEX, func); };
		void		forAllEntries(std::function<void(VeHandle)>&& func) { forAllEntries(VE_NULL_INDEX, func); };
	};

	//--------------------------------------------------------------------------------------------------------------------------

	template<typename T> inline void VeFixedSizeTable<T>::operator=( VeFixedSizeTable<T>& table) {
		in();
		assert(!m_read_only);
		m_directory = table.m_directory;
		m_data		= table.m_data;
		for (uint32_t i = 0; i < table.m_maps.size(); ++i ) 
			*(m_maps[i]) = *(table.m_maps[i]);
		m_tbl2dir	= table.m_tbl2dir;
		out();
	}


	//--------------------------------------------------------------------------------------------------------------------------

	template<typename T> inline void VeFixedSizeTable<T>::sortTableByMap(VeIndex num_map) {
		in();
		assert(!m_read_only && num_map < m_maps.size());
		std::vector<VeHandle> handles; 
		handles.reserve(m_maps.size());
		getAllHandlesFromMap(num_map, handles);
		for (uint32_t i = 0; i < m_data.size(); ++i) 
			swapEntriesByHandle( getHandleFromIndex(i), handles[i]);
		out();
	}

	template<typename T> inline VeHandle VeFixedSizeTable<T>::addEntry(T entry, VeHandle* pHandle) {
		in();
		assert(!m_read_only);
		VeIndex table_index = (VeIndex)m_data.size();
		m_data.emplace_back(entry);

		VeHandle handle		= m_directory.addEntry( table_index );
		auto [auto_id, dir_index] = VeDirectory::splitHandle( handle );
		m_tbl2dir.emplace_back(dir_index);
		for (auto map : m_maps) 
			map->insertIntoMap( (void*)&entry, dir_index );
		out();
		if (pHandle != nullptr) *pHandle = handle;
		return handle;
	};

	template<typename T> inline bool VeFixedSizeTable<T>::updateEntry(VeHandle handle, T entry) {
		in();
		assert(!m_read_only);
		if (!isValid(handle) || m_data.empty()) {
			out();
			return false;
		}

		auto [auto_id, dir_index]	= m_directory.splitHandle(handle);
		auto dir_entry				= m_directory.getEntry(dir_index);
		if (auto_id != dir_entry.m_auto_id) {
			out();
			return false;
		}

		for (auto map : m_maps) 
			map->deleteFromMap((void*)&m_data[dir_entry.m_table_index], dir_index);
		m_data[dir_entry.m_table_index] = entry;
		for (auto map : m_maps) 
			map->insertIntoMap((void*)&entry, dir_index);
		out();
		return true;
	};

	template<typename T> inline void VeFixedSizeTable<T>::swapEntriesByHandle(VeHandle h1, VeHandle h2) {
		in();
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
		std::swap(m_tbl2dir[first], m_tbl2dir[second]);
		m_directory.updateTableIndex(m_tbl2dir[first], first);
		m_directory.updateTableIndex(m_tbl2dir[second], second);
		out();
	};

	template<typename T> inline bool VeFixedSizeTable<T>::deleteEntry(VeHandle key) {
		in();
		assert(!m_read_only);
		if (key == VE_NULL_HANDLE || m_data.empty()) {
			out();
			return false;
		}

		auto [auto_id, dir_index]	= m_directory.splitHandle(key);
		auto dir_entry				= m_directory.getEntry(dir_index);
		if (auto_id != dir_entry.m_auto_id) {
			out();
			return false;
		}

		VeIndex table_index = dir_entry.m_table_index;
		swapEntriesByHandle( key, getHandleFromIndex((VeIndex)m_data.size() - 1) );

		for (auto map : m_maps) 
			map->deleteFromMap((void*)&m_data[(VeIndex)m_data.size() - 1], dir_index);
		m_directory.deleteEntry(dir_index);
		m_data.pop_back();
		m_tbl2dir.pop_back();
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

		auto [auto_id, dir_index]	= m_directory.splitHandle(key);
		auto dir_entry				= m_directory.getEntry(dir_index);

		VeIndex result = dir_entry.m_table_index;
		if (auto_id != dir_entry.m_auto_id) 
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

		auto [auto_id, dir_index] = m_directory.splitHandle(key);
		auto dir_entry = m_directory.getEntry(dir_index);
		if (auto_id != dir_entry.m_auto_id)
			return false;

		entry = m_data[dir_entry.m_table_index];
		out();
		return true;
	};

	template<typename T> inline VeHandle VeFixedSizeTable<T>::getHandleFromIndex(VeIndex table_index) {
		in();
		VeIndex dir_index	= m_tbl2dir[table_index];
		VeIndex auto_id		= m_directory.getEntry(dir_index).m_auto_id;
		VeHandle handle		= m_directory.getHandle(dir_index);
		out();
		return handle;
	};

	template<typename T> inline void VeFixedSizeTable<T>::clear() {
		in();
		for (auto map : m_maps) map->clear();
		m_data.clear();
		m_directory.clear();
		m_tbl2dir.clear();
		out();
	}


	//--------------------------------------------------------------------------------------------------------------------------

	template<typename T>
	template <typename M, typename K, typename I>
	VeIndex VeFixedSizeTable<T>::leftJoin(	VeIndex own_map, VeTable& table, 
											VeIndex other_map, std::vector<VeTableHandlePair>& result) {
		in();
		VeTypedMap<M, K, I>* l = (VeTypedMap<M, K, I>*)getMap(own_map);
		VeTypedMap<M, K, I>* r = (VeTypedMap<M, K, I>*)table.getMap(other_map);

		VeIndex num = 0;
		std::vector<VeTableIndexPair> dir_indices;
		l->leftJoin(*r, dir_indices);
		for (auto [first, second] : dir_indices) {
			result.emplace_back(m_directory.getHandle(first), table.getDirectory()->getHandle(second) );
			++num;
		}
		out();
		return num;
	}

	template<typename T>
	template<typename K>
	inline VeHandle VeFixedSizeTable<T>::getHandleEqual(VeIndex num_map, K key ) {
		in();
		assert(num_map < m_maps.size());
		if (m_data.empty()) {
			out();
			return VE_NULL_HANDLE;
		}
		VeIndex dir_index;
		bool found = m_maps[num_map]->getMappedIndexEqual(key, dir_index);
		VeHandle result = m_directory.getHandle(dir_index);
		out();
		return result;
	};

	template<typename T>
	template<typename K>
	inline uint32_t VeFixedSizeTable<T>::getHandlesEqual(VeIndex num_map, K key, std::vector<VeHandle>& result) {
		in();
		assert(num_map < m_maps.size());
		if (m_data.empty()) {
			out();
			return 0;
		}
		uint32_t num = 0;
		std::vector<VeIndex> dir_indices;
		dir_indices.reserve(initVecLen());

		num = m_maps[num_map]->getMappedIndicesEqual( key, dir_indices);
		for (auto dir_index : dir_indices)
			result.emplace_back(m_directory.getHandle(dir_index));
		out();
		return num;
	};

	template <typename T>
	template <typename K> 
	inline uint32_t VeFixedSizeTable<T>::getHandlesRange(VeIndex num_map, K lower, K upper, std::vector<VeHandle>& result) {
		in();
		assert(num_map < m_maps.size());
		if (m_data.empty()) {
			out();
			return 0;
		}
		uint32_t num = 0;
		std::vector<VeIndex> dir_indices;
		dir_indices.reserve(initVecLen());

		num = m_maps[num_map]->getMappedIndicesRange(lower, upper, dir_indices);
		for (auto dir_index : dir_indices)
			result.emplace_back(m_directory.getHandle(dir_index));
		out();
		return num;
	}

	template<typename T> inline	uint32_t VeFixedSizeTable<T>::getAllHandles(std::vector<VeHandle>& result) {
		in();
		for (VeIndex i = 0; i < m_data.size(); ++i)
			result.emplace_back(getHandleFromIndex(i));
		out();
		return (uint32_t)m_data.size();
	}

	template<typename T> inline	uint32_t VeFixedSizeTable<T>::getAllHandlesFromMap(VeIndex num_map, std::vector<VeHandle>& result) {
		if (m_data.empty())
			return 0;

		if (num_map == VE_NULL_INDEX)
			return getAllHandles( result );

		in();
		assert(num_map < m_maps.size());
		std::vector<VeIndex> dir_indices; 
		dir_indices.reserve((VeIndex)m_data.size() + 1);
		uint32_t num = m_maps[num_map]->getAllIndices(dir_indices);
		for (auto dir_index : dir_indices) 
			result.emplace_back( m_directory.getHandle(dir_index) );
		out();
		return num;
	};


	template<typename T> inline void VeFixedSizeTable<T>::forAllEntries(VeIndex num_map, std::function<void(VeHandle)>& func) {
		in();
		assert(num_map == VE_NULL_INDEX || num_map < m_maps.size());
		if (m_data.empty()) {
			out();
			return;
		}

		std::vector<VeHandle> handles;
		handles.reserve(m_data.size());
		getAllHandlesFromMap(num_map, handles);
		for (auto handle : handles)
			func(handle);
		out();
	}


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

		VeFixedSizeTableMT<T>(bool memcopy = false, bool clear_on_swap = false, VeIndex align = 16, VeIndex capacity = 16) :
			VeFixedSizeTable<T>(memcopy, clear_on_swap, align, capacity) {};

		VeFixedSizeTableMT<T>(std::vector<VeMap*>& maps, bool memcopy = false, bool clear_on_swap = false, VeIndex align = 16, VeIndex capacity = 16) :
			VeFixedSizeTable<T>(maps, memcopy, clear_on_swap, align, capacity) {};

		VeFixedSizeTableMT<T>(VeFixedSizeTable<T>& table) : VeFixedSizeTable<T>(table) {};

		virtual ~VeFixedSizeTableMT<T>() {};

		//----------------------------------------------------------------------------

		virtual void operator=(VeFixedSizeTableMT<T>& table) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_write_table;
			VeFixedSizeTable<T>* other = (VeFixedSizeTable<T>*)&table;
			JADDT( me->VeFixedSizeTable<T>::operator=(*other), this->m_thread_id);
		};

		virtual void swapEntriesByHandle(VeHandle h1, VeHandle h2) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_write_table;
			JADDT(me->VeFixedSizeTable<T>::swapEntriesByHandle(h1,h2), this->m_thread_id);
		};

		virtual void clear() {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_write_table;
			JADDT(me->VeFixedSizeTable<T>::clear(), this->m_thread_id);
		};

		virtual void sortTableByMap(VeIndex num_map) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_write_table;
			JADDT(me->VeFixedSizeTable<T>::sortTableByMap(num_map), this->m_thread_id);
		};

		virtual VeHandle addEntry(T entry, VeHandle* pHandle = nullptr) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_write_table;
			JADDT(me->VeFixedSizeTable<T>::addEntry(entry, pHandle), this->m_thread_id);
			return VE_NULL_HANDLE;
		};

		virtual bool updateEntry(VeHandle key, T entry) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_write_table;
			JADDT(me->VeFixedSizeTable<T>::updateEntry(key, entry), this->m_thread_id);
			return true;
		};

		virtual bool deleteEntry(VeHandle key) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_write_table;
			JADDT(me->VeFixedSizeTable<T>::deleteEntry(key), this->m_thread_id);
			return true;
		};


		//----------------------------------------------------------------------------

		// read operations
		bool isValid(VeHandle handle) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			return me->VeFixedSizeTable<T>::isValid(handle);
		};

		const VeVector<T>& getData() { 
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			return me->VeFixedSizeTable<T>::getData();
		};

		VeMap* getMap(VeIndex num_map) { 
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			return me->VeFixedSizeTable<T>::getMap(num_map);
		};

		VeDirectory* getDirectory() { 
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			return me->VeFixedSizeTable<T>::getDirectory();
		};

		VeIndex	getSize() { 
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			return me->VeFixedSizeTable<T>::getSize();
		};

		bool getEntry(VeHandle key, T& entry) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			return me->VeFixedSizeTable<T>::getEntry(key, entry);
		};

		VeIndex	getIndexFromHandle(VeHandle key) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			return me->VeFixedSizeTable<T>::getIndexFromHandle(key);
		};

		VeHandle getHandleFromIndex(VeIndex table_index) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			return me->VeFixedSizeTable<T>::getHandleFromIndex(table_index);
		};

		uint32_t getAllHandles(std::vector<VeHandle>& result) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			return me->VeFixedSizeTable<T>::getAllHandles(result);
		};

		uint32_t getAllHandlesFromMap(VeIndex num_map, std::vector<VeHandle>& result) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			return me->VeFixedSizeTable<T>::getAllHandlesFromMap(num_map, result);
		};

		template <typename M, typename K, typename I>
		VeIndex leftJoin(VeIndex own_map, VeTable& table, VeIndex other_map, std::vector<std::pair<VeHandle, VeHandle>>& result) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			VeFixedSizeTable<T>* other = (VeFixedSizeTable<T>*)table.m_read_table;

			return me->VeFixedSizeTable<T>::leftJoin<M,K,I>(own_map, *other, other_map, result);
		};

		template <typename K>
		VeHandle getHandleEqual(VeIndex num_map, K key) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			return me->VeFixedSizeTable<T>::getHandleEqual(num_map, key);
		};

		template <typename K>
		uint32_t getHandlesEqual(VeIndex num_map, K key, std::vector<VeHandle>& result) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			return me->VeFixedSizeTable<T>::getHandlesEqual(num_map, key, result);
		};

		template <typename K>
		uint32_t getHandlesRange(VeIndex num_map, K lower, K upper, std::vector<VeHandle>& result) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			return me->VeFixedSizeTable<T>::getHandlesRange(num_map, lower, upper, result);
		};

		void forAllEntries(VeIndex num_map, std::function<void(VeHandle)>& func) {
			VeFixedSizeTable<T>* me = (VeFixedSizeTable<T>*)this->m_read_table;
			me->VeFixedSizeTable<T>::forAllEntries(num_map, func);
		};

		void forAllEntries(VeIndex num_map, std::function<void(VeHandle)>&& func) { 
			forAllEntries(num_map, func); 
		};

		void forAllEntries(std::function<void(VeHandle)>& func) { 
			forAllEntries(VE_NULL_INDEX, func); 
		};

		void forAllEntries(std::function<void(VeHandle)>&& func) { 
			forAllEntries(VE_NULL_INDEX, func); 
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
			std::vector<VeHandle> handles;  
			handles.reserve( (VeIndex)m_directory.getSize() + 1 );
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
					m_directory.deleteEntry(handles[i]);
				}
				else 
					entry1 = entry2;
			}
		}


	public:
		VeVariableSizeTable(VeIndex size = 1<<20, bool clear_on_swap = false, VeIndex align = 16, bool immediateDefrag = false ) :
			VeTable(clear_on_swap), m_align(align), m_immediateDefrag(immediateDefrag) {

			m_directory.addMap(new VeTypedMap< std::multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >(
				(VeIndex)offsetof(struct VeDirectoryEntry, m_occupied), (VeIndex)sizeof(VeDirectoryEntry::m_occupied)));
			m_directory.addMap(new VeTypedMap< std::map<VeTableKeyInt, VeIndex>, VeTableKeyInt, VeTableIndex >(
				(VeIndex)offsetof(struct VeDirectoryEntry, m_start), (VeIndex)sizeof(VeDirectoryEntry::m_start)));
					   
			m_data.resize((VeIndex)size + m_align);
			uint64_t start = (VeIndex) ( alignBoundary((uint64_t)m_data.data(), m_align) - (uint64_t)m_data.data() );
			VeDirectoryEntry entry{ (VeIndex)start, size, 0 };
			m_directory.addEntry(entry);
		}

		VeVariableSizeTable( VeVariableSizeTable& table) :
			VeTable(table), m_directory(table.m_directory), m_data(table.m_data), m_align(table.m_align), 
			m_immediateDefrag(table.m_immediateDefrag) {
		};

		virtual ~VeVariableSizeTable() {};

		void operator=(VeVariableSizeTable & table) {
			m_directory = table.m_directory;
			m_data = table.m_data;
		}

		virtual void setReadOnly(bool ro) { 
			m_read_only = ro; 
			m_directory.setReadOnly(ro);
		};

		virtual void clear() {
			in();
			m_directory.clear();
			uint64_t start = (VeIndex)(alignBoundary((uint64_t)m_data.data(), m_align) - (uint64_t)m_data.data());
			VeDirectoryEntry entry{ (VeIndex)start, (VeIndex)(m_data.size() - m_align), 0 };
			m_directory.addEntry(entry);
			out();
		}

		VeHandle insertBlob( uint8_t* ptr, VeIndex size, VeHandle *pHandle = nullptr, bool defrag = true ) {
			in();
			size = (VeIndex)alignBoundary( size, m_align );

			std::vector<VeHandle> result;
			m_directory.getHandlesEqual((VeIndex)0, (VeIndex)0, result );

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

			if (entry.m_size > size) { 
				VeDirectoryEntry newentry{ entry.m_start + size, entry.m_size - size, 0 };
				m_directory.addEntry(newentry);
			}
			entry.m_size = size;
			entry.m_occupied = 1;
			m_directory.updateEntry(h, entry);
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
			entry.m_occupied = 0;
			m_directory.updateEntry(handle, entry);
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
		VeVariableSizeTableMT(VeIndex size = 1 << 20, bool clear_on_swap = false, VeIndex align = 16, bool immediateDefrag = false) :
			VeVariableSizeTable(size, clear_on_swap, align, immediateDefrag) {};

		VeVariableSizeTableMT(VeVariableSizeTableMT& table) : VeVariableSizeTable(table) {};

		virtual ~VeVariableSizeTableMT() {};

		//---------------------------------------------------------------------------------

		void operator=(VeVariableSizeTableMT& table) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->m_write_table;
			VeVariableSizeTable* other = (VeVariableSizeTable*)table.m_read_table;

			JADDT( me->VeVariableSizeTable::operator=(*other), this->m_thread_id );
		}

		virtual void setReadOnly(bool ro) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->m_write_table;
			JADDT(me->VeVariableSizeTable::setReadOnly(ro), this->m_thread_id);
		};

		virtual void clear() {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->m_write_table;
			JADDT(me->VeVariableSizeTable::clear(), this->m_thread_id);
		}

		VeHandle insertBlob(uint8_t* ptr, VeIndex size, VeHandle* pHandle = nullptr, bool defrag = true) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->m_write_table;
			JADDT(me->VeVariableSizeTable::insertBlob(ptr, size, pHandle, defrag), this->m_thread_id);
		}

		bool deleteBlob(VeHandle handle) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->m_write_table;
			JADDT(me->VeVariableSizeTable::deleteBlob(handle), this->m_thread_id);
		}

		//---------------------------------------------------------------------------------

		uint8_t* getPointer(VeHandle handle) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->m_read_table;
			return me->VeVariableSizeTable::getPointer(handle);
		};

	};


	namespace tab {
		void testTables();
	}


}


