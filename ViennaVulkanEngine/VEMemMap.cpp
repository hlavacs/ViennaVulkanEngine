

#include "VEDefines.h"


namespace vve::map {


	struct TestEntry {
		VeIndex m_int1;
		VeIndex m_int2;
		VeHandle m_handle;
		std::string m_name;
	};

	VeOrderedMultimap<VeHandle, VeIndex> testmap(offsetof(TestEntry, m_int1), sizeof(TestEntry::m_int1));

	void testOrderedMap() {
		TestEntry t1{ 1, 1, 2, "A" };
		TestEntry t2{ 2, 1, 2, "A" };
		TestEntry t3{ 3, 1, 2, "A" };
		TestEntry t4{ 4, 1, 2, "A" };
		TestEntry t5{ 5, 1, 2, "A" };
		TestEntry t6{ 6, 1, 2, "A" };

		TestEntry t7{ 7, 1, 2, "A" };
		TestEntry t8{ 8, 1, 2, "A" };
		TestEntry t9{ 9, 1, 2, "A" };
		TestEntry t10{ 10, 1, 2, "A" };
		TestEntry t11{ 11, 1, 2, "A" };
		TestEntry t12{ 12, 1, 2, "A" };

		testmap.insert( &t1, 1 );
		testmap.print();
		testmap.printTree();
		testmap.insert(&t2, 0);
		testmap.print();
		testmap.printTree();
		testmap.erase(&t2, 0);
		testmap.print();
		testmap.printTree();

		testmap.insert(&t3, 0);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t3, 1);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t3, 2);
		testmap.print();
		testmap.printTree();

		testmap.erase(&t3, 1);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t3, 1);
		testmap.print();
		testmap.printTree();


		testmap.insert(&t5, 0);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t6, 0);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t4, 0);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t4, 1);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t4, 2);
		testmap.print();
		testmap.printTree();
		testmap.insert(&t4, 3);
		testmap.printTree();

		std::cout << std::endl;

		testmap.erase(&t3, 1);
		testmap.printTree();
		testmap.erase(&t4, 1);
		testmap.printTree();
		testmap.erase(&t4, 2);
		testmap.printTree();

		testmap.insert(&t12, 0);
		testmap.insert(&t11, 0);
		testmap.insert(&t10, 0);
		testmap.printTree();
		testmap.insert(&t11, 1);
		testmap.insert(&t11, 2);
		testmap.insert(&t11, 3);
		testmap.printTree();
		testmap.insert(&t7, 0);
		testmap.insert(&t7, 1);
		testmap.insert(&t7, 2);
		testmap.printTree();

		testmap.erase(&t11, 3);
		testmap.erase(&t11, 2);
		testmap.erase(&t11, 1);
		testmap.erase(&t11, 0);
		testmap.printTree();

		testmap.clear();

		for (uint32_t i = 0; i < 1000; ++i) {
			TestEntry t1{ (VeIndex)std::rand(), 1, 2, "A" };
			testmap.insert(&t1, i);
		}
		//testmap.printTree();

		VeClock mapClock("Map Clock", 1);
		mapClock.start();
		for (uint32_t i = 0; i < 1000; ++i) {
			//std::cout << "i=" << i << std::endl;

			//TestEntry t1{ i, 1, 2, "B" };
			//testmap.insert(&t1, i);
			
			//uint32_t idx = std::rand() % testmap.size();
			auto [key, value] = testmap.getKeyValuePair(i % testmap.size());
			//t1.m_int1 = (VeIndex)key;
			//testmap.erase(&t1, value);
		}
		mapClock.stop();

		//testmap.print();
		//testmap.printTree();


	}


	VeHashedMultimap<VeHandle, VeIndex> testmap2(offsetof(TestEntry, m_int1), sizeof(TestEntry::m_int1));

	void testHashedMap() {
		TestEntry t1{ 1, 1, 2, "A" };
		TestEntry t2{ 2, 1, 2, "A" };
		TestEntry t3{ 3, 1, 2, "A" };
		TestEntry t4{ 4, 1, 2, "A" };
		TestEntry t5{ 5, 1, 2, "A" };
		TestEntry t6{ 6, 1, 2, "A" };

		TestEntry t7{ 7, 1, 2, "A" };
		TestEntry t8{ 8, 1, 2, "A" };
		TestEntry t9{ 9, 1, 2, "A" };
		TestEntry t10{ 10, 1, 2, "A" };
		TestEntry t11{ 11, 1, 2, "A" };
		TestEntry t12{ 12, 1, 2, "A" };

		testmap2.insert(&t1, 1);
		testmap2.insert(&t2, 1);
		testmap2.insert(&t3, 1);
		testmap2.insert(&t3, 2);
		testmap2.insert(&t3, 3);
		testmap2.insert(&t3, 4);
		testmap2.insert(&t4, 1);
		testmap2.insert(&t5, 1);
		testmap2.insert(&t6, 1);
		testmap2.insert(&t6, 2);
		testmap2.insert(&t6, 3);

		testmap2.print();

		testmap2.erase(&t1, 1);
		testmap2.erase(&t3, 1);
		testmap2.erase(&t3, 2);

		testmap2.print();

		testmap2.clear();

		constexpr VeIndex NUM = 10000;
		for (uint32_t i = 0; i < NUM; ++i) {
			TestEntry t1{ i, 1, 2, "A" };
			testmap2.insert(&t1, i);
		}
		//testmap2.print();

		VeClock mapClock("Map Clock", 1);
		mapClock.start();
		for (uint32_t i = 0; i < 10000; ++i) {
			//std::cout << "i=" << i << std::endl;

			TestEntry t1{ i+100000, 1, 2, "B" };
			testmap2.insert(&t1, i+100000);

			uint32_t key = std::rand() % testmap2.size();
			auto index = testmap.find(key);

			t1.m_int1 = (VeIndex)key;
			testmap2.erase(&t1, key);
		}
		mapClock.stop();



	}


	void testMap() {
		//testOrderedMap();
		testHashedMap();
	}

}


