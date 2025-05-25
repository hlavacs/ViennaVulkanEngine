#pragma once

namespace vve {

	template <typename Derived>
	class RendererDeferredCommon : public Renderer{

	public:
		RendererDeferredCommon(std::string systemName, Engine& engine, std::string windowName) : Renderer(systemName, engine, windowName) {

			engine.RegisterCallbacks({
				
			});
		}

		virtual ~RendererDeferredCommon() {};

	};

}	// namespace vve
