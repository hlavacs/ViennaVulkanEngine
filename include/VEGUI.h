#pragma once

#include "VEInclude.h"
#include "VESystem.h"
#include "VECS.h"

namespace vve {


	//-------------------------------------------------------------------------------------------------------

	template<ArchitectureType ATYPE>
	class GUI : public System<ATYPE> {

		using System<ATYPE>::m_engine;
		using typename System<ATYPE>::Message;
		using typename System<ATYPE>::MsgAnnounce;
		using typename System<ATYPE>::MsgFileLoadObject;

	public:
		GUI(std::string systemName, Engine<ATYPE>& engine );
    	~GUI() {};

	private:
		void OnAnnounce(Message message);
		void OnRecordNextFrame(Message message);
		WindowSDL<ATYPE>* m_windowSDL;
	};

};  // namespace vve

