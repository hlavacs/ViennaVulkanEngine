


#include "VEDefines.h"
#include "VEMemory.h"
#include "VEEngine.h"
#include "VESysVulkan.h"
#include "VESysWindow.h"


namespace ve {

	//-----------------------------------------------------------------------------------

	bool g_goon = true;


	void createTables() {
		using MapPtr = mem::VeMap*;

		std::vector<MapPtr> maps = {
			(MapPtr) new mem::VeTypedMap< std::unordered_map<VeHandle, VeIndex> >()
		};

		//g_main_table = new mem::VeFixedSizeTypedTable<ve::VeMainTableEntry>( std::move(maps), 0 );
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

