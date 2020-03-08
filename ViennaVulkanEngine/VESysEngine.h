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

#endif
	double getTimeDelta();
	double getNowTime();
	double getCurrentUpdateTime();
	double getNextUpdateTime();
	double getReachedTime();

	///Public engine interface
	void init();
	void runGameLoop();
	void computeOneFrame();
	void close();

	void closeEngine();
}

