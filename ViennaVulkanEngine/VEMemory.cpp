



#include "VEDefines.h"
#include "VEMemory.h"


namespace mem {


	constexpr uint32_t num_repeats = 200;
	constexpr uint32_t n = 1000;
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(1, 1 << 30);


	struct TestEntry {
		VeHandle	m_int64;
		VeIndex		m_int1;
		VeIndex		m_int2;
		std::string m_name;
		TestEntry() : m_int64(VE_NULL_HANDLE), m_int1(VE_NULL_INDEX), m_int2(VE_NULL_INDEX), m_name("") {};
		TestEntry( VeHandle handle, VeIndex int1, VeIndex int2, std::string name ): m_int64(handle), m_int1(int1), m_int2(int2), m_name(name) {};
	};

	std::vector<mem::VeMap*> maps = {
		(mem::VeMap*) new mem::VeTypedMap< std::map<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >(
			(VeIndex)offsetof(struct TestEntry, m_int64), (VeIndex)sizeof(TestEntry::m_int64)),
		(mem::VeMap*) new mem::VeTypedMultimap< std::multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >(
			(VeIndex)offsetof(struct TestEntry, m_int1), (VeIndex)sizeof(TestEntry::m_int1)),
		(mem::VeMap*) new mem::VeTypedMultimap< std::multimap<VeTableKeyString, VeTableIndex>, VeTableKeyString, VeTableIndex >(
			(VeIndex)offsetof(struct TestEntry, m_name), 0),
		(mem::VeMap*) new mem::VeTypedMultimap< std::multimap<VeTableKeyIntPair, VeTableIndex>, VeTableKeyIntPair, VeTableIndexPair >(
			VeTableIndexPair((VeIndex)offsetof(struct TestEntry, m_int1), (VeIndex)offsetof(struct TestEntry, m_int2)),
			VeTableIndexPair((VeIndex)sizeof(TestEntry::m_int1), (VeIndex)sizeof(TestEntry::m_int2)))
	};
	VeFixedSizeTypedTable<TestEntry> testTable( maps );


	void printEntry( VeHandle handle) {

		TestEntry entry;
		testTable.getEntry(handle, entry);

		auto [auto_id, dir_index] = VeDirectory::splitHandle(handle);

		std::cout << "Entry handle auto_id " << auto_id << " dir_index " << dir_index << 
			" int64 " << entry.m_int64 << " int1 " << entry.m_int1 << " int2 " << entry.m_int2 << 
			" name " << entry.m_name << std::endl;
	}

	void testTables() {

		VeHandle handle;
		handle = testTable.addEntry({ 1, 2, 3, "4" } );
		handle = testTable.addEntry({ 4, 2, 1, "3" });
		handle = testTable.addEntry({ 2, 1, 3, "1" });
		handle = testTable.addEntry({ 3, 4, 5, "2" });
		handle = testTable.addEntry({ 5, 4, 2, "3" });
		handle = testTable.addEntry({ 6, 3, 2, "2" });

		testTable.forAllEntries( std::bind( printEntry, std::placeholders::_1) ); std::cout << std::endl;

		testTable.sortTableByMap(0);
		testTable.forAllEntries(std::bind(printEntry, std::placeholders::_1)); std::cout << std::endl;

		testTable.sortTableByMap(1);
		testTable.forAllEntries(std::bind(printEntry, std::placeholders::_1)); std::cout << std::endl;

		testTable.sortTableByMap(2);
		testTable.forAllEntries(std::bind(printEntry, std::placeholders::_1)); std::cout << std::endl;

		testTable.sortTableByMap(3);
		testTable.forAllEntries(std::bind(printEntry, std::placeholders::_1)); std::cout << std::endl;

		testTable.deleteEntry(handle);
		testTable.forAllEntries(std::bind(printEntry, std::placeholders::_1)); std::cout << std::endl;

		handle = testTable.addEntry({ 7, 1, 6, "5" });
		testTable.forAllEntries(std::bind(printEntry, std::placeholders::_1)); std::cout << std::endl;

		testTable.sortTableByMap(3);
		testTable.forAllEntries(std::bind(printEntry, std::placeholders::_1)); std::cout << std::endl;

		testTable.deleteEntry(testTable.getHandleFromIndex(2));
		testTable.deleteEntry(testTable.getHandleFromIndex(3));
		testTable.sortTableByMap(3);
		testTable.forAllEntries(std::bind(printEntry, std::placeholders::_1)); std::cout << std::endl;




	}


};


