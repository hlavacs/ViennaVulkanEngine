

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


	void tick() {
		VeFixedSizeTableMT<VeEventTableEntry>*					events_table = g_events_table.getTablePtrRead();
		VeFixedSizeTableMT<VeEventRegisteredHandlerTableEntry>* handler_table = g_event_handler_table.getTablePtrRead();

		std::vector<std::pair<VeHandle,VeHandle>> handle_list;
		events_table->leftJoin<std::multimap<VeHandle, VeIndex>, VeHandle, VeIndex>(0, handler_table, 0, handle_list);

		for (auto [first, second] : handle_list) {
			VeEventTableEntry event;
			events_table->getEntry(first, event);
			VeEventRegisteredHandlerTableEntry handler;
			handler_table->getEntry(second, handler);
			JADD( handler.m_handler(event));
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

