
#include <iostream>
#include <functional>

#include "STLTest.h"

#include "VEInclude.h"

using namespace vve;

int main()
{
    std::cout << "Hello World!\n";

    //tab::testTables();
    vec::testVector();
    return 0;

    syseng::initEngine();
    syseng::computeOneFrame();
    syseng::closeEngine();
 
    return 0;
}

