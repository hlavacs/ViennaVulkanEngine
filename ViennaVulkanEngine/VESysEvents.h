#pragma once


namespace vve::syseve {

	struct VeEventTableEntry {
		std::string m_name;
	};


#ifndef VE_PUBLIC_INTERFACE

	void init();
	void tick();
	void sync();
	void close();


#endif



}

