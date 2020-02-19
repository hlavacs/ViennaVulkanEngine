#pragma once



namespace vve::syseng {

	struct VeMainTableEntry {
		tab::VeTable*	m_table_pointer;
		std::string		m_name;
	};

	struct VeSysTableEntry {
		std::function<void()>	m_tick;
		std::function<void()>	m_close;
		std::string				m_name;
	};

#ifndef VE_PUBLIC_INTERFACE

	///
	void registerTablePointer(tab::VeTable* ptr, std::string name );
	tab::VeTable* getTablePointer( std::string name );

#endif

	///Public engine interface
	void initEngine();
	void runGameLoop();
	void computeOneFrame();
	void closeEngine();

}

