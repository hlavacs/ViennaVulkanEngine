#pragma once

namespace vve {

	class RendererDeferredCommon : public Renderer{

	public:
		RendererDeferredCommon(std::string systemName, Engine& engine, std::string windowName);
		virtual ~RendererDeferredCommon();

	};

}	// namespace vve
