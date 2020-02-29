
#include <iostream>
#include <functional>

#include "VEInclude.h"
#include "jobSystemTest.h"
#include "STLTest.h"


int main()
{
    std::cout << "Hello World!\n";

    
    JADD( jst::jobSystemTest() );
#ifdef VE_ENABLE_MULTITHREADING
    vgjs::JobSystem::getInstance()->wait();
    vgjs::JobSystem::getInstance()->terminate();
    vgjs::JobSystem::getInstance()->waitForTermination();
#endif
    return 0;

    //vec::testVector();
    //tab::testTables();
    //stltest::runSTLTests();
    //return 0;

    vve::syseng::init();
    vve::syseng::computeOneFrame();
    vve::syseng::close();
 
    return 0;
}

