


#include "VEDefines.h"
#include "VESysEngine.h"
#include "VESysVulkan.h"
#include "VESysWindow.h"
#include "VESysEvents.h"
#include "VESysPhysics.h"
#include "VESysAssets.h"
#include "VESysScene.h"


namespace vve::syseng {

	//-----------------------------------------------------------------------------------

	struct VeMainTableEntry {
		VeTable*		m_table_pointer;
		std::string		m_name;
	};

	struct VeSysTableEntry {
		std::function<void()>	m_init;
		std::function<void()>	m_tick;
		std::function<void()>	m_close;
	};

	std::atomic<bool> g_loop = false;
	std::atomic<bool> g_goon = true;
	VeFixedSizeTable<VeMainTableEntry>*	g_main_table = nullptr;
	VeFixedSizeTable<VeSysTableEntry>*	g_systems_table = nullptr;

	void registerTablePointer(VeTable* tptr, std::string &&name) {
		g_main_table->addEntry({ tptr, name });
	}

	void registerSystem(std::function<void()>&& init, std::function<void()>&& tick, std::function<void()>&& close) {
		g_systems_table->addEntry( { init, tick, close } );
	}

	void createTables() {
		std::vector<VeMap*> maps = {
			(VeMap*) new VeTypedMap< std::unordered_map<VeTableKeyString, VeTableIndex >, VeTableKeyString, VeTableIndex >(
				(VeIndex)offsetof(struct VeMainTableEntry, m_name ), 0)
		};
		g_main_table = new VeFixedSizeTable<VeMainTableEntry>( std::move(maps), 0 );
		registerTablePointer(g_main_table, "Tables");

		g_systems_table = new VeFixedSizeTable<VeSysTableEntry>();
		registerTablePointer(g_systems_table, "Systems");
	}

	VeTable* getTablePointer(std::string name) {
		auto data = g_main_table->getData();
		for (uint32_t i = 0; i < data.size(); ++i) 
			if (data[i].m_name == name) return data[i].m_table_pointer; 
		return nullptr;
	}


	///public interface

	void init() {
		std::cout << "init engine 2\n";

		createTables();
		registerSystem(syswin::init, syswin::tick, syswin::close);
		registerSystem(sysvul::init, sysvul::tick, sysvul::close);
		registerSystem(syseve::init, syseve::tick, syseve::close);
		registerSystem(sysass::init, sysass::tick, sysass::close);
		registerSystem(syssce::init, syssce::tick, syssce::close);
		registerSystem(sysphy::init, sysphy::tick, sysphy::close);

		for (auto entry : g_systems_table->getData()) 
			entry.m_init();
	}

	void computeOneFrame() {
		for (auto entry : g_systems_table->getData()) entry.m_tick();
	}

	void runGameLoop() {
		g_loop = true;
		while (g_goon) {
			computeOneFrame();
		}
		g_loop = false;
		close();
	}

	void close() {
		if (g_loop) {
			g_goon = false;
			return;
		}
		for (auto entry : g_systems_table->getData()) 
			entry.m_close();
	}

}

