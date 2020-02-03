#pragma once

#include <functional>
#include <atomic>


#include "VEDefines.h"

namespace ve {

	///Public engine interface
	void initEngine();
	void runGameLoop();

	///private engine interface
#ifndef VE_PUBLIC_INTERFACE
	
	VEHANDLE getNewHandle();

#endif
}

