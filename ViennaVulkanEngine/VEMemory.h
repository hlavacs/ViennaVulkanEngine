#pragma once



namespace mem {

	const uint32_t VE_MEM_IDX_NULL = std::numeric_limits<uint32_t>::max();
	const uint32_t VE_MEM_DEFAULT_INDEX_SIZE = 512;

	struct TableListIndex {
		uint32_t	m_next = VE_MEM_IDX_NULL;
		uint32_t	m_prev = VE_MEM_IDX_NULL;
	};

	template <typename T>
	struct TableEntry {
		uint32_t	m_id;
		T			m_entry;
		uint32_t	m_next = VE_MEM_IDX_NULL;
		uint32_t	m_prev = VE_MEM_IDX_NULL;

		TableEntry(uint32_t id, T& entry) {
			m_id = id;
			m_entry = entry;
		};
	};

	template <typename T>
	struct FixedTable {
		std::vector<uint32_t>		m_index;
		std::vector<TableEntry<T>>	m_data;

		void addEntry( VEHANDLE handle, T& entry) {
			uint32_t new_idx = m_data.size();

			TableEntry te( handle.m_id, entry );

			uint32_t idx = handle.m_hash % m_index.size();
			uint32_t entry_idx = m_index[idx];

			if (entry_idx != VE_MEM_IDX_NULL) {
				te.m_next = entry_idx;
				m_data[entry_idx].m_prev = new_idx;
			}
			m_index[idx] = new_idx;
			m_data.emplace_back( te );
		};

		T& getEntry( uint32_t id, uint32_t hash ) {

		};

		void deleteEntry( uint32_t key ) {

		};
	};

	struct VariableTable {
		TableListIndex			m_indices;
		std::vector<uint8_t>	m_data;
	};


}


