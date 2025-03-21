#pragma once

namespace vve {

	class RendererDeferred11 : public Renderer {

		friend class RendererDeferred;

	public:
		RendererDeferred11(std::string systemName, Engine& engine, std::string windowName);
		virtual ~RendererDeferred11();

	private:
	};

}	// namespace vve
