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
#include "VESysEvents.h"
#include "VESysEngine.h"
#include "VESysVulkan.h"
#include "VESysWindow.h"
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
		//registered tables

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

		void registerTablePointer(VeTable* tptr) {
			g_main_table.VeFixedSizeTable<VeMainTableEntry>::insert({ tptr, tptr->getName() });
		}

		VeTable* getTablePointer(const std::string name) {
			VeMainTableEntry entry;
			if (g_main_table.getEntry(g_main_table.find(name, 0), entry))
				return entry.m_table_pointer;
			return nullptr;
		}

		//----------------------------------------------------------------------------------------------------
		//entities register here their names

		struct VeEntityTableEntry {
			std::string m_name;
		};
		std::vector<VeMap*> maps2 = {
			new VeHashedMultimap< std::string, VeIndex >((VeIndex)offsetof(struct VeEntityTableEntry, m_name), 0)
		};
		VeFixedSizeTableMT<VeEntityTableEntry> g_entities_table("Entities Table", maps2, false, false, 0, 0);

		void registerEntity(const std::string& name) {
			g_entities_table.insert({ name });
		}

		VeHandle getEntityHandle(const std::string &name ) {
			return g_entities_table.find(name, 0);
		}


		//----------------------------------------------------------------------------------------------------
		//wall clock times for advancing the simulation

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

		//----------------------------------------------------------------------------------------------------

		void closeEngine(syseve::VeEventTableEntry e);
		VeHandle g_closeHandle;

		void init() {
			std::cout << "init engine 2\n";

			now_time = std::chrono::high_resolution_clock::now();
			current_update_time = now_time;
			next_update_time = current_update_time + time_delta;
			reached_time = current_update_time;

			registerTablePointer(&g_main_table);

			registerEntity(VE_SYSTEM_NAME);
			VE_SYSTEM_HANDLE = getEntityHandle(VE_SYSTEM_NAME);

			syswin::init();	//init window to get the surface!
			sysvul::init();
			syseve::init();
			sysass::init();
			syssce::init();
			sysphy::init();

			g_closeHandle = syseve::addHandler( std::bind(closeEngine, std::placeholders::_1));
			syseve::subscribeEvent(	syswin::VE_SYSTEM_HANDLE, VE_NULL_HANDLE, g_closeHandle,
									syseve::VeEventType::VE_EVENT_TYPE_CLOSE);

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
			g_main_table.setReadOnly(true);
		}

		VeClock loopClock(   "Game loop   ", 1);
		VeClock swapClock(   "Swap Clock  ", 200);
		VeClock forwardClock("FW Clock    ", 1);
		VeClock tickClock(   "Tick Clock  ", 100);
		VeClock cleanClock(  "Clean Clock ", 100);

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

			syseve::addEvent({ syseve::VeEventType::VE_EVENT_TYPE_UPDATE, VE_SYSTEM_HANDLE });
		}

		//acts like a co-routine
		void computeOneFrame2(uint32_t step) {

			if (step == 1) goto step1;
			if (step == 2) goto step2;
			if (step == 3) goto step3;
			if (step == 4) goto step4;

			now_time = std::chrono::high_resolution_clock::now();

			if (now_time < next_update_time) {		//still in the same time epoch
				return;
			}

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
			JADD( syseve::update() );
			JDEP( computeOneFrame2(4));		//wait for finishing, then do step4
			return;

		step4:
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

			JRESET;
			JADDT(computeOneFrame2(0), vgjs::TID(0, 2));	 //run on main thread for polling!
			if (g_goon) {
				JREP;
				return;
			}
			JTERM;
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

		void closeEngine(syseve::VeEventTableEntry e) {
			g_goon = false;
		}

		void close() {
			JTERM;
			JWAITTERM;

			g_entities_table.setReadOnly(false);
			g_entities_table.clear();
			g_main_table.setReadOnly(false);
			g_main_table.clear();
		}

	}

}

