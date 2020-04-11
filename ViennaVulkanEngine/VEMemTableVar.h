#pragma once



namespace vve {



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
			std::vector<VeHandle, custom_alloc<VeHandle>> handles(&m_heap);
			handles.reserve((VeIndex)((uint64_t)m_directory.size() + 1));
			m_directory.getAllHandlesFromMap(1, handles);
			if (handles.size() < 2)
				return;

			VeDirectoryEntry entry1;
			if (!m_directory.getEntry(handles[0], entry1))
				return;

			for (uint32_t i = 1; i < handles.size(); ++i) {
				VeDirectoryEntry entry2;
				if (!m_directory.getEntry(handles[i], entry2))
					return;

				if (entry1.m_occupied == 0 && entry2.m_occupied == 0) {
					entry1.m_size += entry2.m_size;
					m_directory.erase(handles[i]);
				}
				else
					entry1 = entry2;
			}
		}

	public:
		VeVariableSizeTable(std::string name, VeIndex size = 1 << 20, bool clear_on_swap = false, VeIndex align = 16, bool immediateDefrag = false) :
			VeTable(name, clear_on_swap), m_directory(name), m_align(align), m_immediateDefrag(immediateDefrag) {

			m_directory.addMap(new VeOrderedMultimap< VeKey, VeIndex >(
				(VeIndex)offsetof(struct VeDirectoryEntry, m_occupied), (VeIndex)sizeof(VeDirectoryEntry::m_occupied)));

			m_directory.addMap(new VeOrderedMultimap< VeKey, VeIndex >(
				(VeIndex)offsetof(struct VeDirectoryEntry, m_start), (VeIndex)sizeof(VeDirectoryEntry::m_start)));

			m_data.resize((VeIndex)size + (VeIndex)m_align);
			uint64_t start = (VeIndex)(alignBoundary((uint64_t)m_data.data(), m_align) - (uint64_t)m_data.data());
			VeDirectoryEntry entry{ (VeIndex)start, size, 0 };
			m_directory.insert(entry, nullptr);
		}

		VeVariableSizeTable(VeVariableSizeTable& table) :
			VeTable(table), m_directory(table.m_directory), m_data(table.m_data), m_align(table.m_align),
			m_immediateDefrag(table.m_immediateDefrag) {
		};

		virtual ~VeVariableSizeTable() {};

		void operator=(VeTable& table) {
			VeVariableSizeTable* other = (VeVariableSizeTable*)&table;
			m_directory = other->m_directory;
			m_data = other->m_data;
		}

		void operator=(VeVariableSizeTable& table) {
			m_directory = table.m_directory;
			m_data = table.m_data;
		}

		virtual void setReadOnly(bool ro) {
			m_read_only = ro;
			m_directory.setReadOnly(ro);
		};

		virtual void swapTables() {
			if (m_companion_table == nullptr)
				return;

			VeVariableSizeTable::setReadOnly(!getReadOnly());
			((VeVariableSizeTable*)m_companion_table)->VeVariableSizeTable::setReadOnly(!m_companion_table->getReadOnly());

			if (m_clear_on_swap) {
				((VeVariableSizeTable*)getWriteTablePtr())->clear();
			}
			else {
				auto pWrite = (VeVariableSizeTable*)getWriteTablePtr();
				auto pRead = (VeVariableSizeTable*)getReadTablePtr();

				if (pRead->m_dirty) {
					if (pWrite->m_data.size() != pRead->m_data.size()) {
						pWrite->m_data.resize(pRead->m_data.size());
					}
					memcpy(pWrite->m_data.data(), pRead->m_data.data(), pRead->m_data.size());
					*(m_directory.getWriteTablePtr()) = *(m_directory.getReadTablePtr());
				}
				m_dirty = false;
			}
		};

		virtual void clear() {
			in();
			m_dirty = true;
			m_directory.clear();
			uint64_t start = (VeIndex)(alignBoundary((uint64_t)m_data.data(), m_align) - (uint64_t)m_data.data());
			VeDirectoryEntry entry{ (VeIndex)start, (VeIndex)(m_data.size() - m_align), 0 };
			m_directory.insert(entry, nullptr);
			out();
		}

		VeHandle insertBlob(uint8_t* ptr, VeIndex size, VeHandle* pHandle = nullptr, bool defrag = true) {
			in();
			size = (VeIndex)alignBoundary(size, m_align);

			std::vector<VeHandle, custom_alloc<VeHandle>> result(&m_heap);
			m_directory.getHandlesEqual(0_Ke, 0, result); //map 0, all where occupied == false

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

			m_dirty = true;
			if (entry.m_size > size) {
				VeDirectoryEntry newentry{ entry.m_start + size, entry.m_size - size, 0 };
				m_directory.insert(newentry, nullptr);
			}
			entry.m_size = size;
			entry.m_occupied = 1;
			m_directory.update(h, entry);
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

		bool deleteBlob(VeHandle handle) {
			in();
			VeDirectoryEntry entry;
			if (!m_directory.getEntry(handle, entry)) {
				out();
				return false;
			}
			m_dirty = true;
			entry.m_occupied = 0;
			m_directory.update(handle, entry);
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
		VeVariableSizeTableMT(std::string name, VeIndex size = 1 << 20, bool clear_on_swap = false, VeIndex align = 16, bool immediateDefrag = false) :
			VeVariableSizeTable(name, size, clear_on_swap, align, immediateDefrag) {};

		VeVariableSizeTableMT(VeVariableSizeTableMT& table) : VeVariableSizeTable(table) {};

		virtual ~VeVariableSizeTableMT() {};

		//---------------------------------------------------------------------------------

		void operator=(VeTable& table) {
			VeVariableSizeTable* other_tab = (VeVariableSizeTable*)&table;

			VeVariableSizeTable* me = (VeVariableSizeTable*)this->getWriteTablePtr();
			VeVariableSizeTable* other = (VeVariableSizeTable*)other_tab->getReadTablePtr();

			JADDT(me->VeVariableSizeTable::operator=(*other), vgjs::TID(this->m_thread_idx));
		}

		void operator=(VeVariableSizeTableMT& table) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->getWriteTablePtr();
			VeVariableSizeTable* other = (VeVariableSizeTable*)table.getReadTablePtr();

			JADDT(me->VeVariableSizeTable::operator=(*other), vgjs::TID(this->m_thread_idx));
		}

		virtual void setReadOnly(bool ro) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this;
			JADDT(me->VeVariableSizeTable::setReadOnly(ro), vgjs::TID(this->m_thread_idx));
		};

		virtual void clear() {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->getWriteTablePtr();
			JADDT(me->VeVariableSizeTable::clear(), vgjs::TID(this->m_thread_idx));
		}

		VeHandle insertBlob(uint8_t* ptr, VeIndex size, VeHandle* pHandle = nullptr, bool defrag = true) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->getWriteTablePtr();
			JADDT(me->VeVariableSizeTable::insertBlob(ptr, size, pHandle, defrag), vgjs::TID(this->m_thread_idx));
		}

		bool deleteBlob(VeHandle handle) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->getWriteTablePtr();
			JADDT(me->VeVariableSizeTable::deleteBlob(handle), vgjs::TID(this->m_thread_idx));
		}

		//---------------------------------------------------------------------------------

		uint8_t* getPointer(VeHandle handle) {
			VeVariableSizeTable* me = (VeVariableSizeTable*)this->getReadTablePtr();
			return me->VeVariableSizeTable::getPointer(handle);
		};

	};


};




