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


	namespace syseng {

		inline const std::string VE_SYSTEM_NAME = "VE SYSTEM ENGINE";
		inline VeHandle VE_SYSTEM_HANDLE = VE_NULL_HANDLE;

		VeHandle		getGUID();
		void			createHeaps(uint32_t num);
		VeHeapMemory*	getHeap();
		VeHeapMemory*	getTmpHeap();

		VeHandle	registerEntity(const std::string& name);
		VeHandle	getEntityHandle(const std::string& name);
		void		registerTablePointer(VeTable* ptr);
		VeTable*	getTablePointer(const std::string name);

		std::chrono::duration<double, std::micro>					getTimeDelta();
		std::chrono::time_point<std::chrono::high_resolution_clock> getNowTime();
		std::chrono::time_point<std::chrono::high_resolution_clock> getCurrentUpdateTime();
		std::chrono::time_point<std::chrono::high_resolution_clock> getNextUpdateTime();
		std::chrono::time_point<std::chrono::high_resolution_clock> getReachedTime();

		void init();
		void forwardTime();
		void runGameLoop();
		void computeOneFrame();
		void close();
	}

	inline VeHandle getGUID() {
		return syseng::getGUID();
	};

	inline 	VeHeapMemory* getHeap() {
		return syseng::getHeap();
	};

	inline 	VeHeapMemory* getTmpHeap() {
		return syseng::getTmpHeap();
	};

}

