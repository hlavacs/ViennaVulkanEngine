

#include "VEDefines.h"
#include "jobSystemTest.h"



namespace jst {


	void testFunction1( uint32_t i) {
		std::cout << i << std::endl;
	}

	void testFunction2(uint32_t i) {
		std::cout << "End " << i << std::endl << std::endl;
	}




	void jobSystemTest() {

		JCHILD(testFunction1(1));
		JCHILD(testFunction1(2));
		JCHILD(testFunction1(3));
		JCHILD(testFunction1(4));

		JDEP(testFunction2(5));

	}


}



