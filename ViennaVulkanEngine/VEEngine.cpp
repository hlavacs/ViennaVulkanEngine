


#include "VEDefines.h"
#include "VEMemory.h"
#include "VEEngine.h"
#include "VESysVulkan.h"
#include "VESysWindow.h"
#include "VESysEvents.h"
#include "VESysPhysics.h"
#include "VESysScene.h"


namespace ve {

	//-----------------------------------------------------------------------------------

	bool g_goon = true;
	mem::VeFixedSizeTypedTable<VeMainTableEntry>*	g_main_table = nullptr;
	mem::VeFixedSizeTypedTable<VeSysTableEntry>*	g_systems_table = nullptr;
	mem::VariableSizeTable*							g_meshes_table = nullptr;

	void createTables() {
		std::vector<mem::VeMap*> maps = {
			(mem::VeMap*) new mem::VeTypedMap< std::unordered_map<std::string, VeIndex>, std::string, VeIndex >( offsetof(struct VeMainTableEntry, m_name ), 0)
		};
		g_main_table = new mem::VeFixedSizeTypedTable<VeMainTableEntry>( std::move(maps), 0 );
		registerTablePointer(g_main_table, "Main Table");

		maps = {
			(mem::VeMap*) new mem::VeTypedMap< std::unordered_map<std::string, VeIndex>, std::string, VeIndex >(offsetof(struct VeSysTableEntry, m_name), 0)
		};
		g_systems_table = new mem::VeFixedSizeTypedTable<VeSysTableEntry>(std::move(maps), 0);
		registerTablePointer(g_systems_table, "Systems Table");

		VeMainTableEntry entry;
		bool found = g_main_table->getEntryFromMap(0, std::string("Main Table"), entry);

		g_meshes_table = new mem::VariableSizeTable(1 << 20);

	}

	void registerTablePointer(mem::VeTable* tptr, std::string name) {
		VeMainTableEntry entry = { tptr, name };
		g_main_table->addEntry(entry);
	}

	mem::VeTable* getTablePointer(std::string name) {
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
		syseve::initEvents();
		syssce::initScene();
		sysphy::initPhysics();

	}

	void computeOneFrame() {
		for (auto entry : g_systems_table->getData()) entry.m_tick();
	}

	void runGameLoop() {
		while (g_goon) {
			computeOneFrame();
		}
	}

	void closeEngine() {
		for (auto entry : g_systems_table->getData()) entry.m_close();
	}

}

