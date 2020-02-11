#pragma once



namespace mem {

	//------------------------------------------------------------------------------------------------------

	class VeMap {
	protected:
		VeIndex		m_offset = VE_NULL_INDEX;			///
		std::size_t	m_num_bytes = sizeof(VeHandle);		///

	public:
		VeMap(VeIndex offset, uint32_t num_bytes) : m_offset(offset), m_num_bytes(num_bytes) {};
		virtual ~VeMap() {};
		virtual bool getMappedIndex( VeHandle key, VeIndex &index ) = 0;
		virtual uint32_t getMappedIndex(VeHandle key, std::vector<VeIndex>& result) = 0;
		virtual void mapHandleToIndex(VeHandle key, VeIndex value) = 0;
	};


	template <typename M>
	class VeTypedMap : public VeMap {
	protected:
		M m_map;		///key value pairs - value is the index of the entry in the table

	public:
		VeTypedMap(VeIndex offset, std::size_t num_bytes = sizeof(VeHandle) ) : VeMap( offset, num_bytes ) {};
		virtual ~VeTypedMap() {};

		virtual bool getMappedIndex( VeHandle key, VeIndex &index ) override {
			auto search = m_map.find(key);
			if (search == m_map.end()) return false;
			index = search->second;
			return true;
		}

		virtual uint32_t getMappedIndex(VeHandle key, std::vector<VeIndex>& result) override {
			uint32_t num = 0;
			auto range = m_map.equal_range( key );
			for (auto it = range.first; it != range.second; ++it) {
				result.emplace_back(*it);
			}
			return num;
		};

		virtual void mapHandleToIndex(VeHandle key, VeIndex index) override {
			m_map.try_emplace( key, index );
		};

		virtual void mapPairToIndex( std::pair<VeHandle,VeHandle> &pair, VeIndex index ) {
			m_map.try_emplace(pair, index);
		}

		virtual void mapTripleToIndex(std::tuple<VeHandle, VeHandle, VeHandle> &triple, VeIndex index) {
			m_map.try_emplace(triple, index);
		}

	};



	template <typename N>
	class VeTypedPairMap : public VeMap {
	protected:
		N m_map;		///key value pairs - value is the index of the entry in the table

	public:
		VeTypedPairMap(std::pair<VeIndex,VeIndex> offset, std::size_t num_bytes = sizeof(VeHandle)) : VeMap(offset, num_bytes) {};
		virtual ~VeTypedPairMap() {};

		virtual bool getMappedIndex(VeHandle key, VeIndex& index) override {
			auto search = m_map.find(key);
			if (search == m_map.end()) return false;
			index = search->second;
			return true;
		}

		virtual uint32_t getMappedIndex(VeHandle key, std::vector<VeIndex>& result) override {
			uint32_t num = 0;
			auto range = m_map.equal_range(key);
			for (auto it = range.first; it != range.second; ++it) {
				result.emplace_back(*it);
			}
			return num;
		};

		virtual void mapHandleToIndex(VeHandle key, VeIndex index) override {
			m_map.try_emplace(key, index);
		};

		virtual void mapPairToIndex(std::pair<VeHandle, VeHandle>& pair, VeIndex index) {
			m_map.try_emplace(pair, index);
		}

		virtual void mapTripleToIndex(std::tuple<VeHandle, VeHandle, VeHandle>& triple, VeIndex index) {
			m_map.try_emplace(triple, index);
		}

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
		void clearAutoCounter() { m_auto_counter = 0; };
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
		using MapPtr = std::unique_ptr<VeMap>;
		std::vector<MapPtr>	m_maps;				///vector of hashed indices for quickly finding entries in O(1)
		VeDirectory			m_directory;		///
		std::vector<T>		m_table_entries;	///growable entry data table

	public:

		VeFixedSizeTypedTable( std::vector<MapPtr> &&maps, VeIndex thread_id = VE_NULL_INDEX) :
			VeFixedSizeTable( thread_id ) {

			if (maps.size() > 0) m_maps = std::move(maps);
			else {
				m_maps.emplace_back( (VeMap*) new std::unordered_map<VeHandle, VeIndex >(VE_NULL_INDEX) );
				m_directory.clearAutoCounter();
			}
		
		};
		~VeFixedSizeTypedTable() {};

		void addEntry(T& te) {
		};

		bool getEntry(VeIndex num_map, VeHandle key, T& entry) {
		};

		uint32_t getEntries(VeIndex num_map, VeHandle key, std::vector<T>& result) {
		};

		uint32_t getSortedEntries(VeIndex num_map, std::vector<T>& result) {
		};

		bool deleteEntry(VeIndex num_map, VeHandle key ) {
		};

		uint32_t deleteEntries( VeIndex num_map, VeHandle key ) {
		};
	};



	///-------------------------------------------------------------------------------
	struct VariableSizeTable {
		//TableSortIndex		m_indices;
		std::vector<uint8_t>	m_data;
	};


}


