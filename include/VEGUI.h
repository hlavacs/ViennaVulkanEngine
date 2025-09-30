#pragma once

#include "VEInclude.h"
#include "VESystem.h"
#include "VECS.h"

namespace vve {


	//-------------------------------------------------------------------------------------------------------

	class GUI : public System {

	public:
		/**
		 * @brief Constructor for the GUI class
		 * @param systemName Name of the system
		 * @param engine Reference to the engine
		 * @param windowName Name of the window
		 */
		GUI(std::string systemName, Engine& engine, std::string windowName);

		/**
		 * @brief Destructor for the GUI class
		 */
    	~GUI() {};

	private:
		/**
		 * @brief Handle announce message
		 * @param message Message containing announce data
		 * @return True if message was handled
		 */
		bool OnAnnounce(Message message);

		/**
		 * @brief Handle key down event
		 * @param message Message containing key down data
		 * @return True if message was handled
		 */
		bool OnKeyDown(Message message);

		/**
		 * @brief Handle key up event
		 * @param message Message containing key up data
		 * @return True if message was handled
		 */
		bool OnKeyUp(Message message);

		/**
		 * @brief Handle mouse button down event
		 * @param message Message containing mouse button down data
		 * @return True if message was handled
		 */
		bool OnMouseButtonDown(Message message);

		/**
		 * @brief Handle mouse button up event
		 * @param message Message containing mouse button up data
		 * @return True if message was handled
		 */
		bool OnMouseButtonUp(Message message);

		/**
		 * @brief Handle mouse move event
		 * @param message Message containing mouse move data
		 * @return True if message was handled
		 */
		bool OnMouseMove(Message message);

		/**
		 * @brief Handle mouse wheel event
		 * @param message Message containing mouse wheel data
		 * @return True if message was handled
		 */
		bool OnMouseWheel(Message message);

		/**
		 * @brief Handle frame end event
		 * @param message Message containing frame end data
		 * @return True if message was handled
		 */
		bool OnFrameEnd(Message message);

		/**
		 * @brief Get the current camera handle
		 */
		void GetCamera();

		std::string m_windowName;
		bool m_mouseButtonDown=false;
		bool m_shiftPressed=false;
		float m_x = -1;
		float m_y = -1;
		vecs::Handle m_cameraHandle{};
		vecs::Handle m_cameraNodeHandle{};
		bool m_makeScreenshot{false};
		int m_numScreenshot{0};
		bool m_makeScreenshotDepth{false};

	};

};  // namespace vve

