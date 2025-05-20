#include "VHInclude2.h"
#include "VEInclude.h"

namespace vve {

	RendererDeferred13::RendererDeferred13(std::string systemName, Engine& engine, std::string windowName)
		: Renderer(systemName, engine, windowName) {

		engine.RegisterCallbacks({
			{this,  3500, "INIT",				[this](Message& message) { return OnInit(message); } },
			//{this,  2000, "PREPARE_NEXT_FRAME", [this](Message& message) { return OnPrepareNextFrame(message); } },
			//{this,  2000, "RECORD_NEXT_FRAME",	[this](Message& message) { return OnRecordNextFrame(message); } },
			//{this,  2000, "OBJECT_CREATE",		[this](Message& message) { return OnObjectCreate(message); } },
			//{this, 10000, "OBJECT_DESTROY",		[this](Message& message) { return OnObjectDestroy(message); } },
			//{this,  1500, "WINDOW_SIZE",		[this](Message& message) { return OnWindowSize(message); }},
			//{this, 	   0, "QUIT",				[this](Message& message) { return OnQuit(message); } }
			});
	}

	RendererDeferred13::~RendererDeferred13() {};

	bool RendererDeferred13::OnInit(Message message) {
		// TODO: maybe a reference will be enough here to not make a message copy?
		Renderer::OnInit(message);

		return false;
	}

}	// namespace vve
