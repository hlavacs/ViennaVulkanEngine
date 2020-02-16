
#include <iostream>
#include <functional>

#include "STLTest.h"

#include "VEInclude.h"

int main()
{
    std::cout << "Hello World!\n";

    ve::initEngine();
    ve::computeOneFrame();
    ve::closeEngine();
 
}

