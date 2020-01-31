#pragma once



namespace mem {

	const uint32_t VE_MEM_IDX_NULL = std::numeric_limits<uint32_t>::max();

	struct TableHashIndex {
		std::vector<uint32_t> m_indices;
	};

	struct TableListIndex {
		uint32_t	m_next;
		uint32_t	m_prev;
	};

	template <typename T>
	struct TableEntry {
		T			m_entry;
		uint32_t	m_next;
		uint32_t	m_prev;
	};

	template <typename T>
	struct FixedTable {
		TableHashIndex				m_indices;
		std::vector<TableEntry<T>>	m_data;

		void addEntry(T& entry) {

		};

		T& getEntry(VEHANDLE handle) {

		};

		void deleteEntry(VEHANDLE handle) {

		};
	};

	struct VariableTable {
		TableListIndex			m_indices;
		std::vector<uint8_t>	m_data;
	};


}


