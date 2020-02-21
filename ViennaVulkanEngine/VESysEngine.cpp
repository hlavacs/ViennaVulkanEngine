


#include "VEDefines.h"
#include "VEVector.h"
#include "VETable.h"
#include "VESysEngine.h"
#include "VESysVulkan.h"
#include "VESysWindow.h"
#include "VESysEvents.h"
#include "VESysPhysics.h"
#include "VESysAssets.h"
#include "VESysScene.h"


namespace vve::syseng {

	//-----------------------------------------------------------------------------------

	bool g_goon = true;
	VeFixedSizeTable<VeMainTableEntry>*	g_main_table = nullptr;
	VeFixedSizeTable<VeSysTableEntry>*		g_systems_table = nullptr;
	VeVariableSizeTable*					g_meshes_table = nullptr;

	void createTables() {
		std::vector<VeMap*> maps = {
			(VeMap*) new VeTypedMap< std::unordered_map<VeTableKeyString, VeTableIndex >, VeTableKeyString, VeTableIndex >(
				(VeIndex)offsetof(struct VeMainTableEntry, m_name ), 0)
		};
		g_main_table = new VeFixedSizeTable<VeMainTableEntry>( std::move(maps), 0 );
		registerTablePointer(g_main_table, "Main Table");

		maps = {
			(VeMap*) new VeTypedMap< std::unordered_map<VeTableKeyString, VeTableIndex >, VeTableKeyString, VeTableIndex >(
				(VeIndex)offsetof(struct VeSysTableEntry, m_name), 0)
		};
		g_systems_table = new VeFixedSizeTable<VeSysTableEntry>(std::move(maps), 0);
		registerTablePointer(g_systems_table, "Systems Table");

		std::vector<VeHandle> handles;
		g_main_table->getHandlesFromMap(0, (std::string&)std::string("Main Table"), handles);
		
		VeMainTableEntry entry;
		bool found = g_main_table->getEntry(handles[0], entry);

		g_meshes_table = new VeVariableSizeTable(1 << 20);

	}

	void registerTablePointer(VeTable* tptr, std::string name) {
		VeMainTableEntry entry = { tptr, name };
		g_main_table->addEntry(entry);
	}

	VeTable* getTablePointer(std::string name) {
		auto data = g_main_table->getData();
		for (uint32_t i = 0; i < data.size(); ++i) 
			if (data[i].m_name == name) return data[i].m_table_pointer; 
		return nullptr;
	}


	///public interface

	void initEngine() {
		std::cout << "init engine 2\n";

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

