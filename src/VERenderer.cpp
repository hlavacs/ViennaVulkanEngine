


#include <any>
#include "VHInclude.h"
#include "VHVulkan.h"
#include "VERenderer.h"


namespace vve {
    
   	template<ArchitectureType ATYPE>
    Renderer<ATYPE>::Renderer(std::string systemName, Engine<ATYPE>& engine, Window<ATYPE>* window ) 
        : System<ATYPE>{systemName, engine }, m_window{window} {
	};

   	template<ArchitectureType ATYPE>
    Renderer<ATYPE>::~Renderer(){};

    template class Renderer<ENGINETYPE_SEQUENTIAL>;
    template class Renderer<ENGINETYPE_PARALLEL>;

};  // namespace vve

