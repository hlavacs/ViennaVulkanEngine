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
		void OnAnnounce(Message message);
		void OnRecordNextFrame(Message message);

		std::string m_windowName;
		WindowSDL* m_windowSDL;
	};

};  // namespace vve

