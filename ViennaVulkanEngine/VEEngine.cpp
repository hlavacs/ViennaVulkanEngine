
#include <iostream>
#include <vector>

#include "VEDefines.h"
#include "VEMemory.h"
#include "VEEngine.h"
#include "VESysVulkan.h"
#include "VESysWindow.h"


namespace ve {

	//-----------------------------------------------------------------------------------

	std::atomic<uint32_t> g_handle_counter = 0;

	bool g_goon = true;

	void initEngine() {
		std::cout << "init engine 2\n";

		for (int i = 0; i < 100; i++) {
			VEHANDLE handle = getNewHandle();
			std::cout << handle.m_id << " " << handle.m_hash << "\n";
		}

		syswin::initWindow();
		sysvul::initVulkan();

	}

	void runGameLoop() {
		while (g_goon) {

		}
	}


	//-----------------------------------------------------------------------------------
	VEHANDLE getNewHandle() {
		VEHANDLE handle;
		handle.m_id = (uint32_t)g_handle_counter.fetch_add(1);
		handle.m_hash = (uint32_t)std::hash<uint64_t>()(handle.m_id);
		return handle;
	};



}

