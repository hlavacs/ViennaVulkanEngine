


#include "VEDefines.h"
#include "VEMemory.h"
#include "VEEngine.h"
#include "VESysVulkan.h"
#include "VESysWindow.h"


namespace ve {

	//-----------------------------------------------------------------------------------

	bool g_goon = true;


	void createTables() {
		std::vector<mem::VeMap*> maps = {
			(mem::VeMap*) new std::unordered_map<VeHandle, VeIndex>
		};

		g_main_table = new mem::VeFixedSizeTypedTable<ve::VeMainTableEntry>( std::move(maps) );
	}

	void initEngine() {
		std::cout << "init engine 2\n";

		createTables();
		syswin::initWindow();
		sysvul::initVulkan();

	}

	void runGameLoop() {
		while (g_goon) {

		}
	}





}

