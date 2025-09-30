#pragma once

namespace vve
{
    /**
     * @brief Forward renderer implementation (factory class)
     */
    class RendererForward : public Renderer {

	public:
        /**
         * @brief Constructor for Forward Renderer
         * @param systemName Name of the system
         * @param engine Reference to the engine
         * @param windowName Name of the associated window
         */
        RendererForward(std::string systemName, Engine& engine, std::string windowName);
        /**
         * @brief Destructor for Forward Renderer
         */
        virtual ~RendererForward();

    private:
        bool OnInit(Message message);
	};
}


