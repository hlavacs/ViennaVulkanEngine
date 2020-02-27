

#define IMPLEMENT_GAMEJOBSYSTEM

#include "VEDefines.h"
#include "VESysEngine.h"
#include "VESysVulkan.h"
#include "VESysWindow.h"
#include "VESysEvents.h"
#include "VESysPhysics.h"
#include "VESysAssets.h"
#include "VESysScene.h"


namespace vve::syseng {

	//-----------------------------------------------------------------------------------

	struct VeMainTableEntry {
		VeTable	*	m_table_pointer;
		std::string	m_name;
		VeMainTableEntry() : m_table_pointer(nullptr), m_name("") {};
		VeMainTableEntry( VeTable* tptr, std::string name) : m_table_pointer(tptr), m_name(name) {};
	};
	VeFixedSizeTable<VeMainTableEntry> g_main_table;

	struct VeSysTableEntry {
		std::function<void()>	m_init;
		std::function<void()>	m_tick;
		std::function<void()>	m_sync;
		std::function<void()>	m_close;
		VeSysTableEntry() : m_init(), m_tick(), m_sync(), m_close() {};
		VeSysTableEntry(std::function<void()> init, std::function<void()> tick, 
			std::function<void()> sync, std::function<void()> close) :
			m_init(init), m_tick(tick), m_sync(sync), m_close(close) {};
	};
	VeFixedSizeTable<VeSysTableEntry> g_systems_table;


	void registerTablePointer(VeTable* tptr, std::string name) {
		g_main_table.addEntry({ tptr, name});
	}

	void createTables() {
		g_main_table.addMap(
			(VeMap*) new VeTypedMap< std::unordered_map<VeTableKeyString, VeTableIndex >, VeTableKeyString, VeTableIndex >(
			(VeIndex)offsetof(struct VeMainTableEntry, m_name), 0) );

		registerTablePointer(&g_systems_table, "Systems");
	}

	VeTable* getTablePointer(std::string name) {
		VeMainTableEntry entry;
		if( g_main_table.getEntry(g_main_table.getHandleEqual(0, name), entry) )
			return entry.m_table_pointer;
		return nullptr;
	}

	void registerSystem(std::function<void()> init, std::function<void()> tick, std::function<void()> sync, std::function<void()> close) {
		g_systems_table.addEntry({ init, tick, sync, close });
	}

	void init() {
		std::cout << "init engine 2\n";

		createTables();
		registerSystem(syswin::init, syswin::tick, syswin::sync, syswin::close);
		registerSystem(sysvul::init, sysvul::tick, sysvul::sync, sysvul::close);
		registerSystem(syseve::init, syseve::tick, syseve::sync, syseve::close);
		registerSystem(sysass::init, sysass::tick, sysass::sync, sysass::close);
		registerSystem(syssce::init, syssce::tick, syssce::sync, syssce::close);
		registerSystem(sysphy::init, sysphy::tick, sysphy::sync, sysphy::close);

		for (auto entry : g_systems_table.getData()) 
			entry.m_init();

		g_systems_table.setReadOnly(true);
		g_main_table.setReadOnly(true);
	}

	void tick() {
		for (auto entry : g_systems_table.getData()) 
			JADD( entry.m_tick() );
	}

	void sync() {
		//might copy all new game states here
	}

	void computeOneFrame2() {
		JADD(sync());

		for (auto entry : g_systems_table.getData())
			JADD(entry.m_sync());

		JDEP(tick());
	}

	void computeOneFrame() {
		JADD(computeOneFrame2());

#ifdef VE_ENABLE_MULTITHREADING
		vgjs::JobSystem::getInstance()->wait();
		vgjs::JobSystem::getInstance()->resetPool();
#endif
	}

	std::atomic<bool> g_goon = true;

	void runGameLoop() {
		while (g_goon) {
			computeOneFrame();
		}

#ifdef VE_ENABLE_MULTITHREADING
		vgjs::JobSystem::getInstance()->terminate();
		vgjs::JobSystem::getInstance()->waitForTermination();
#endif
	}

	void close() {
		g_goon = false;

		auto data = g_systems_table.getData();
		for (int32_t i = (int32_t)data.size() - 1; i >= 0; --i) 
			data[i].m_close();
		
		g_systems_table.setReadOnly(false);
		g_main_table.setReadOnly(false);
		g_systems_table.clear();
		g_main_table.clear();
	}

}

