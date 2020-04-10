

#include "VEDefines.h"


namespace vve::map {


	struct TestEntry {
		VeIndex m_int1;
		VeIndex m_int2;
		VeHandle m_handle;
		std::string m_name;
	};

	VeOrderedMultimap<VeKey, VeIndex> testmap(offsetof(TestEntry, m_int1), sizeof(TestEntry::m_int1));

	void testOrderedMap() {
		TestEntry t1{ 1, 1, 2_Hd, "A" };
		TestEntry t2{ 2, 1, 2_Hd, "A" };
		TestEntry t3{ 3, 1, 2_Hd, "A" };
		TestEntry t4{ 4, 1, 2_Hd, "A" };
		TestEntry t5{ 5, 1, 2_Hd, "A" };
		TestEntry t6{ 6, 1, 2_Hd, "A" };

		TestEntry t7{ 7, 1, 2_Hd, "A" };
		TestEntry t8{ 8, 1, 2_Hd, "A" };
		TestEntry t9{ 9, 1, 2_Hd, "A" };
		TestEntry t10{ 10, 1, 2_Hd, "A" };
		TestEntry t11{ 11, 1, 2_Hd, "A" };
		TestEntry t12{ 12, 1, 2_Hd, "A" };

		testmap.insert( &t1, 1_Va );
		testmap.print();
		testmap.printTree();
		testmap.insert(&t2, 0_Va);
		testmap.print();
		testmap.printTree();
		testmap.erase(&t2, 0_Va);
		testmap.print();
		testmap.printTree();

		testmap.insert(&t3, 0_Va);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t3, 1_Va);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t3, 2_Va);
		testmap.print();
		testmap.printTree();

		testmap.erase(&t3, 1_Va);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t3, 1_Va);
		testmap.print();
		testmap.printTree();


		testmap.insert(&t5, 0_Va);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t6, 0_Va);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t4, 0_Va);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t4, 1_Va);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t4, 2_Va);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t4, 3_Va);
		testmap.printTree();

		std::cout << std::endl;

		testmap.erase(&t3, 1_Va);
		testmap.printTree();
		testmap.erase(&t4, 1_Va);
		testmap.printTree();
		testmap.erase(&t4, 2_Va);
		testmap.printTree();

		testmap.insert(&t12, 0_Va);
		testmap.insert(&t11, 0_Va);
		testmap.insert(&t10, 0_Va);
		testmap.printTree();
		testmap.insert(&t11, 1_Va);
		testmap.insert(&t11, 2_Va);
		testmap.insert(&t11, 3_Va);
		testmap.printTree();
		testmap.insert(&t7, 0_Va);
		testmap.insert(&t7, 1_Va);
		testmap.insert(&t7, 2_Va);
		testmap.printTree();

		testmap.erase(&t11, 3_Va);
		testmap.erase(&t11, 2_Va);
		testmap.erase(&t11, 1_Va);
		testmap.erase(&t11, 0_Va);
		testmap.printTree();

		testmap.clear();

		for (uint32_t i = 0; i < 1000; ++i) {
			TestEntry t1{ (VeIndex)std::rand(), 1, 2_Hd, "A" };
			testmap.insert(&t1, VeValue(i));
		}
		//testmap.printTree();

		VeClock mapClock("Map Clock", 1);
		mapClock.start();
		for (uint32_t i = 0; i < 1000; ++i) {
			//std::cout << "i=" << i << std::endl;

			//TestEntry t1{ i, 1, 2, "B" };
			//testmap.insert(&t1, i);
			
			//uint32_t idx = std::rand() % testmap.size();
			auto [key, value] = testmap.getKeyValuePair(i % (uint64_t)testmap.size());
			//t1.m_int1 = (VeIndex)key;
			//testmap.erase(&t1, value);
		}
		mapClock.stop();

		//testmap.print();
		//testmap.printTree();


	}


	VeHashedMultimap<VeKey, VeIndex> testmap2(offsetof(TestEntry, m_int1), sizeof(TestEntry::m_int1));

	void testHashedMap() {
		TestEntry t1{ 1, 1, 2_Hd, "A" };
		TestEntry t2{ 2, 1, 2_Hd, "A" };
		TestEntry t3{ 3, 1, 2_Hd, "A" };
		TestEntry t4{ 4, 1, 2_Hd, "A" };
		TestEntry t5{ 5, 1, 2_Hd, "A" };
		TestEntry t6{ 6, 1, 2_Hd, "A" };

		TestEntry t7{ 7, 1, 2_Hd, "A" };
		TestEntry t8{ 8, 1, 2_Hd, "A" };
		TestEntry t9{ 9, 1, 2_Hd, "A" };
		TestEntry t10{ 10, 1, 2_Hd, "A" };
		TestEntry t11{ 11, 1, 2_Hd, "A" };
		TestEntry t12{ 12, 1, 2_Hd, "A" };

		testmap2.insert(&t1, 1_Va);
		testmap2.insert(&t2, 1_Va);
		testmap2.insert(&t3, 1_Va);
		testmap2.insert(&t3, 2_Va);
		testmap2.insert(&t3, 3_Va);
		testmap2.insert(&t3, 4_Va);
		testmap2.insert(&t4, 1_Va);
		testmap2.insert(&t5, 1_Va);
		testmap2.insert(&t6, 1_Va);
		testmap2.insert(&t6, 2_Va);
		testmap2.insert(&t6, 3_Va);

		testmap2.print();

		testmap2.erase(&t1, 1_Va);
		testmap2.erase(&t3, 1_Va);
		testmap2.erase(&t3, 2_Va);

		testmap2.print();

		testmap2.clear();

		constexpr VeIndex NUM = 10000;
		for (uint32_t i = 0; i < NUM; ++i) {
			TestEntry t1{ i, 1, 2_Hd, "A" };
			testmap2.insert(&t1, VeValue(i));
		}
		//testmap2.print();

		VeClock mapClock("Map Clock", 1);
		mapClock.start();
		for (uint32_t i = 0; i < 10000; ++i) {
			//std::cout << "i=" << i << std::endl;

			TestEntry t1{ i+100000, 1, 2_Hd, "B" };
			testmap2.insert(&t1, VeValue(i+100000));

			uint32_t key = std::rand() % (uint64_t)testmap2.size();
			auto index = testmap2.find(VeKey(key));

			t1.m_int1 = (VeIndex)key;
			testmap2.erase(&t1, VeValue(key));
		}
		mapClock.stop();



	}


	void testMap() {
		//testOrderedMap();
		testHashedMap();
	}

}


