#pragma once

/**
*
* \file
* \brief
*
* Details
*
*/


namespace vve::syseng {


#ifndef VE_PUBLIC_INTERFACE

	void registerTablePointer(VeTable* ptr, std::string name );
	VeTable* getTablePointer( std::string name );
	void tick();
	void cleanUp();
	void forwardTime();

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

