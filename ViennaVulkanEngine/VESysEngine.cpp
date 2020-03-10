/**
*
* \file
* \brief
*
* Details
*
*/

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
	VeFixedSizeTable<VeMainTableEntry> g_main_table(maps, false, false, 0, 0);

	struct VeSysTableEntry {
		std::function<void()>	m_init;
		std::function<void()>	m_tick;
		std::function<void()>	m_cleanUp;
		std::function<void()>	m_close;
		VeSysTableEntry() : m_init(), m_tick(), m_cleanUp(), m_close() {};
		VeSysTableEntry(std::function<void()> init, std::function<void()> tick, std::function<void()> cleanUp, std::function<void()> close) :
			m_init(init), m_tick(tick), m_cleanUp(cleanUp), m_close(close) {};
	};
	VeFixedSizeTable<VeSysTableEntry> g_systems_table(false, false, 0, 0);


	void registerTablePointer(VeTable* tptr, std::string name) {
		g_main_table.VeFixedSizeTable<VeMainTableEntry>::addEntry({ tptr, name});
	}

	VeTable* getTablePointer(std::string name) {
		VeMainTableEntry entry;
		if( g_main_table.getEntry(g_main_table.getHandleEqual(0, name), entry) )
			return entry.m_table_pointer;
		return nullptr;
	}

	using namespace std::chrono;

	duration<int,std::micro> time_delta = duration<int, std::micro>{16666};	
	time_point<high_resolution_clock> now_time				= high_resolution_clock::now();			//now time
	time_point<high_resolution_clock> current_update_time	= now_time;								//start of the current epoch
	time_point<high_resolution_clock> next_update_time		= current_update_time + time_delta;		//end of the current epoch
	time_point<high_resolution_clock> reached_time			= current_update_time;					//time the simulation has reached

	duration<double, std::micro> getTimeDelta() {
		return time_delta;
	}

	high_resolution_clock::time_point getNowTime() {
		return now_time;
	};

	high_resolution_clock::time_point getCurrentUpdateTime() {
		return current_update_time;
	};

	high_resolution_clock::time_point getNextUpdateTime() {
		return next_update_time;
	};

	high_resolution_clock::time_point getReachedTime() {
		return reached_time;
	};


	void init() {
		std::cout << "init engine 2\n";

		now_time = std::chrono::high_resolution_clock::now();
		current_update_time = now_time;
		next_update_time = current_update_time + time_delta;
		reached_time = current_update_time;

		registerTablePointer(&g_main_table, "Main Table");
		registerTablePointer(&g_systems_table, "Systems Table");

		//first init window to get the surface!
		g_systems_table.addEntry({ syswin::init, []() {},      syswin::cleanUp, syswin::close });
		g_systems_table.addEntry({ sysvul::init, sysvul::tick, sysvul::cleanUp, sysvul::close });
		g_systems_table.addEntry({ syseve::init, syseve::tick, syseve::cleanUp, syseve::close });
		g_systems_table.addEntry({ sysass::init, sysass::tick, sysass::cleanUp, sysass::close });
		g_systems_table.addEntry({ syssce::init, syssce::tick, syssce::cleanUp, syssce::close });
		g_systems_table.addEntry({ sysphy::init, sysphy::tick, sysphy::cleanUp, sysphy::close });

		for (auto entry : g_systems_table.getData()) {
			entry.m_init();
		}

#ifdef VE_ENABLE_MULTITHREADING
		VeIndex i = 0, threadCount = std::thread::hardware_concurrency();
		for (auto table : g_main_table.getData()) {
			table.m_table_pointer->setName(table.m_name);
			if (!table.m_table_pointer->getReadOnly()) {
				table.m_table_pointer->setThreadId(i);
				++i;
			}
		}
#endif

		g_systems_table.setReadOnly(true);
		g_main_table.setReadOnly(true);
	}


	void cleanUp() {
		for (auto entry : g_systems_table.getData()) {			//clean after simulation
			JADD(entry.m_cleanUp());							//includes render in the last interval
		}

		if (now_time > next_update_time) {						//if now is not reached yet
			JDEP(reached_time = next_update_time; forwardTime(); );	//move one epoch further
		}
		else {
			JDEP(reached_time = next_update_time );				//remember reached time
		}
	}

	void tick() {
		for (auto entry : g_systems_table.getData()) {			//simulate one epoch
			JADD(entry.m_tick());
		}
		JDEP(cleanUp());
	}

	void swapTables() {
		for (auto table : g_main_table.getData()) {	//swap tables
			//std::cout << "swap table " << table.m_name << " " << std::endl;
			table.m_table_pointer->swapTables();	//might clear() some tables here
		}
		JDEP(tick());								//simulate one epoch
	}

	void forwardTime() {
		current_update_time = next_update_time;		//move one epoch further
		next_update_time	= current_update_time + time_delta;

		syseve::addEvent({syseve::VeEventType::VE_EVENT_TYPE_EPOCH_TICK});

		JDEP(swapTables());						//simulate one epoch
	}

	void computeOneFrame2() {
		syseve::addEvent({syseve::VeEventType::VE_EVENT_TYPE_FRAME_TICK});
		syswin::tick();							//must poll GLFW events in the main thread

		now_time = std::chrono::high_resolution_clock::now();

		if (now_time < next_update_time) {		//still in the same time interval
			JDEP(tick());						//tick, process events and render one frame
		}
		else {
			JDEP(forwardTime());				//bring simulation to now
		}
	}

	//can call this from the main thread if you have your own game loop
	void computeOneFrame() {
		computeOneFrame2();

#ifdef VE_ENABLE_MULTITHREADING
		vgjs::JobSystem::getInstance()->wait();
		vgjs::JobSystem::getInstance()->resetPool();
#endif
	}

	std::atomic<bool> g_goon = true;

	void runGameLoop2() {
		vgjs::JobSystem::getInstance()->resetPool();
		JADDT( computeOneFrame2(), vgjs::TID(0,2) );	 //run on main thread for polling!
		if (g_goon) {
			JREP;
		}
	}

	void runGameLoop() {

#ifdef VE_ENABLE_MULTITHREADING
		vgjs::JobSystem::getInstance(0, 1); //create pool without thread 0
		JADD(runGameLoop2());				//schedule the game loop
		vgjs::JobSystem::getInstance()->threadTask(0);		//put main thread as first thread into pool
		return;
#endif

		while (g_goon) {
			computeOneFrame();
		}
	}

	void closeEngine() {
		g_goon = false;

#ifdef VE_ENABLE_MULTITHREADING
		vgjs::JobSystem::getInstance()->terminate();
#endif

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

