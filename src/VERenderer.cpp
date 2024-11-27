


#include <any>
#include "VHInclude.h"
#include "VHVulkan.h"
#include "VERenderer.h"


namespace vve {
    
   	template<ArchitectureType ATYPE>
    Renderer<ATYPE>::Renderer(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name ) 
        : System<ATYPE>{engine, name }, m_window{window} {};

   	template<ArchitectureType ATYPE>
    Renderer<ATYPE>::~Renderer(){};

    template class Renderer<ENGINETYPE_SEQUENTIAL>;
    template class Renderer<ENGINETYPE_PARALLEL>;

};  // namespace vve

