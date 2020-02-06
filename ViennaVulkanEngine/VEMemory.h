#pragma once



namespace mem {

	struct TableFindIndex {
		std::unordered_multimap<uint32_t, uint32_t> m_index;		///key value pairs - value is the index of the entry in the table
	};

	struct TableSortIndex {
		std::multimap<uint32_t, uint32_t> m_index;		///key value pairs, key is sorted, value is the entry idx in the table
	};

	enum TableTypes {

	};

	template <typename T>
	struct FixedSizeTable {

		struct TableInfo {
			VeHandle	m_handle;		///table handle
			uint32_t	m_thread_id;	///id of thread that accesses to this table are scheduled to
			TableTypes	m_table_type;	///type of the table
		};

		struct IndexEntry {
			uint32_t m_num_index = VE_NULL;	///number of index to put the key into
			uint32_t m_key = VE_NULL;		///key for the index, the value is the directory entry index
		};

		struct DirectoryEntry {
			uint32_t m_table_index = VE_NULL;		///index into the entry table
			uint32_t m_next_free = VE_NULL;			///index of next free entry in directory
			std::vector<IndexEntry> m_find_index;	///remember the search keys that were used for this entry
			std::vector<IndexEntry> m_sort_index;
		};

		TableInfo					m_table_info;			///information about the table, must be the first variable
		std::vector<TableFindIndex>	m_find_index;			///vector of hashed indices for quickly finding entries in O(1)
		std::vector<TableSortIndex>	m_sort_index;			///vector of sorted keys for sorted iterating through entries
		std::vector<DirectoryEntry>	m_directory;			///1 level of indirection, idx into the entry table
		uint32_t					m_first_free = VE_NULL;	///index of first free entry in directory
		std::vector<T>				m_data;					///growable entry data table

#ifndef VE_PUBLIC_INTERFACE

		FixedSizeTable(uint32_t num_find, uint32_t num_sort ) {
			if( num_find > 0 ) m_find_index.resize(num_find);
			if( num_sort > 0 ) m_sort_index.resize(num_sort);
		};

		void addToFindIndex( uint32_t dir_index, std::vector<IndexEntry>&& find_index ) {
			for each (auto entry in find_index) {
				m_find_index[entry.m_num_index].emplace( entry.m_key, dir_index );
			}
		};

		void removeFromFindIndex(uint32_t dir_index, std::vector<IndexEntry>& find_index) {
			for each (auto entry in find_index) {
				auto itpair = m_find_index[entry.m_num_index].equal_range(entry.m_key);
				for (auto it = itpair->first; it != itpair->second; ++it) {
					if( it->second == dir_index ) 
						m_find_index[entry.m_num_index].erase(entry.m_key);				}
			}
		}

		void addToSortIndex(uint32_t dir_index, std::vector<IndexEntry>& sort_index ) {
			for each (auto entry in sort_index) {
				m_sort_index[entry.m_num_index].emplace(entry.m_key, dir_index);
			}
		};

		void removeFromSortIndex(uint32_t dir_index, std::vector<IndexEntry>&& sort_index) {
			for each (auto entry in sort_index) {
				auto itpair = m_sort_index[entry.m_num_index].equal_range(entry.m_key);
				for (auto it = itpair->first; it != itpair->second; ++it) {
					if (it->second == dir_index)
						m_sort_index[entry.m_num_index].erase(entry.m_key);
				}
			}
		}

		void addToDirectory( uint32_t table_idx, std::vector<IndexEntry>&& find_index, std::vector<IndexEntry>&& sort_index) {
			uint32_t dir_idx = m_first_free;
			if (dir_idx != VE_NULL) {
				m_first_free = m_directory[dir_idx].m_next_free;
				m_directory[dir_idx].m_table_index = table_idx;
				m_directory[dir_idx].m_find_index = std::forward(find_index);
				m_directory[dir_idx].m_sort_index = std::forward(sort_index);
			}
			else {
				dir_idx = m_directory.size();
				m_directory.emplace_back(table_idx, std::forward(find_index), std::forward(sort_index));
			}
			addToFindIndex(dir_idx, m_directory[dir_idx].find_index);
			addToSortIndex(dir_idx, m_directory[dir_idx].sort_index);
		}

		void removeFromDirectory( uint32_t dir_idx ) {
			m_directory[dir_idx].m_next_free = m_first_free;
			m_first_free = dir_idx;


		}

#endif

		//------------------------------------------------------------------------------------------
		//public interface

		void addEntry(T& te, std::vector<IndexEntry>&& find_index, std::vector<IndexEntry>&& sort_index) {
			uint32_t table_idx = m_data.size();
			m_data.emplace_back(te);
			addToDirectory(table_idx, std::forward(find_index), std::forward(sort_index));
		};

		uint32_t findEntry(uint32_t index_nr, uint32_t key, T& entry) {
			auto& map = m_find_index[index_nr].m_index;
			auto result = map.find(key);
			if (result == map.end()) return VE_NULL;
			entry = m_data[result->second()];
			return result->second();
		}

		uint32_t findEntries(uint32_t index_nr, uint32_t key, std::vector<T>& result) {
			auto ret = m_find_index[index_nr].m_index.equal_range(key);		//get pair of iterators begin, end
			uint32_t num_result = 0;
			for (auto it = ret.first; it != ret.second; ++it, ++num_result) {
				result.emplace_back(m_data[it->second()]);
			}
			return num_result;
		};

		uint32_t getSortedEntries(uint32_t key, uint32_t index_nr, std::vector<T>& result) {
			auto map = m_sort_index[index_nr].m_index;
			uint32_t num_indices = 0;
			for (auto iter = map.begin(); iter != map.end(); ++iter, ++num_indices) 
				result.emplace_back(m_data[iter->second]);
			return num_indices;
		}

		bool removeEntry(uint32_t key, uint32_t index_nr) {
			auto& map = m_find_index[index_nr].m_index;
			auto result = map.find(key);
			if (result == map.end()) return false;
			removeFromDirectory( result->second );
			return true;
		}

		uint32_t removeEntries(uint32_t key, uint32_t index_nr) {
			bool result;
			uint32_t num_removed = 0;
			do {
				if ( result = removeEntry(key, index_nr) ) ++num_removed;
			} while ( result );
			return num_removed;
		}
	};



	///-------------------------------------------------------------------------------
	struct VariableSizeTable {
		TableSortIndex			m_indices;
		std::vector<uint8_t>	m_data;
	};


}


