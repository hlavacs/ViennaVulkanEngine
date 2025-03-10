#include <filesystem>

#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

	RendererDeferred::RendererDeferred(std::string systemName, Engine& engine, std::string windowName)
		: Renderer(systemName, engine, windowName) {

		// TODO engine.RegisterCallback -> register Callbacks
		engine.RegisterCallback({
			{this, 3000, "INIT", [this](Message& message) { return OnInit(message); } },
			{this,    0, "QUIT", [this](Message& message) { return OnQuit(message); } }
			});
	}

	RendererDeferred::~RendererDeferred() {};

	bool RendererDeferred::OnInit(Message message) {
		// TODO
		std::cout << "RendererDeferred: OnInit callback\n";

		return false;
	}

	bool RendererDeferred::OnQuit(Message message) {
		// TODO
		std::cout << "RendererDeferred: OnQuit callback\n";

		return false;
	}

}	 // namespace vve
