/**
*
* \file
* \brief
*
* Details
*
*/

#include "VEDefines.h"
#include "VETable.h"
#include "VESysEngine.h"


namespace vve {


	namespace tab {
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
			TestEntry(VeHandle handle, VeIndex int1, VeIndex int2, std::string name) : m_int64(handle), m_int1(int1), m_int2(int2), m_name(name) {};
		};

		std::vector<VeMap*> maps = {
			new VeTypedMap< std::map<     VeHandle,   VeIndex>, VeHandle, VeIndex >(
				(VeIndex)offsetof(struct TestEntry, m_int64), (VeIndex)sizeof(TestEntry::m_int64)),
			new VeTypedMap< std::multimap<VeHandle,   VeIndex>, VeHandle, VeIndex >(
				(VeIndex)offsetof(struct TestEntry, m_int1), (VeIndex)sizeof(TestEntry::m_int1)),
			new VeTypedMap< std::multimap<std::string, VeIndex>, std::string, VeIndex >(
				(VeIndex)offsetof(struct TestEntry, m_name), 0),
			new VeTypedMap< std::multimap<VeHandlePair, VeIndex>, VeHandlePair, VeIndexPair >(
				VeIndexPair((VeIndex)offsetof(TestEntry, m_int1), (VeIndex)offsetof(TestEntry, m_int2)),
				VeIndexPair((VeIndex)sizeof(TestEntry::m_int1),   (VeIndex)sizeof(TestEntry::m_int2)))
		};
		VeFixedSizeTable<TestEntry> testTable(maps);


		void printEntry(VeHandle handle) {

			TestEntry entry;
			testTable.getEntry(handle, entry);

			auto [auto_id, dir_index] = VeDirectory::splitHandle(handle);

			std::cout << "Entry handle auto_id " << auto_id << " dir_index " << dir_index <<
				" int64 " << entry.m_int64 << " int1 " << entry.m_int1 << " int2 " << entry.m_int2 <<
				" name " << entry.m_name << std::endl;
		}

		void testFixedTables1() {

			VeHandle handle, handle1, handle2;
			handle1 = testTable.addEntry({ 1, 2, 3, "4" });
			handle2 = testTable.addEntry({ 4, 2, 1, "3" });
			testTable.forAllEntries(std::bind(printEntry, std::placeholders::_1)); std::cout << std::endl;
			testTable.swapEntriesByHandle(handle1, handle2);
			testTable.forAllEntries(std::bind(printEntry, std::placeholders::_1)); std::cout << std::endl;

			testTable.sortTableByMap(0);
			testTable.forAllEntries(std::bind(printEntry, std::placeholders::_1)); std::cout << std::endl;

			handle = testTable.addEntry({ 2, 1, 3, "1" });
			handle = testTable.addEntry({ 3, 4, 5, "2" });
			handle = testTable.addEntry({ 5, 4, 2, "3" });
			handle = testTable.addEntry({ 6, 3, 2, "2" });

			testTable.forAllEntries(std::bind(printEntry, std::placeholders::_1)); std::cout << std::endl;

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

			//--------
			std::vector<VeHandle, custom_alloc<VeHandle>> handles(getHeap());
			testTable.getHandlesEqual(0, 5, handles);
			auto [auto_id, dir_index] = VeDirectory::splitHandle(handles[0]);

			handles.clear();
			testTable.getHandlesEqual(2, "2", handles);

			handles.clear();
			//testTable.getHandlesEqual(3, VeHandlePair(1,6), handles);

			//--------

			handles.clear();
			testTable.getHandlesRange(0, 2, 5, handles);

			handles.clear();
			testTable.getHandlesRange(2, "2", "3", handles);

			handles.clear();
			//testTable.getHandlesRange(3, VeHandlePair( 4, 0 ), VeHandlePair( 5, 0 ), handles);


			handle = testTable.addEntry({ 8, 2, 3, "4" });
			handle = testTable.addEntry({ 9, 4, 1, "3" });
			testTable.sortTableByMap(0);
			testTable.forAllEntries(std::bind(printEntry, std::placeholders::_1)); std::cout << std::endl;

			testTable.clear();
			testTable.sortTableByMap(0);
			testTable.forAllEntries(std::bind(printEntry, std::placeholders::_1)); std::cout << std::endl;
		}



		VeVariableSizeTable testVarTable;

		struct testVarTables1 {
			VeHandle h1, h2, h3;
			VeHandle a[100];
		};

		struct testVarTables2 {
			VeHandle h1, h2;
			VeHandle a[1000];
		};

		void testVarTables() {
			testVarTables1 t1;
			testVarTables2 t2;

			VeHandle h1 = testVarTable.insertBlob((uint8_t*)&t1, sizeof(t1));
			VeHandle h2 = testVarTable.insertBlob((uint8_t*)&t2, sizeof(t2));
		}

		void testTables() {
			testFixedTables1();
			testVarTables();
		}
	}

};


