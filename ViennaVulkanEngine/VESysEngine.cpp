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


namespace vve {

	std::vector<VeHeapMemory> g_thread_heap(1);

	VeHeapMemory* getHeap() {
		return syseng::getHeap();
	}

	namespace syseng {

		VeHeapMemory* getHeap() {
			VeIndex heapIdx = 0;
#ifdef VE_ENABLE_MULTITHREADING
			uint32_t threadIdx = vgjs::JobSystem::getInstance()->getThreadIndex();
			if (threadIdx < g_thread_heap.size())
				heapIdx = threadIdx;
#endif
			return &g_thread_heap[heapIdx];
		}

		void createHeaps(uint32_t num) {
			for (uint32_t i = (uint32_t)g_thread_heap.size() - 1; i < num; ++i)
				g_thread_heap.emplace_back(VeHeapMemory());
		}

		//-----------------------------------------------------------------------------------

		struct VeMainTableEntry {
			VeTable* m_table_pointer;
			std::string	m_name;
			VeMainTableEntry() : m_table_pointer(nullptr), m_name("") {};
			VeMainTableEntry(VeTable* tptr, std::string name) : m_table_pointer(tptr),  m_name(name) {};
		};
		std::vector<VeMap*> maps = {
			new VeOrderedMultimap< std::string, VeIndex >((VeIndex)offsetof(struct VeMainTableEntry, m_name), 0)
		};
		VeFixedSizeTable<VeMainTableEntry> g_main_table("Main Table", maps, false, false, 0, 0);

		struct VeSysTableEntry {
			std::function<void()>	m_init;
			std::function<void()>	m_update;
			std::function<void()>	m_cleanUp;
			std::function<void()>	m_close;
			VeSysTableEntry() : m_init(), m_update(), m_cleanUp(), m_close() {};
			VeSysTableEntry(std::function<void()> init, std::function<void()> tick, std::function<void()> cleanUp, std::function<void()> close) :
				m_init(init), m_update(tick), m_cleanUp(cleanUp), m_close(close) {};
		};
		VeFixedSizeTable<VeSysTableEntry> g_systems_table( "Systems Table", false, false, 0, 0);


		void registerTablePointer(VeTable* tptr) {
			g_main_table.VeFixedSizeTable<VeMainTableEntry>::addEntry({ tptr, tptr->getName() });
		}

		VeTable* getTablePointer(std::string name) {
			VeMainTableEntry entry;
			if (g_main_table.getEntry(g_main_table.getHandleEqual(0, name), entry))
				return entry.m_table_pointer;
			return nullptr;
		}

		using namespace std::chrono;

		duration<int, std::micro> time_delta = duration<int, std::micro>{ 16666 };
		time_point<high_resolution_clock> now_time = high_resolution_clock::now();			//now time
		time_point<high_resolution_clock> current_update_time = now_time;								//start of the current epoch
		time_point<high_resolution_clock> next_update_time = current_update_time + time_delta;		//end of the current epoch
		time_point<high_resolution_clock> reached_time = current_update_time;					//time the simulation has reached

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

			registerTablePointer(&g_main_table);
			registerTablePointer(&g_systems_table);

			//first init window to get the surface!
			g_systems_table.addEntry({ syswin::init, []() {},        syswin::cleanUp, syswin::close });
			g_systems_table.addEntry({ sysvul::init, sysvul::update, sysvul::cleanUp, sysvul::close });
			g_systems_table.addEntry({ syseve::init, syseve::update, syseve::cleanUp, syseve::close });
			g_systems_table.addEntry({ sysass::init, sysass::update, sysass::cleanUp, sysass::close });
			g_systems_table.addEntry({ syssce::init, syssce::update, syssce::cleanUp, syssce::close });
			g_systems_table.addEntry({ sysphy::init, sysphy::update, sysphy::cleanUp, sysphy::close });

			for (auto entry : g_systems_table.getData()) {
				entry.m_init();
			}

			uint32_t threadCount = 1;

#ifdef VE_ENABLE_MULTITHREADING
			VeIndex i = 0;
			for (auto table : g_main_table.getData()) {
				table.m_table_pointer->setName(table.m_name);
				if (!table.m_table_pointer->getReadOnly()) {
					table.m_table_pointer->setThreadId(i);
					++i;
				}
			}
#endif
			createHeaps(threadCount);

