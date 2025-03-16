#pragma once

namespace vve
{
    class RendererShadow11 : public Renderer {

	public:
	RendererShadow11(std::string systemName, Engine& engine, std::string windowName);
        virtual ~RendererShadow11();

    private:
        bool OnInit(Message message);
        bool OnPrepareNextFrame(Message message);
        bool OnRecordNextFrame(Message message);
        bool OnQuit(Message message);
    };

};   // namespace vve

