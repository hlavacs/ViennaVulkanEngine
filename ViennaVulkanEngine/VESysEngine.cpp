


#include "VEDefines.h"
#include "VETable.h"
#include "VESysEngine.h"
#include "VESysVulkan.h"
#include "VESysWindow.h"
#include "VESysEvents.h"
#include "VESysPhysics.h"
#include "VESysAssets.h"
#include "VESysScene.h"


namespace syseng {

	//-----------------------------------------------------------------------------------

	bool g_goon = true;
	tab::VeFixedSizeTable<VeMainTableEntry>*	g_main_table = nullptr;
	tab::VeFixedSizeTable<VeSysTableEntry>*		g_systems_table = nullptr;
	tab::VeVariableSizeTable*					g_meshes_table = nullptr;

	void createTables() {
		std::vector<tab::VeMap*> maps = {
			(tab::VeMap*) new tab::VeTypedMap< std::unordered_map<tab::VeTableKeyString, tab::VeTableIndex >, tab::VeTableKeyString, tab::VeTableIndex >(
				(VeIndex)offsetof(struct VeMainTableEntry, m_name ), 0)
		};
		g_main_table = new tab::VeFixedSizeTable<VeMainTableEntry>( std::move(maps), 0 );
		registerTablePointer(g_main_table, "Main Table");

		maps = {
			(tab::VeMap*) new tab::VeTypedMap< std::unordered_map<tab::VeTableKeyString, tab::VeTableIndex >, tab::VeTableKeyString, tab::VeTableIndex >(
				(VeIndex)offsetof(struct VeSysTableEntry, m_name), 0)
		};
		g_systems_table = new tab::VeFixedSizeTable<VeSysTableEntry>(std::move(maps), 0);
		registerTablePointer(g_systems_table, "Systems Table");

		std::vector<VeHandle> handles;
		g_main_table->getHandlesFromMap(0, (std::string&)std::string("Main Table"), handles);
		
		VeMainTableEntry entry;
		bool found = g_main_table->getEntry(handles[0], entry);

		g_meshes_table = new tab::VeVariableSizeTable(1 << 20);

	}

	void registerTablePointer(tab::VeTable* tptr, std::string name) {
		VeMainTableEntry entry = { tptr, name };
		g_main_table->addEntry(entry);
	}

	tab::VeTable* getTablePointer(std::string name) {
		const std::vector<VeMainTableEntry>& data = g_main_table->getData();
		for (uint32_t i = 0; i < data.size(); ++i) if (data[i].m_name == name) return data[i].m_table_pointer; 
		return nullptr;
	}


	///public interface

	void initEngine() {
		std::cout << "init engine 2\n";

		tab::testTables();


		createTables();
		syswin::initWindow();
		sysvul::initVulkan();
		syseve::initEvents();
		sysass::initAssets();
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

