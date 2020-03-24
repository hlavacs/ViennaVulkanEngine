

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

		testmap.insertIntoMap( &t1, 1 );
		testmap.print();
		testmap.printTree();
		testmap.insertIntoMap(&t2, 0);
		testmap.print();
		testmap.printTree();
		testmap.deleteFromMap(&t2, 0);
		testmap.print();
		testmap.printTree();

		testmap.insertIntoMap(&t3, 0);
		testmap.print();
		testmap.printTree();
		testmap.insertIntoMap(&t3, 1);
		testmap.print();
		testmap.printTree();
		testmap.insertIntoMap(&t3, 2);
		testmap.print();
		testmap.printTree();

		testmap.deleteFromMap(&t3, 1);
		testmap.print();
		testmap.printTree();
		testmap.insertIntoMap(&t3, 1);
		testmap.print();
		testmap.printTree();


		testmap.insertIntoMap(&t5, 0);
		testmap.print();
		testmap.printTree();
		testmap.insertIntoMap(&t6, 0);
		testmap.print();
		testmap.printTree();
		testmap.insertIntoMap(&t4, 0);
		testmap.print();
		testmap.printTree();
		testmap.insertIntoMap(&t4, 1);
		testmap.print();
		testmap.printTree();
		testmap.insertIntoMap(&t4, 2);
		testmap.print();
		testmap.printTree();
		testmap.insertIntoMap(&t4, 3);
		testmap.printTree();

		std::cout << std::endl;

		testmap.deleteFromMap(&t3, 1);
		testmap.printTree();
		testmap.deleteFromMap(&t4, 1);
		testmap.printTree();
		testmap.deleteFromMap(&t4, 2);
		testmap.printTree();

		testmap.insertIntoMap(&t12, 0);
		testmap.insertIntoMap(&t11, 0);
		testmap.insertIntoMap(&t10, 0);
		testmap.printTree();
		testmap.insertIntoMap(&t11, 1);
		testmap.insertIntoMap(&t11, 2);
		testmap.insertIntoMap(&t11, 3);
		testmap.printTree();
		testmap.insertIntoMap(&t7, 0);
		testmap.insertIntoMap(&t7, 1);
		testmap.insertIntoMap(&t7, 2);
		testmap.printTree();

		testmap.deleteFromMap(&t11, 3);
		testmap.deleteFromMap(&t11, 2);
		testmap.deleteFromMap(&t11, 1);
		testmap.deleteFromMap(&t11, 0);
		testmap.printTree();

		testmap.clear();

		for (uint32_t i = 0; i < 1000; ++i) {
			TestEntry t1{ (VeIndex)std::rand(), 1, 2, "A" };
			testmap.insertIntoMap(&t1, i);
		}
		//testmap.printTree();

		VeClock mapClock("Map Clock", 1);
		mapClock.start();
		for (uint32_t i = 0; i < 1000; ++i) {
			//std::cout << "i=" << i << std::endl;

			//TestEntry t1{ i, 1, 2, "B" };
			//testmap.insertIntoMap(&t1, i);
			
			//uint32_t idx = std::rand() % testmap.size();
			auto [key, value] = testmap.getKeyValuePair(i % testmap.size());
			//t1.m_int1 = (VeIndex)key;
			//testmap.deleteFromMap(&t1, value);
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

		testmap2.insertIntoMap(&t1, 1);
		testmap2.insertIntoMap(&t2, 1);
		testmap2.insertIntoMap(&t3, 1);
		testmap2.insertIntoMap(&t4, 1);
		testmap2.insertIntoMap(&t5, 1);
		testmap2.insertIntoMap(&t6, 1);


	}


	void testMap() {
		//testOrderedMap();
		testHashedMap();
	}

}


