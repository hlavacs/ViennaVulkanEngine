


#include "VEDefines.h"
#include "VEMemory.h"
#include "VEEngine.h"
#include "VESysVulkan.h"
#include "VESysWindow.h"


namespace ve {

	//-----------------------------------------------------------------------------------

	bool g_goon = true;


	void createTables() {
		using MapPtr = std::unique_ptr<mem::VeMap>;

		std::vector<MapPtr> maps = {
			std::make_unique<mem::VeMap>( (mem::VeMap*) new mem::VeTypedMap<std::unordered_map<VeHandle, VeIndex>>(VE_NULL_INDEX) )
		};

		g_main_table = new mem::VeFixedSizeTypedTable<ve::VeMainTableEntry>( std::move(maps), 0 );
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

