
#include <iostream>
#include <functional>

#include "STLTest.h"

#include "VEInclude.h"

int main()
{
    std::cout << "Hello World!\n";

    stltest::runTests();
    return 0;

    ve::initEngine();
    ve::computeOneFrame();
    ve::closeEngine();
 
}

