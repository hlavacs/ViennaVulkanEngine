#pragma once



namespace vve::syseng {


#ifndef VE_PUBLIC_INTERFACE

	void registerTablePointer(VeTable* ptr, std::string name );
	void registerSystem(std::function<void()> init, std::function<void()> tick, std::function<void()> sync, std::function<void()> close);
	VeTable* getTablePointer( std::string name );

	void createTables();

#endif

	///Public engine interface
	void init();
	void runGameLoop();
	void computeOneFrame();
	void close();
}

