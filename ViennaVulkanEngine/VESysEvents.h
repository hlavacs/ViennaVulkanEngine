#pragma once


namespace vve::syseve {

	struct VeEventTableEntry {
		std::string m_name;
	};


#ifndef VE_PUBLIC_INTERFACE

	void init();
	void createTables();
	void tick();
	void close();


#endif



}