			g_systems_table.setReadOnly(true);
			g_main_table.setReadOnly(true);
		}

		VeClock loopClock(   "Game loop   ", 1);
		VeClock swapClock(   "Swap Clock  ", 200);
		VeClock forwardClock("FW Clock    ", 1);
		VeClock tickClock(   "Tick Clock  ", 100);
		VeClock cleanClock(  "Clean Clock ", 100);

		void cleanUp() {
			for (auto entry : g_systems_table.getData()) {			//clean after simulation
				syseve::addEvent({ syseve::VeEventType::VE_EVENT_TYPE_FRAME_TICK });
				JADD(entry.m_cleanUp());							//includes render in the last interval
			}
		}

		void update() {
			for (auto entry : g_systems_table.getData()) {			//simulate one epoch
				JADD(entry.m_update());
			}
		}

		void swapTables() {
			for (auto table : g_main_table.getData()) {	//swap tables
				//std::cout << "swap table " << table.m_name << " " << std::endl;
				if (table.m_table_pointer->getCompanionTable() != nullptr) {
					JADD(table.m_table_pointer->swapTables());	//might clear() some tables here
				}
			}
		}

		void forwardTime() {
			current_update_time = next_update_time;		//move one epoch further
			next_update_time = current_update_time + time_delta;

			syseve::addEvent({ syseve::VeEventType::VE_EVENT_TYPE_EPOCH_TICK });
		}

		//acts like a co-routine
		void computeOneFrame2(uint32_t step) {

			if (step == 1) goto step1;
			if (step == 2) goto step2;
			if (step == 3) goto step3;
			if (step == 4) goto step4;
			if (step == 5) goto step5;

			syswin::update();		//must poll GLFW events in the main thread

			now_time = std::chrono::high_resolution_clock::now();

			if (now_time < next_update_time) {		//still in the same time epoch
				return;
			}
			JDEP(computeOneFrame2(1));		//wait for finishing, then do step3
			return;

		step1:
			//forwardClock.tick();
			forwardTime();
			JDEP(computeOneFrame2(2));		//wait for finishing, then do step3
			return;

		step2:
			//swapClock.start();
			swapTables();
			JDEP(computeOneFrame2(3));		//wait for finishing, then do step3
			return;

		step3:
			//tickClock.start();
			update();
			JDEP( computeOneFrame2(4));		//wait for finishing, then do step4
			return;

		step4:
			//cleanClock.start();
			cleanUp();
			JDEP(computeOneFrame2(5));		//wait for finishing, then do step5
			return;

		step5: 
			reached_time = next_update_time;

			if (now_time > next_update_time) {	//if now is not reached yet
				JDEP( computeOneFrame2(1); );	//move one epoch further
			}
		}


		//can call this from the main thread if you have your own game loop
		void computeOneFrame() {
			computeOneFrame2(0);
			JWAIT;
			JRESET;
		}

		std::atomic<bool> g_goon = true;

		void runGameLoopMT() {
			loopClock.tick();

			syseve::addEvent({ syseve::VeEventType::VE_EVENT_TYPE_LOOP_TICK });

			JRESET;
			JADDT(computeOneFrame2(0), vgjs::TID(0, 2));	 //run on main thread for polling!
			if (g_goon) {
				JREP;
			}
		}

		void runGameLoop() {

#ifdef VE_ENABLE_MULTITHREADING
			vgjs::JobSystem::getInstance(4, 1); //create pool without thread 0
			JADD(runGameLoopMT());				//schedule the game loop
			vgjs::JobSystem::getInstance()->threadTask(0);		//put main thread as first thread into pool
			return;
#endif

			while (g_goon) {
				tickClock.tick();
				computeOneFrame();
			}
		}

		void closeEngine() {
			g_goon = false;
			JTERM;
		}

		void close() {
			g_goon = false;

			JTERM;
			JWAITTERM;

			auto data = g_systems_table.getData();
			for (int32_t i = (int32_t)data.size() - 1; i >= 0; --i)
				data[i].m_close();

			g_systems_table.setReadOnly(false);
			g_main_table.setReadOnly(false);
			g_systems_table.clear();
			g_main_table.clear();
		}

	}

}

