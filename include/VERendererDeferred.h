#pragma once

namespace vve {
	class RendererDeferred : public Renderer {
		
		friend class RendererDeferred11;
		friend class RendererDeferred13;

		enum GBufferIndex { POSITION = 0, NORMAL = 1, ALBEDO = 2, DEPTH = 3 };

	public:
		RendererDeferred(std::string systemName, Engine& engine, std::string windowName);
		virtual ~RendererDeferred();

	private:
		bool OnInit(Message message);
	};

}	// namespace vve
