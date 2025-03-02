#pragma once

namespace vve {
	class RendererDeferred : public Renderer {
	public:
		RendererDeferred(std::string systemName, Engine& engine, std::string windowName);
		virtual ~RendererDeferred();
	};

}	// namespace vve
