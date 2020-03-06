

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
		new VeTypedMap< std::unordered_map<VeTableKeyString, VeTableIndex >, VeTableKeyString, VeTableIndex >((VeIndex)offsetof(struct VeMainTableEntry, m_name), 0)
	};
	VeFixedSizeTable<VeMainTableEntry> g_main_table(maps);

	struct VeSysTableEntry {
		std::function<void()>	m_init;
		std::function<void()>	m_tick;
		std::function<void()>	m_close;
		VeSysTableEntry() : m_init(), m_tick(), m_close() {};
		VeSysTableEntry(std::function<void()> init, std::function<void()> tick, std::function<void()> close) : 
			m_init(init), m_tick(tick), m_close(close) {};
	};
	VeFixedSizeTable<VeSysTableEntry> g_systems_table;


	void registerTablePointer(VeTable* tptr, std::string name) {
		g_main_table.VeFixedSizeTable<VeMainTableEntry>::addEntry({ tptr, name});
	}

	VeTable* getTablePointer(std::string name) {
		VeMainTableEntry entry;
		if( g_main_table.getEntry(g_main_table.getHandleEqual(0, name), entry) )
			return entry.m_table_pointer;
		return nullptr;
	}

	auto time_delta								= std::chrono::microseconds( (int)((1.0f/60.0)*1000000.0f) );
	std::chrono::time_point now_time			= std::chrono::high_resolution_clock::now();
	std::chrono::time_point current_update_time = now_time;
	std::chrono::time_point next_update_time	= current_update_time + time_delta;
	std::chrono::time_point reached_time		= now_time;


	void init() {
		std::cout << "init engine 2\n";

		now_time = std::chrono::high_resolution_clock::now();
		next_update_time = now_time + time_delta;

		registerTablePointer(&g_main_table, "Main Table");
		registerTablePointer(&g_systems_table, "Systems Table");

		//first init window to get the surface!
		g_systems_table.addEntry({ syswin::init, []() {},      syswin::close });
		g_systems_table.addEntry({ sysvul::init, []() {},      sysvul::close });
		g_systems_table.addEntry({ syseve::init, syseve::tick, syseve::close });
		g_systems_table.addEntry({ sysass::init, sysass::tick, sysass::close });
		g_systems_table.addEntry({ syssce::init, syssce::tick, syssce::close });
		g_systems_table.addEntry({ sysphy::init, sysphy::tick, sysphy::close });

#ifdef VE_ENABLE_MULTITHREADING
		VeIndex i = 0, threadCount = (VeIndex)vgjs::JobSystem::getInstance()->getThreadCount();
		for (auto table : g_main_table.getData()) {
			if (!table.m_table_pointer->getReadOnly()) {
				table.m_table_pointer->setThreadId(i % threadCount);
				++i;
			}
		}
#endif

		for (auto entry : g_systems_table.getData()) 
			entry.m_init();

		g_systems_table.setReadOnly(true);
		g_main_table.setReadOnly(true);
	}


	void tickSystems() {
		for (auto entry : g_systems_table.getData()) //simulate one epoch
			JADD( entry.m_tick() );

		if (now_time < next_update_time) {			//if now is reached, render frame
			JDEP(reached_time = next_update_time; sysvul::tick(); );
		} else
			JDEP(reached_time = next_update_time; tick(); ); //else move one epoch further
	}

	void tick() {
		current_update_time = next_update_time;		//move one epoch further
		next_update_time	= current_update_time + time_delta;

		for (auto table : g_main_table.getData())	//swap tables
			table.m_table_pointer->swapTables();

		JDEP(tickSystems());						//simulate one epoch
	}

	void computeOneFrame() {
		syswin::tick();			//must poll GLFW events in the main thread

		now_time = std::chrono::high_resolution_clock::now();
		if (now_time < next_update_time) {		//if caught up with now
			JADD(sysvul::tick());				//render the next frame
		} else
			JADD(tick());						//else bring simulation to now

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
		g_goon = false;

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

