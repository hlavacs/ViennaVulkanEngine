


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
			(mem::VeMap*) new mem::VeTypedMap< std::unordered_map<VeHandle, VeIndex> >(),
			(mem::VeMap*) new mem::VeTypedStringMap< std::unordered_map<std::string, VeIndex> >( offsetof(struct VeMainTableEntry, m_name ))
		};
		g_main_table = new mem::VeFixedSizeTypedTable<ve::VeMainTableEntry>( std::move(maps), 0 );
	}

	mem::VeFixedSizeTable* getTablePointer(std::string name) {

		std::vector<VeMainTableEntry>& data = g_main_table->getData();
		for (uint32_t i = 0; i < data.size(); ++i) if (data[i].m_name == name) return data[i].m_table_pointer; 
		return nullptr;
	}


	///public interface

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

