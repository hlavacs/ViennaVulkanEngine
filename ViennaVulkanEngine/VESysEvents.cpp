

#include "VEDefines.h"
#include "VESysEngine.h"
#include "VESysEvents.h"


namespace vve::syseve {


	std::vector<VeMap*> maps1 = {
		(VeMap*) new VeTypedMap< std::unordered_map<std::string, VeIndex>, std::string, VeIndex >(offsetof(struct VeEventTypeTableEntry, m_name), 0)
	};
	VeFixedSizeTable<VeEventTypeTableEntry> g_event_types_table(maps1);

	std::vector<VeMap*> maps2 = {
		(VeMap*) new VeTypedMap< std::unordered_map<VeHandle, VeIndex>,	VeHandle, VeIndex >(offsetof(struct VeEventTableEntry, m_type), sizeof(VeEventTableEntry::m_type))
	};
	VeFixedSizeTable<VeEventTableEntry> g_events_table(maps2);



	void init() {
		syseng::registerTablePointer( &g_event_types_table, "Event");
	}

	void tick() {

	}

	void close() {

	}

}

