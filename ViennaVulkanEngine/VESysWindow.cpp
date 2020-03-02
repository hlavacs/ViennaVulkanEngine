
#include "VEDefines.h"
#include "VESysEngine.h"
#include "VESysWindow.h"
#include "VESysWindowGLFW.h"

namespace vve::syswin {


	void init() {
		glfw::init();
	}

	void tick() {
		glfw::tick();
	}

	void sync() {
		glfw::sync();
	}

	void close() {
		glfw::close();
	}



}

