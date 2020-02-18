

#include "VEDefines.h"
#include "VETable.h"
#include "VESysEngine.h"
#include "VESysEvents.h"


namespace syseve {


	tab::VeFixedSizeTable<VeEventTableEntry>* g_event_table = nullptr;

	void initEvents() {
		std::vector<tab::VeMap*> maps = {
			(tab::VeMap*) new tab::VeTypedMap< std::unordered_map<VeHandle, VeIndex>,		VeHandle,		VeIndex >(VE_NULL_INDEX,0),
			(tab::VeMap*) new tab::VeTypedMap< std::unordered_map<std::string, VeIndex>,	std::string,	VeIndex >(offsetof(struct VeEventTableEntry, m_name), 0)
		};
		g_event_table = new tab::VeFixedSizeTable<VeEventTableEntry>(std::move(maps), 0);
		syseng::registerTablePointer( g_event_table, "Event Table");

	}

	void tickEvents() {

	}

	void closeEvents() {

	}

}

