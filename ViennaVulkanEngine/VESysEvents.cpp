

#include "VEDefines.h"
#include "VESysEngine.h"
#include "VESysEvents.h"


namespace vve::syseve {


	std::vector<VeMap*> maps1 = {
		new VeTypedMap< std::multimap<VeHandle, VeIndex>, VeHandle, VeIndex >(offsetof(struct VeEventTableEntry, m_type), sizeof(VeEventTableEntry::m_type))
	};
	VeFixedSizeTableMT<VeEventTableEntry> g_events_table(maps1);
	VeFixedSizeTableMT<VeEventTableEntry> g_events_table2(g_events_table);

	std::vector<VeMap*> maps2 = {
		new VeTypedMap< std::unordered_multimap<VeHandle, VeIndex>, VeHandle, VeIndex >(offsetof(struct VeEventRegisteredHandlerTableEntry, m_type), sizeof(VeEventRegisteredHandlerTableEntry::m_type))
	};
	VeFixedSizeTableMT<VeEventRegisteredHandlerTableEntry> g_event_handler_table(maps2);
	VeFixedSizeTableMT<VeEventRegisteredHandlerTableEntry> g_event_handler_table2(g_event_handler_table);


	void init() {
		syseng::registerTablePointer(&g_events_table, "Events Table");
		syseng::registerTablePointer(&g_event_handler_table, "Event Handler Table");
	}


	void handleEvents( VeEventType type ) {

	}

	void tick() {
		VeFixedSizeTableMT<VeEventTableEntry>*					events_table = g_events_table.getTablePtrRead();
		VeFixedSizeTableMT<VeEventRegisteredHandlerTableEntry>* handler_table = g_event_handler_table.getTablePtrRead();

		std::vector<VeHandle> event_list;
		event_list.resize(events_table->getData().size());
		events_table->getAllHandlesFromMap(0, event_list);

		std::vector<VeHandle> handler_list;

		VeEventType type = syseve::VeEventType::VE_EVENT_TYPE_NULL;
		for (auto event_handle : event_list) {
			VeEventTableEntry ev;
			events_table->getEntry(event_handle, ev);
			if (ev.m_type != type) {
				handler_table->getHandlesEqual(0, type, handler_list);
			}
			for (auto handler_handle : handler_list) {
				VeEventRegisteredHandlerTableEntry handler;
				handler_table->getEntry(handler_handle, handler);
				handler.m_handler(ev);
			}
		}

	}

	void close() {

	}

	void addEvent(VeEventTableEntry event) {
		g_events_table.getTablePtrWrite()->addEntry( event );
	}

	void addHandler(VeEventType type, std::function<void( VeEventTableEntry)> handler) {
		g_event_handler_table.getTablePtrWrite()->addEntry({ type, handler });
	}


}

