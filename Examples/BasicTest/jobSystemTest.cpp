

#include "VEDefines.h"
#include "jobSystemTest.h"



namespace jst {

	void testFunction1( uint32_t i) {
		std::cout << " " << i << " " << std::endl;
	}

	void testFunction2(uint32_t i) {
		std::cout << "End " << i << std::endl << std::endl;
	}

	void jobSystemTest() {

		uint32_t i = 0;
		for( ; i<30; ++i) 
			JADD(testFunction1(i));

		JDEPT( testFunction2(i+1) , vgjs::TID(0,1) );

	}

}



