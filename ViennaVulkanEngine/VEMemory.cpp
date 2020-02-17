



#include "VEDefines.h"
#include "VEMemory.h"


namespace mem {


	struct TestEntry {
		VeHandle handle;
		VeIndex int1;
		VeIndex int2;
		std::string name;
	};

	void printEntry( TestEntry *pentry) {
		auto [auto_id, dir_index] = VeDirectory::splitHandle(pentry->handle);

		std::cout << "Entry handle auto id" << auto_id << " dir index " << dir_index << 
			" int1 " << pentry->int1 << " int2 " << pentry->int2 << " name " << pentry->name << std::endl;
	}

	void testTables() {

		std::vector<mem::VeMap*> maps = {
			(mem::VeMap*) new mem::VeTypedMap< std::map<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >(
				(VeIndex)offsetof(struct TestEntry, handle), (VeIndex)sizeof(TestEntry::handle)),
			(mem::VeMap*) new mem::VeTypedMap< std::map<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >(
				(VeIndex)offsetof(struct TestEntry, int1), (VeIndex)sizeof(TestEntry::int1)),
			(mem::VeMap*) new mem::VeTypedMap< std::map<VeTableKeyIntPair, VeTableIndex>, VeTableKeyIntPair, VeTableIndexPair >(
				VeTableIndexPair((VeIndex)offsetof(struct TestEntry, int1),
								 (VeIndex)offsetof(struct TestEntry, int2)),
				VeTableIndexPair((VeIndex)sizeof(TestEntry::int1), (VeIndex)sizeof(TestEntry::int2)))
		};
		VeFixedSizeTypedTable<TestEntry> testTable( maps );



	}


};


