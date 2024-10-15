

#include "VEWindowSDL.h"

using namespace vve;

VeWindowSDL::VeWindowSDL(std::string windowName, int width, int height) : VeWindow(windowName, width, height) {
    //if(!sdl_initialized) {
    //    initSDL();
    //    sdl_initialized = true;
    //}

}

VeWindowSDL::~VeWindowSDL(){}

void VeWindowSDL::initSDL() {

}
