#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	RendererDeferred11::RendererDeferred11(std::string systemName, Engine& engine, std::string windowName)
		: Renderer(systemName, engine, windowName) {

		engine.RegisterCallback({
			{this, 3500, "INIT", [this](Message& message) { return OnInit(message); } },
			//{this,     0, "QUIT", [this](Message& message) { return OnQuit(message); } }
			});
	}

	RendererDeferred11::~RendererDeferred11() {};
}
