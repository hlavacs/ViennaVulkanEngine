#pragma once

#include "VEInclude.h"
#include "VESystem.h"
#include "VECS.h"

namespace vve {


	//-------------------------------------------------------------------------------------------------------

	class GUI : public System {

	public:
		GUI(std::string systemName, Engine& engine, std::string windowName);
    	~GUI() {};

	private:
		bool OnAnnounce(Message message);
		bool OnRecordNextFrame(Message message);
		bool OnKeyDown(Message message);
		bool OnKeyUp(Message message);
		bool OnMouseButtonDown(Message message);
		bool OnMouseButtonUp(Message message);
		bool OnMouseMove(Message message);
		bool OnMouseWheel(Message message);
		void GetCamera();

		std::string m_windowName;
		WindowSDL* m_windowSDL;
		bool m_mouseButtonDown=false;
		bool m_shiftPressed=false;
		int m_x = -1;
		int m_y = -1;
		vecs::Handle m_cameraHandle{};
		vecs::Handle m_cameraNodeHandle{};

	};

};  // namespace vve

