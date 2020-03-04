#pragma once



namespace vve::syseng {


#ifndef VE_PUBLIC_INTERFACE

	void registerTablePointer(VeTable* ptr, std::string name );
	VeTable* getTablePointer( std::string name );
	void tick();

#endif

	///Public engine interface
	void init();
	void runGameLoop();
	void computeOneFrame();
	void close();

	void closeEngine();
}

