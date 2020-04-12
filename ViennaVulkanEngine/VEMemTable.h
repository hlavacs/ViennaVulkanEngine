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

	/**
	*
	* \brief
	*
	*
	*/
	class VeTable {
	protected:
		VeHeapMemory			m_heap;
		vgjs::VgjsThreadIndex	m_thread_idx = vgjs::VGJS_NULL_THREAD_IDX;	///id of thread that accesses to this table are scheduled to
		bool					m_current_state = false;
		VeTable	*				m_companion_table = nullptr;
		bool					m_clear_on_swap = false;
		std::atomic<bool>		m_swapping = false;
		bool					m_dirty = false;

		//for debugging
		uint32_t	m_table_nr = 0;
		std::string m_name;
		VeClock		m_clock;

		inline void in() {};
		inline void out() {};

	public:
		VeTable(std::string name, bool clear_on_swap = false) : 
			m_name(name), m_heap(), m_clear_on_swap(clear_on_swap), m_clock("table clock", 100) {};

		VeTable(VeTable& table) : m_heap(), m_thread_idx(table.m_thread_idx), 
			m_current_state(!table.m_current_state), m_companion_table(&table),
			m_clear_on_swap(table.m_clear_on_swap), 
			m_name(table.m_name), m_clock(table.m_clock) {
			table.m_companion_table = this;
			m_table_nr = table.m_table_nr + 1;
		};

		virtual ~VeTable() {};
		virtual void operator=(VeTable& tab) { assert(false);  return; };
		virtual void clear() { assert(false);  return; };
		virtual VeMap* getMap(VeIndex num_map) { assert(false);  return nullptr; };
		virtual VeSlotMap* getDirectory() { assert(false);  return nullptr; };

		void setThreadIdx(vgjs::VgjsThreadIndex id) {
			if (m_thread_idx == id)
				return;

			m_thread_idx = id;
			VeTable* companion_table = getCompanionTable();
			if (companion_table != nullptr)
				companion_table->setThreadIdx(id);
		};
		vgjs::VgjsThreadIndex	getThreadIdx() { return m_thread_idx; };

		void setName( std::string name, bool set_companion = true ) {
			m_name = name;
			if (set_companion && getCompanionTable() != nullptr) {
				getCompanionTable()->setName( name, false );
			}
		};
		std::string getName() {
			return m_name;
		}

		virtual void setCurrentState(bool ro) { 
			m_current_state = ro;
		};
		bool isCurrentState() { return m_current_state;  };

		VeTable* getCurrentStatePtr() { 
			if (m_companion_table == nullptr)
				return this;

			VeTable* table = this;
			if (!m_current_state) {
				table = m_companion_table;
			}
			assert(table->getReadOnly());
			return table;
		};

		VeTable* getNextStatePtr() { 
			if (m_companion_table == nullptr)
				return this;

			VeTable* table = this;
			if (m_current_state) {
				table = m_companion_table;
			}
			assert(!table->getReadOnly());
			return table;

		};

		virtual void swapTables() {
			in();

			//std::cout << "table " << m_name << " old read " << getReadTablePtr()->m_table_nr << " old write " << getWriteTablePtr()->m_table_nr << std::endl;

			if (m_companion_table == nullptr) {
				out();
				return;
			}

			m_swapping = true;
			m_companion_table->m_swapping = true;

			setCurrentState(!isCurrentState());
			m_companion_table->setCurrentState(!m_companion_table->isCurrentState());

			//std::cout << "table " << m_name << " new read " << getReadTablePtr()->m_table_nr << " new write " << getWriteTablePtr()->m_table_nr << std::endl;

			if (m_clear_on_swap) {
				getNextStatePtr()->clear();
			}
			else if(getCurrentStatePtr()->m_dirty ) {
				*getNextStatePtr() = *getCurrentStatePtr();
			}
			m_dirty = false;

			m_swapping = false;
			m_companion_table->m_swapping = false;

			out();
		};

		VeTable* getCompanionTable() {
			return m_companion_table;
		}
	};



	namespace tab {
		void testTables();
	};


};


#include "VEMemTableFixed.h"
#include "VEMemTableVar.h"

