/**
*
* \file
* \brief
*
* Details
*
*/

#include "VEDefines.h"
#include "VESysMessages.h"
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
			void print() {
				std::cout << " int64 " << m_int64+0 << " int1 " << m_int1 << " int2 " << m_int2 << " name " << m_name << std::endl;
			};
		};

		std::vector<VeMap*> maps = {
			new VeOrderedMultimap< VeHandle, VeIndex >((VeIndex)offsetof(struct TestEntry, m_int64), (VeIndex)sizeof(TestEntry::m_int64)),
			new VeOrderedMultimap< VeHandle, VeIndex >((VeIndex)offsetof(struct TestEntry, m_int1), (VeIndex)sizeof(TestEntry::m_int1)),
			new VeOrderedMultimap< std::string, VeIndex >((VeIndex)offsetof(struct TestEntry, m_name), 0),
			new VeOrderedMultimap< VeHandlePair, VeIndexPair >(
				VeIndexPair((VeIndex)offsetof(TestEntry, m_int1), (VeIndex)offsetof(TestEntry, m_int2)),
				VeIndexPair((VeIndex)sizeof(TestEntry::m_int1),   (VeIndex)sizeof(TestEntry::m_int2)))
		};
		VeFixedSizeTable<TestEntry> testTable("Test Table", maps);


		void printEntry(VeHandle handle) {

			TestEntry entry;
			testTable.getEntry(handle, entry);

			auto [auto_id, dir_index] = VeDirectory::splitHandle(handle);

			std::cout << "Entry handle auto_id " << auto_id << " dir_index " << dir_index <<
				" int64 " << entry.m_int64+0 << " int1 " << entry.m_int1 << " int2 " << entry.m_int2 <<
				" name " << entry.m_name << std::endl;
		}

		void print1( ) {
			for (auto entry : testTable.data()) {
				entry.print();
			}
		}

		void testFixedTables1() {

			VeHandle handle, handle1, handle2;
			handle1 = testTable.insert({ 1_Hd, 2, 3, "4" });
			handle2 = testTable.insert({ 4_Hd, 2, 1, "3" });
			print1();
			testTable.swap(handle1, handle2);
			print1();

			testTable.sort(0);
			print1();

			handle = testTable.insert({ 2_Hd, 1, 3, "1" });
			handle = testTable.insert({ 3_Hd, 4, 5, "2" });
			handle = testTable.insert({ 5_Hd, 4, 2, "3" });
			handle = testTable.insert({ 6_Hd, 3, 2, "2" });

			print1();

			testTable.sort(0);
			print1();

			testTable.sort(1);
			print1();

			testTable.sort(2);
			print1();

			testTable.sort(3);
			print1();

			testTable.erase(handle);
			print1();

			handle = testTable.insert({ 7_Hd, 1, 6, "5" });
			print1();

			testTable.sort(3);
			print1();

			testTable.erase(testTable.getHandleFromIndex(2));
			testTable.erase(testTable.getHandleFromIndex(3));
			testTable.sort(3);
			print1();

			//--------
			std::vector<VeHandle, custom_alloc<VeHandle>> handles(getTmpHeap());
			testTable.getHandlesEqual(0_Hd, 5, handles);
			auto [auto_id, dir_index] = VeDirectory::splitHandle(handles[0]);

			handles.clear();
			testTable.getHandlesEqual("2", 2, handles);

			handles.clear();
			testTable.getHandlesEqual(VeHandlePair(1_Hd,6_Hd), 3, handles);

			//--------

			handles.clear();
			testTable.getHandlesRange(2_Hd, 5_Hd, 0, handles);

			handles.clear();
			testTable.getHandlesRange("2", "3", 2, handles);

			handles.clear();
			testTable.getHandlesRange(VeHandlePair( 4_Hd, 0_Hd), VeHandlePair( 5_Hd, 0_Hd), 3, handles);


			handle = testTable.insert({ 8_Hd, 2, 3, "4" });
			handle = testTable.insert({ 9_Hd, 4, 1, "3" });
			testTable.sort(0);
			print1();

			testTable.clear();
			testTable.sort(0);
			print1();
		}



		VeVariableSizeTable testVarTable("Test Var Table");

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


