

#include "VEDefines.h"
#include "VESysEngine.h"
#include "VESysEvents.h"


namespace vve::syseve {


	VeFixedSizeTable<VeEventTableEntry> g_event_table;

	void init() {
		g_event_table.addMap((VeMap*) new VeTypedMap< std::unordered_map<VeHandle, VeIndex>,		VeHandle,		VeIndex >(VE_NULL_INDEX,0));
		g_event_table.addMap((VeMap*) new VeTypedMap< std::unordered_map<std::string, VeIndex>,	std::string,	VeIndex >(offsetof(struct VeEventTableEntry, m_name), 0));
		syseng::registerTablePointer( &g_event_table, "Event");
	}

	void tick() {

	}

	void sync() {
	}

	void close() {

	}

}

