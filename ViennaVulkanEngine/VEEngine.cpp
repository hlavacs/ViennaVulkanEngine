
#include <iostream>

#include "VEEngine.h"
#include "VESysVulkan.h"
#include "VESysWindow.h"


namespace ve {

	std::atomic<uint64_t> g_handle_counter = 1;

	VEHANDLE getNewHandle() {
		std::uint64_t id = (uint64_t)g_handle_counter.fetch_add(1);
		return (id << 32) + (uint64_t) std::hash<uint64_t>()(id) & 0xFFFFFFFF;
	};


	void initEngine() {
		std::cout << "init engine 2\n";

		for (int i = 0; i < 100; i++) {
			std::cout << getNewHandle() << "\n";
		}

		syswin::initWindow();
		sysvul::initVulkan();

	}


	void runGameLoop() {

	}


}

