

#include "vulkan/vulkan.h"

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
	std::vector<VeMap*> maps = {
		(VeMap*) new VeTypedMap< std::unordered_map<VeTableKeyString, VeTableIndex >, VeTableKeyString, VeTableIndex >(
		(VeIndex)offsetof(struct VeMainTableEntry, m_name), 0)
	};
	VeFixedSizeTable<VeMainTableEntry> g_main_table(maps);

	struct VeSysTableEntry {
		std::function<void()>	m_init;
		std::function<void()>	m_sync;
		std::function<void()>	m_tick;
		std::function<void()>	m_close;
		VeSysTableEntry() : m_init(), m_sync(), m_tick(), m_close() {};
		VeSysTableEntry(std::function<void()> init, std::function<void()> sync,
			std::function<void()> tick, std::function<void()> close) :
			m_init(init), m_sync(sync), m_tick(tick), m_close(close) {};
	};
	VeFixedSizeTable<VeSysTableEntry> g_systems_table;


	void registerTablePointer(VeTable* tptr, std::string name) {
		g_main_table.addEntry({ tptr, name});
	}

	VeTable* getTablePointer(std::string name) {
		VeMainTableEntry entry;
		if( g_main_table.getEntry(g_main_table.getHandleEqual(0, name), entry) )
			return entry.m_table_pointer;
		return nullptr;
	}

	void registerSystem(std::function<void()> init, std::function<void()> sync, std::function<void()> tick, std::function<void()> close) {
		g_systems_table.addEntry({ init, sync, tick, close });
	}

	void init() {
		std::cout << "init engine 2\n";

		registerTablePointer(&g_main_table, "Main Table");
		registerTablePointer(&g_systems_table, "Systems Table");
		registerSystem(syswin::init, syswin::sync, []() {},      syswin::close);	//first init window to get the surface!
		registerSystem(sysvul::init, sysvul::sync, sysvul::tick, sysvul::close);
		registerSystem(syseve::init, syseve::sync, syseve::tick, syseve::close);
		registerSystem(sysass::init, sysass::sync, sysass::tick, sysass::close);
		registerSystem(syssce::init, syssce::sync, syssce::tick, syssce::close);
		registerSystem(sysphy::init, sysphy::sync, sysphy::tick, sysphy::close);

#ifdef VE_ENABLE_MULTITHREADING
		VeIndex i = 0, threadCount = (VeIndex)vgjs::JobSystem::getInstance()->getThreadCount();
		for (auto table : g_main_table.getData()) {
			table.m_table_pointer->setThreadId(i % threadCount);
			++i;
		}
#endif

		for (auto entry : g_systems_table.getData()) 
			entry.m_init();

		g_systems_table.setReadOnly(true);
		g_main_table.setReadOnly(true);
	}


	void sync() {
		//might copy all new game states here
		for (auto entry : g_systems_table.getData())
			JADD( entry.m_sync() );
	}

	void tick() {
		for (auto entry : g_systems_table.getData()) 
			JADD( entry.m_tick() );

		JDEP(sync());
	}

	void computeOneFrame() {
		syswin::tick();			//must poll GLFW events in the main thread
		JADD(tick());			//start as child thread so there can be dependencies

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
	}

	void closeEngine() {
		g_goon = false;
	}

	void close() {

#ifdef VE_ENABLE_MULTITHREADING
		vgjs::JobSystem::getInstance()->terminate();
		vgjs::JobSystem::getInstance()->waitForTermination();
#endif

		auto data = g_systems_table.getData();
		for (int32_t i = (int32_t)data.size() - 1; i >= 0; --i) 
			data[i].m_close();
		
		g_systems_table.setReadOnly(false);
		g_main_table.setReadOnly(false);
		g_systems_table.clear();
		g_main_table.clear();
	}

}

