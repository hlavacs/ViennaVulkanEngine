

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

		JADD(testFunction1(1));
		JADD(testFunction1(2));
		JADD(testFunction1(3));
		JADD(testFunction1(4));

		JDEP(testFunction2(5));

	}


}



