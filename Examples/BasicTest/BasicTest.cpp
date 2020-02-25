
#include <iostream>
#include <functional>

#include "STLTest.h"

#include "VEInclude.h"

using namespace vve;

int main()
{
    std::cout << "Hello World!\n";

    vec::testVector();
    tab::testTables();
    //stltest::runSTLTests();
    return 0;

    syseng::init();
    syseng::computeOneFrame();
    syseng::close();
 
    return 0;
}

