#pragma once

namespace vve
{
    class RendererForward : public Renderer {

	public:
        RendererForward(std::string systemName, Engine& engine, std::string windowName);
        virtual ~RendererForward();

    private:
        bool OnInit(Message message);
	};
}


