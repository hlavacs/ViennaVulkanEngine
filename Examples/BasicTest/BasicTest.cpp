
#include <iostream>
#include <functional>

#include "STLTest.h"

#include "VEInclude.h"

using namespace vve;

int main()
{
    std::cout << "Hello World!\n";

    syseng::initEngine();
    syseng::computeOneFrame();
    syseng::closeEngine();
 
}

