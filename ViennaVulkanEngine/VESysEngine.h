#pragma once

/**
*
* \file
* \brief
*
* Details
*
*/


namespace vve {

	VeHeapMemory* getHeap();

	namespace syseng {

		inline const std::string VE_SYSTEM_NAME = "VE SYSTEM ENGINE";
		inline VeHandle VE_SYSTEM_HANDLE = VE_NULL_HANDLE;

#ifndef VE_PUBLIC_INTERFACE

		void registerEntity(const std::string& name);
		VeHandle getEntityHandle(const std::string& name);
		void registerTablePointer(VeTable* ptr);
		VeTable* getTablePointer(const std::string name);
		void update();
		void cleanUp();
		void forwardTime();
		void createHeaps(uint32_t num);
		VeHeapMemory* getHeap();

#endif

		using namespace std::chrono;

		duration<double, std::micro>	  getTimeDelta();
		time_point<high_resolution_clock> getNowTime();
		time_point<high_resolution_clock> getCurrentUpdateTime();
		time_point<high_resolution_clock> getNextUpdateTime();
		time_point<high_resolution_clock> getReachedTime();

		///Public engine interface
		void init();
		void runGameLoop();
		void computeOneFrame();
		void close();

		void closeEngine();
	}
}

