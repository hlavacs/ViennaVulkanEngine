

#include "VEDefines.h"
#include "VESysEngine.h"
#include "VESysAssets.h"


namespace vve::sysass {

	VeVariableSizeTableMT* g_meshes_table = nullptr;
	VeVariableSizeTableMT* g_meshes_table2 = nullptr;


	void init() {
		g_meshes_table = new VeVariableSizeTableMT(1 << 20);
		g_meshes_table2 = new VeVariableSizeTableMT( *g_meshes_table );	//companion table

		syseng::registerTablePointer(g_meshes_table, "Meshes");
	}

	void tick() {
	}

	void close() {
		delete g_meshes_table;
		delete g_meshes_table2;
	}

}


