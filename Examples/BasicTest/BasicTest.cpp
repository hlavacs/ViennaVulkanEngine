
#include <iostream>
#include <functional>

#include "STLTest.h"

#include "VEInclude.h"

int main()
{
    std::cout << "Hello World!\n";

    syseng::initEngine();
    syseng::computeOneFrame();
    syseng::closeEngine();
 
}

