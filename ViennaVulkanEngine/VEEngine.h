#pragma once


#include "VEDefines.h"

namespace ve {

#ifndef VE_PUBLIC_INTERFACE
	struct VeMainTableEntry {
		std::shared_ptr<mem::VeFixedSizeTable> m_table_pointer;
	};

	mem::VeFixedSizeTypedTable<ve::VeMainTableEntry> * g_main_table = nullptr;

#endif

	///Public engine interface
	void initEngine();
	void runGameLoop();

}

