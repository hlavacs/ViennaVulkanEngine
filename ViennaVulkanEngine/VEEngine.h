#pragma once



namespace ve {

	struct VeMainTableEntry {
		mem::VeTable*	m_table_pointer;
		std::string		m_name;
	};

	struct VeSysTableEntry {
		std::function<void()>	m_tick;
		std::function<void()>	m_close;
		std::string				m_name;
	};

#ifndef VE_PUBLIC_INTERFACE

	///
	void registerTablePointer( mem::VeTable* ptr, std::string name );
	mem::VeTable* getTablePointer( std::string name );

#endif

	///Public engine interface
	void initEngine();
	void runGameLoop();
	void computeOneFrame();
	void closeEngine();

}

