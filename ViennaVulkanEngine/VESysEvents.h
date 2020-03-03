#pragma once


namespace vve::syseve {

	struct VeEventTypeTableEntry {

		std::string m_name;
	};

	struct VeEventTableEntry {
		VeHandle m_type;
	};

	struct VeEventRegisteredHandlerTableEntry {
		VeHandle m_type;
	};


#ifndef VE_PUBLIC_INTERFACE

	void init();
	void tick();
	void sync();
	void close();


#endif



}

