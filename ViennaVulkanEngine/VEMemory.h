#pragma once



namespace mem {

	const uint32_t VE_MEM_IDX_NULL = std::numeric_limits<uint32_t>::max();
	const uint32_t VE_MEM_DEFAULT_INDEX_SIZE = 512;

	struct TableFindIndex {
		uint32_t									m_key_offset;	///offset in the entry struct that contains the 32 bit key
		std::unordered_multimap<uint32_t, uint32_t> m_index;		///key value pairs

		TableFindIndex(uint32_t key_offset) {
			m_key_offset = key_offset;
		}
	};

	struct TableSortIndex {
		uint32_t									m_key_offset;	///offset in the entry struct that contains the 32 bit key
		std::map<uint32_t, std::vector<uint32_t>>	m_index;		///key is the id, value is a list of indices

		TableSortIndex( uint32_t key_offset ) {
			m_key_offset = key_offset;
		}
	};

	template <typename T>
	struct FixedSizeTable {
		uint32_t					m_thread_id;			///id of thread that accesses to this table are scheduled to
		std::vector<TableFindIndex>	m_find_index;			///vector of hashed indices for quickly finding entries
		std::vector<TableSortIndex>	m_sort_index;			///vector of sorted indices for sorted iterating through entries
		std::vector<T>				m_data;					///growable data table

		FixedSizeTable( std::vector<uint32_t>& find_key_offsets, std::vector<uint32_t>& sort_key_offsets ) {
			for each (uint32_t offset in find_key_offsets) {
				TableFindIndex tfi(offset);
				m_find_index.emplace_back(tfi); 
			};
			for each (uint32_t offset in sort_key_offsets) { 
				TableSortIndex tsi(offset);
				m_sort_index.emplace_back(tsi);
			};
		}

		void addFindIndex(T& te, uint32_t idx) {

		}

		void addSortIndex( T& te, uint32_t idx) {

		}

		void addEntry(T& te) {
			uint32_t idx = m_data.size();
			m_data.emplace_back(te);
			addFindIndex(te, idx);
			addSortIndex(te, idx);
		};

		T& getEntry(uint32_t id, uint32_t hash) {

		};

		void deleteEntry(uint32_t key) {

		};
	};



	///-------------------------------------------------------------------------------
	struct VariableSizeTable {
		TableSortIndex			m_indices;
		std::vector<uint8_t>	m_data;
	};


}


