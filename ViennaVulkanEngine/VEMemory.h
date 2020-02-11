#pragma once



namespace mem {

	//------------------------------------------------------------------------------------------------------

	class VeMap {
	public:
		VeMap() {};
		virtual ~VeMap() {};
		virtual bool		getMappedIndex(			VeHandle key, VeIndex& index) { return false; };
		virtual uint32_t	getMappedIndices(		VeHandle key, std::vector<VeIndex>& result) { return 0; };
		virtual bool		getMappedPairIndex(		std::pair<VeHandle, VeHandle> &key, VeIndex& index) { return false; };
		virtual uint32_t	getMappedPairIndices(	std::pair<VeHandle, VeHandle> &key, std::vector<VeIndex>& result) { return 0; };
		virtual bool		getMappedStringIndex(	const std::string& key, VeIndex& index) { return false; };
		virtual uint32_t	getMappedStringIndices( const std::string& key, std::vector<VeIndex>& result) { return 0; };

		virtual void		mapHandleToIndex(		VeHandle key, VeIndex value) {};
		virtual void		mapHandlePairToIndex(	std::pair<VeHandle, VeHandle> &key, VeIndex value) {};
		virtual void		mapStringToIndex(		const std::string& key, VeIndex value) {};
	};


	template <typename M>
	class VeTypedMap : public VeMap {
	protected:
		VeIndex		m_offset;			///
		std::size_t	m_num_bytes;		///
		M			m_map;				///key value pairs - value is the index of the entry in the table

	public:
		VeTypedMap(	VeIndex offset			= VE_NULL_INDEX,
					std::size_t num_bytes	= sizeof(VeHandle)) : VeMap(), m_offset(offset), m_num_bytes(num_bytes) {};
		virtual ~VeTypedMap() {};

		virtual bool getMappedIndex( VeHandle key, VeIndex &index ) override {
			auto search = m_map.find(key);
			if (search == m_map.end()) return false;
			index = search->second;
			return true;
		}

		virtual uint32_t getMappedIndices(VeHandle key, std::vector<VeIndex>& result) override {
			uint32_t num = 0;
			auto range = m_map.equal_range( key );
			for (auto it = range.first; it != range.second; ++it) result.push_back( it->second );
			return num;
		};

		virtual void mapHandleToIndex(VeHandle key, VeIndex index) override {
			m_map.try_emplace( key, index );
		};
	};


	template <typename N>
	class VeTypedPairMap : public VeMap {
	protected:
		std::pair<VeIndex, VeIndex>			m_offset;
		std::pair<std::size_t, std::size_t> m_num_bytes;
		N									m_map;		///key value pairs - value is the index of the entry in the table

	public:
		VeTypedPairMap(	std::pair<VeIndex, VeIndex> offset				= { VE_NULL_INDEX, VE_NULL_INDEX },
						std::pair<std::size_t, std::size_t> num_bytes	= {sizeof(VeHandle), sizeof(VeHandle)} ) : 
							VeMap(), m_offset(offset), m_num_bytes(num_bytes) {};

		virtual ~VeTypedPairMap() {};

		virtual bool getMappedPairIndex( std::pair<VeHandle, VeHandle> &key, VeIndex& index) override {
			auto search = m_map.find(key);
			if (search == m_map.end()) return false;
			index = search->second;
			return true;
		}

		virtual uint32_t getMappedPairIndices( std::pair<VeHandle, VeHandle> &key, std::vector<VeIndex>& result) override {
			uint32_t num = 0;
			auto range = m_map.equal_range(key);
			for (auto it = range.first; it != range.second; ++it) result.push_back( it->second );
			return num;
		};

		virtual void mapHandlePairToIndex( std::pair<VeHandle, VeHandle> key, VeIndex index) override {
			m_map.try_emplace(key, index);
		};
	};


	template <typename S>
	class VeTypedStringMap : public VeMap {
	protected:
		VeIndex		m_offset;			///
		S			m_map;				///key value pairs - value is the index of the entry in the table

	public:
		VeTypedStringMap(VeIndex offset = VE_NULL_INDEX ) : VeMap(), m_offset(offset) {};
		virtual ~VeTypedStringMap() {};

		virtual bool getMappedStringIndex( const std::string& key, VeIndex& index) override {
			auto search = m_map.find(key);
			if (search == m_map.end()) return false;
			index = search->second;
			return true;
		}

		virtual uint32_t getMappedStringIndices(const std::string& key, std::vector<VeIndex>& result) override {
			uint32_t num = 0;
			auto range = m_map.equal_range(key);
			for (auto it = range.first; it != range.second; ++it) result.push_back(it->second);
			return num;
		};

		virtual void mapStringToIndex( const std::string& key, VeIndex index) override {
			m_map.try_emplace(key, index);
		};
	};



	//------------------------------------------------------------------------------------------------------

	class VeDirectory {
	protected:

		struct VeDirectoryEntry {
			VeIndex m_table_index = VE_NULL_INDEX;	///index into the entry table
			VeIndex m_next_free = VE_NULL_INDEX;	///index of next free entry in directory
		};

		VeHandle						m_auto_counter = VE_NULL_HANDLE;	///
		std::vector<VeHandle>			m_auto_ids;							///
		std::vector<VeDirectoryEntry>	m_dir_entries;						///1 level of indirection, idx into the entry table
		VeIndex							m_first_free = VE_NULL_INDEX;		///index of first free entry in directory

	public:
		VeDirectory() {};
		~VeDirectory() {};
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
		std::vector<VeMap*>	m_maps;				///vector of maps for quickly finding or sorting entries
		VeDirectory			m_directory;		///
		std::vector<T>		m_data;				///growable entry data table

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

		void addEntry(T& te) {
		};

		bool getEntry(VeIndex num_map, VeHandle key, T& entry) {
		};
		bool getEntry(VeIndex num_map, std::pair<VeHandle,VeHandle> &key, T& entry) {
		};
		bool getEntry(VeIndex num_map, std::string& key, T& entry) {
		};

		uint32_t getEntries(VeIndex num_map, VeHandle key, std::vector<T>& result) {
		};
		uint32_t getEntries(VeIndex num_map, std::pair<VeHandle, VeHandle>& key, std::vector<T>& result) {
		};
		uint32_t getEntries(VeIndex num_map, std::string& key, std::vector<T>& result) {
		};

		uint32_t getSortedEntries(VeIndex num_map, std::vector<T>& result) {
		};

		uint32_t deleteEntries( VeIndex num_map, VeHandle key ) {
		};
		uint32_t deleteEntries(VeIndex num_map, std::pair<VeHandle, VeHandle>& key) {
		};
		uint32_t deleteEntries(VeIndex num_map, std::string& key) {
		};
	};



	///-------------------------------------------------------------------------------
	struct VariableSizeTable {
		//TableSortIndex		m_indices;
		std::vector<uint8_t>	m_data;
	};


}


