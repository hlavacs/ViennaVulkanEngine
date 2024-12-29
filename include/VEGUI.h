#pragma once

#include "VEInclude.h"
#include "VESystem.h"
#include "VECS.h"

namespace vve {


	//-------------------------------------------------------------------------------------------------------

	template<ArchitectureType ATYPE>
	class GUI : public System<ATYPE> {

		using System<ATYPE>::m_engine;

	public:
		GUI(std::string systemName, Engine<ATYPE>& engine );
    	~GUI() {};

	private:
		void OnAnnounce(vve::Message message);
		void OnRecordNextFrame(vve::Message message);
		WindowSDL<ATYPE>* m_windowSDL;
	};

};  // namespace vve

