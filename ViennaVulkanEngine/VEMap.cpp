

#include "VEDefines.h"


namespace vve::map {


	struct TestEntry {
		VeIndex m_int1;
		VeIndex m_int2;
		VeHandle m_handle;
		std::string m_name;
	};

	VeOrderedMultimap<VeHandle, VeIndex> testmap(offsetof(TestEntry, m_int1), sizeof(TestEntry::m_int1));

	void testMap() {
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
		std::cout << std::endl;
		testmap.insertIntoMap(&t2, 0);
		testmap.print();
		std::cout << std::endl;
		testmap.deleteFromMap(&t2, 0);
		testmap.print();
		std::cout << std::endl;

		testmap.insertIntoMap(&t3, 0);
		testmap.insertIntoMap(&t3, 1);
		testmap.insertIntoMap(&t3, 2);
		testmap.insertIntoMap(&t5, 0);
		testmap.insertIntoMap(&t6, 0);
		testmap.insertIntoMap(&t4, 0);
		testmap.insertIntoMap(&t4, 1);
		testmap.insertIntoMap(&t4, 2);
		testmap.insertIntoMap(&t4, 3);
		testmap.print();
		testmap.printTree();

		std::cout << std::endl;

		testmap.deleteFromMap(&t3, 1);
		testmap.print();
		testmap.printTree();
		testmap.deleteFromMap(&t4, 1);
		testmap.printTree();
		testmap.deleteFromMap(&t4, 2);

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


	}


}


