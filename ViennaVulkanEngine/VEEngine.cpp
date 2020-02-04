


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
			VeHandle handle = getNewHandle();
			std::cout << handle << "\n";
		}

		syswin::initWindow();
		sysvul::initVulkan();

	}

	void runGameLoop() {
		while (g_goon) {

		}
	}


	//-----------------------------------------------------------------------------------
	VeHandle getNewHandle() {
		return (VeHandle)g_handle_counter.fetch_add(1);
	};



}

