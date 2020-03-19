

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
		TestEntry t1{ 0, 1, 2, "A" };
		TestEntry t2{ 5, 1, 2, "A" };
		TestEntry t3{ 2, 1, 2, "A" };
		TestEntry t4{ 9, 1, 2, "A" };
		TestEntry t5{ 1, 1, 2, "A" };
		TestEntry t6{ 2, 1, 2, "A" };

		testmap.insertIntoMap( &t1, 0 );
		testmap.insertIntoMap(&t2, 0);
		testmap.insertIntoMap(&t3, 0);
		testmap.insertIntoMap(&t3, 1);
		testmap.insertIntoMap(&t3, 2);
		testmap.insertIntoMap(&t4, 0);
		testmap.insertIntoMap(&t4, 1);
		testmap.insertIntoMap(&t4, 2);
		testmap.insertIntoMap(&t4, 2);
		testmap.insertIntoMap(&t5, 0);
		testmap.insertIntoMap(&t6, 0);

		testmap.deleteFromMap(&t3, 1);
		testmap.deleteFromMap(&t4, 1);
		testmap.deleteFromMap(&t4, 2);


	}


}


