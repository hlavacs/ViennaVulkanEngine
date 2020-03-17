

#include "VEInclude.h"
#include "jobSystemTest.h"



namespace jst {

	using namespace std::chrono_literals;

	double g_res = 0;


	// relation between number of jobs and depth of DAG
	//how does the depth of the DAG influence scheduling time?

	void dummy() {
		double a = 0;
		auto now = std::chrono::high_resolution_clock::now();
		g_res += sin(std::chrono::duration(std::chrono::high_resolution_clock::now() - now).count());
	}

	void testFunction2(uint32_t i) {
		dummy();
	}

	void testFunction1( uint32_t i, uint32_t  k) {
		for (uint32_t j = 0; j < i; ++j) {
			JADD( testFunction2(j) );
		}
		if (k > 1)
			JDEP( testFunction1(i, k-1) );
	}

	vve::VeClock clock("Vgjs", 1);

	void testFunction0(uint32_t l, uint32_t i, uint32_t  k) {
		clock.start();

		JADD( testFunction1(i,k));

		if (l > 1) {
#ifdef VE_ENABLE_MULTITHREADING
			vgjs::JobSystem::getInstance()->resetPool();
#endif
			JDEP(clock.stop(); testFunction0(l - 1, i, k));
		}
	}

	//how long does it take to schedule 1 job ?

	vve::VeClock schedClock("Scheduling Clock", 500);

	void job() {
		//schedClock.stop();
	}

	using namespace std::placeholders;

	void jobClock(uint32_t j);

	void testScheduling( uint32_t j) {
		static std::function<void()> jobF( job );
		static auto instance = vgjs::JobSystem::getInstance();

		instance->resetPool();

		schedClock.start();
		JADD(job());
		schedClock.stop();

		if (j > 1) {
			JDEP( std::this_thread::sleep_for(std::chrono::milliseconds(1)); testScheduling(j - 1) );
		}
		else {
			//JDEP(testFunction0(20, 11000, 1));
		}
	}

	void jobClock( uint32_t j) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
		testScheduling(j - 1);
	}
	
	void warmUp() {
		//for (uint32_t q = 0; q < 30000; ++q)
		//	JADD(dummy());

		JDEP(testScheduling(5000));
	}


	void jobSystemTest() {
		vgjs::JobSystem::getInstance();
		std::this_thread::sleep_for(3s);

		JADD(warmUp());

	}

}



