#pragma once


namespace syseve {

	struct VeEventTableEntry {
		std::string m_name;
	};


#ifndef VE_PUBLIC_INTERFACE

	void initEvents();
	void tickEvents();
	void closeEvents();

#endif



}

