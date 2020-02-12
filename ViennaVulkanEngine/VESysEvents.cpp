

#include "VEDefines.h"
#include "VEMemory.h"
#include "VEEngine.h"
#include "VESysEvents.h"


namespace syseve {


	mem::VeFixedSizeTypedTable<VeEventTableEntry>* g_event_table = nullptr;

	void initEvents() {
		std::vector<mem::VeMap*> maps = {
			(mem::VeMap*) new mem::VeTypedMap< std::unordered_map<VeHandle, VeIndex>,		VeHandle,		VeIndex >(VE_NULL_INDEX,0),
			(mem::VeMap*) new mem::VeTypedMap< std::unordered_map<std::string, VeIndex>,	std::string,	VeIndex >(offsetof(struct VeEventTableEntry, m_name), 0)
		};
		g_event_table = new mem::VeFixedSizeTypedTable<VeEventTableEntry>(std::move(maps), 0);
		ve::registerTablePointer( g_event_table, "Event Table");

	}

	void tickEvents() {

	}

	void closeEvents() {

	}

}

