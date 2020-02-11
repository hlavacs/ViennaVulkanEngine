#pragma once


#include "VEDefines.h"

namespace ve {

#ifndef VE_PUBLIC_INTERFACE

	///
	struct VeMainTableEntry {
		mem::VeFixedSizeTable *	m_table_pointer;
		std::string				m_name;
	};

	mem::VeFixedSizeTypedTable<VeMainTableEntry> *	g_main_table = nullptr;
	mem::VeFixedSizeTable*							getTablePointer(std::string name);

#endif

	///Public engine interface
	void initEngine();
	void runGameLoop();

}

