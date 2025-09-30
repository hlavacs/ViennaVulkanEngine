#pragma once

namespace vve {
	/**
	 * @brief Deferred renderer implementation (factory class)
	 */
	class RendererDeferred : public System {

	public:
		/**
		 * @brief Constructor for Deferred Renderer
		 * @param systemName Name of the system
		 * @param engine Reference to the engine
		 * @param windowName Name of the associated window
		 */
		RendererDeferred(const std::string& systemName, Engine& engine, const std::string& windowName);
		/**
		 * @brief Destructor for Deferred Renderer
		 */
		virtual ~RendererDeferred();

	private:
		bool OnInit(const Message& message);

		std::string m_windowName;
	};

}	// namespace vve
