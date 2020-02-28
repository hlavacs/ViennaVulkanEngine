

#include "VEDefines.h"
#include "VESysEngine.h"
#include "VESysEvents.h"


namespace vve::syseve {


	std::vector<VeMap*> maps = {
		(VeMap*) new VeTypedMap< std::unordered_map<VeHandle, VeIndex>,		VeHandle,		VeIndex >(VE_NULL_INDEX,0),
		(VeMap*) new VeTypedMap< std::unordered_map<std::string, VeIndex>,	std::string,	VeIndex >(offsetof(struct VeEventTableEntry, m_name), 0)
	};
	VeFixedSizeTable<VeEventTableEntry> g_event_table(maps);

	void init() {
		syseng::registerTablePointer( &g_event_table, "Event");
	}

	void tick() {

	}

	void sync() {
	}

	void close() {

	}

}

