#include <filesystem>

#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

	RendererDeferred::RendererDeferred(std::string systemName, Engine& engine, std::string windowName)
		: Renderer(systemName, engine, windowName) {

		// TODO engine.RegisterCallback -> register Callbacks
	}

	RendererDeferred::~RendererDeferred() {};

}	 // namespace vve
