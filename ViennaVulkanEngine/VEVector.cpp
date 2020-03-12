/**
*
* \file
* \brief
*
* Details
*
*/


#include "VEDefines.h"


namespace vve {

	uint64_t alignBoundary(uint64_t size, VeIndex alignment) {
		uint64_t result = size;
		if (alignment > 1) {
			uint64_t mod = result & (alignment - 1);
			if (mod > 0) result += alignment - mod;
		}
		return result;
	}


	namespace vec {

		struct TestEntry {
			VeHandle	m_int64;
			VeIndex		m_int1;
			VeIndex		m_int2;
			std::string m_name;
			TestEntry() : m_int64(VE_NULL_HANDLE), m_int1(VE_NULL_INDEX), m_int2(VE_NULL_INDEX), m_name("") {};
			TestEntry(VeHandle handle, VeIndex int1, VeIndex int2, std::string& name) :
				m_int64(handle), m_int1(int1), m_int2(int2), m_name(name) {};
			TestEntry(VeHandle handle, VeIndex int1, VeIndex int2, std::string&& name) :
				m_int64(handle), m_int1(int1), m_int2(int2), m_name(name) {};

			TestEntry& operator=(const TestEntry& entry) {
				m_int64 = entry.m_int64;
				m_int1 = entry.m_int1;
				m_int2 = entry.m_int2;
				m_name = entry.m_name;
				return *this;
			};

		};

		void printEntry( TestEntry &entry) {
			std::cout << 
				" int64 " << entry.m_int64 << " int1 " << entry.m_int1 << " int2 " << entry.m_int2 <<
				" name " << entry.m_name << std::endl;
		}

		void testVector1() {
			VeVector<TestEntry> vec;

			vec.push_back({ 1, 2, 3, "4" });
			vec.push_back({ 4, 2, 1, "3" });
			vec.push_back({ 2, 1, 3, "1" });
			vec.push_back({ 3, 4, 5, "2" });
			vec.push_back({ 5, 4, 2, "3" });
			vec.push_back({ 6, 3, 2, "2" });
			vec.push_back({ 5, 4, 2, "3" });
			vec.push_back({ 6, 3, 2, "2" });
			vec.push_back({ 1, 2, 3, "4" });
			vec.push_back({ 4, 2, 1, "3" });
			vec.push_back({ 2, 1, 3, "1" });
			vec.push_back({ 3, 4, 5, "2" });
			vec.push_back({ 5, 4, 2, "3" });
			vec.push_back({ 6, 3, 2, "2" });
			vec.push_back({ 5, 4, 2, "3" });
			vec.push_back({ 6, 3, 2, "2" });
			for (auto entry : vec) { printEntry(entry); } std::cout << std::endl;

			vec.push_back({ 1, 2, 3, "4" });
			for (auto entry : vec) { printEntry(entry); } std::cout << std::endl;

			VeVector<TestEntry> vec2(vec);
			for (auto entry : vec2) { printEntry(entry); } std::cout << std::endl;

			VeVector<TestEntry> vec3;
			vec3.push_back({ 3, 4, 5, "2" });
			vec3.push_back({ 5, 4, 2, "3" });
			vec3.push_back({ 6, 3, 2, "2" });
			vec3.push_back({ 5, 4, 2, "3" });
			vec3.push_back({ 6, 3, 2, "2" });
			for (auto entry : vec3) { printEntry(entry); } std::cout << std::endl;

			vec3.pop_back();
			vec3.pop_back();
			vec3.pop_back();
			vec3.pop_back();
			for (auto entry : vec3) { printEntry(entry); } std::cout << std::endl;

			vec = vec2;
			for (auto entry : vec) { printEntry(entry); } std::cout << std::endl;
			vec = vec3;
			for (auto entry : vec) { printEntry(entry); } std::cout << std::endl;

			vec.clear();
			for (auto entry : vec) { printEntry(entry); } std::cout << std::endl;



		}

		struct TestEntry2 {
			VeHandle	m_int64;
			VeIndex		m_int1;
			VeIndex		m_int2;
			TestEntry2() : m_int64(VE_NULL_HANDLE), m_int1(VE_NULL_INDEX), m_int2(VE_NULL_INDEX) {};
			TestEntry2(VeHandle handle, VeIndex int1, VeIndex int2) : m_int64(handle), m_int1(int1), m_int2(int2) {};

			TestEntry2& operator=(const TestEntry& entry) {
				m_int64 = entry.m_int64;
				m_int1 = entry.m_int1;
				m_int2 = entry.m_int2;
				return *this;
			};

		};


		void printEntry2(TestEntry2& entry) {
			std::cout << " int64 " << entry.m_int64 << " int1 " << entry.m_int1 << " int2 " << entry.m_int2 << std::endl;
		}

		void testVector2() {
			VeVector<TestEntry2> vec( true );

			vec.push_back({ 1, 2, 3 });
			vec.push_back({ 2, 2, 1 });
			vec.push_back({ 4, 2, 3 });
			vec.push_back({ 1, 3, 3 });
			vec.push_back({ 1, 2, 2 });
			for (auto entry : vec) { printEntry2(entry); } std::cout << std::endl;

			VeVector<TestEntry2> vec2(vec);
			vec2.push_back({ 6, 3, 3 });
			vec2.push_back({ 7, 2, 2 });
			for (auto entry : vec2) { printEntry2(entry); } std::cout << std::endl;

			VeVector<TestEntry2> vec3(true);
			vec3 = vec2;
			for (auto entry : vec3) { printEntry2(entry); } std::cout << std::endl;

			vec3.pop_back();
			vec3.pop_back();
			vec3.pop_back();
			vec3.pop_back();
			for (auto entry : vec3) { printEntry2(entry); } std::cout << std::endl;

		}

		void testVector() {
			testVector1();
			testVector2();

			VeHeapMemory mem;

			std::vector < VeHandle, custom_alloc<VeHandle> > handles( &mem );
			handles.push_back(1);
			mem.print();
			handles.push_back(2);
			handles.push_back(3);
			handles.push_back(4);
			mem.print();
			handles.pop_back();
			handles.pop_back();
			handles.pop_back();
			mem.print();

			handles.push_back(5);
			handles.push_back(6);
			handles.push_back(7);
			handles.push_back(8);
			mem.print();
			handles.clear();
			mem.print();

			for (uint32_t i = 0; i < 1000; ++i) {
				handles.push_back(std::rand());
				if (i % 10 == 0) {
					std::cout << "    i " << i << std::endl;
					mem.print();
				}
			}
			mem.print();
			handles.clear();
			mem.print();

			for (uint32_t i = 0; i < 3000; ++i) {
				handles.push_back(std::rand());
			}
			mem.print();


		}

	}


}




