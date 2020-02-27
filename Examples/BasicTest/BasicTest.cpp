
#include <iostream>
#include <functional>

#include "jobSystemTest.h"
#include "STLTest.h"
#include "VEInclude.h"

using namespace vve;

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


    syseng::init();
    syseng::runGameLoop();
    syseng::close();
 
    return 0;
}

