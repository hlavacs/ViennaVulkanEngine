#pragma once



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

		VeHandle addNewEntry(VeIndex table_index) {
			VeIndex guid = (VeIndex)m_auto_counter;
			++m_auto_counter;
			VeIndex dir_index = (VeIndex)m_dir_entries.size();
			m_dir_entries.emplace_back(guid, table_index, VE_NULL_INDEX);
			return getHandle(dir_index);
		}

		VeHandle writeOverOldEntry(VeIndex table_index) {
			VeCount guid = m_auto_counter;
			++m_auto_counter;
			VeIndex dir_index = m_first_free;
			VeIndex next_free = m_dir_entries[dir_index].m_next_free;
			m_dir_entries[dir_index] = { (VeIndex)guid, table_index, VE_NULL_INDEX };
			m_first_free = next_free;
			return getHandle(dir_index);
		}

	public:
		VeDirectory() {};
		~VeDirectory() {};

		void operator=(const VeDirectory& dir) {
			m_auto_counter = dir.m_auto_counter;
			m_dir_entries = dir.m_dir_entries;
			m_first_free = dir.m_first_free;
		}

		void clear() {
			m_dir_entries.clear();
			m_first_free = VE_NULL_INDEX;
		}

		VeHandle addEntry(VeIndex table_index) {
			if (m_first_free == VE_NULL_INDEX)
				return addNewEntry(table_index);
			return writeOverOldEntry(table_index);
		};

		VeDirectoryEntry& getEntry(VeIndex dir_index) {
			assert(isValid(dir_index));
			return m_dir_entries[dir_index];
		};

		VeHandle getHandle(VeIndex dir_index) {
			if (!isValid(dir_index))
				return VE_NULL_HANDLE;
			return VeHandle(m_dir_entries[dir_index].m_guid | ((uint64_t)dir_index << 32));
		};

		static::std::tuple<VeIndex, VeIndex> splitHandle(VeHandle key) {
			return { VeIndex(key) & 0xFFFFFFFF , (VeIndex)((uint64_t)key >> 32) };
		};

		bool isValid(VeIndex dir_index) {
			if (dir_index >= m_dir_entries.size() || m_dir_entries[dir_index].m_next_free != VE_NULL_INDEX)
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

		void deleteEntry(VeIndex dir_index) {
			assert(isValid(dir_index));
			if (m_first_free != VE_NULL_INDEX)
				m_dir_entries[dir_index].m_next_free = m_first_free;
			m_first_free = dir_index;
		}
	};



};




