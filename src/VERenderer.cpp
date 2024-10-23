
#include "VESystem.h"
#include "VERenderer.h"


namespace vve {
    
   	template<ArchitectureType ATYPE>
    Renderer<ATYPE>::Renderer(std::string name, Engine<ATYPE>* engine, Window<ATYPE>* window) 
        : System<ATYPE>{name, engine}, m_window{window} {};

   	template<ArchitectureType ATYPE>
    Renderer<ATYPE>::~Renderer(){};

    template class Renderer<ArchitectureType::SEQUENTIAL>;
    template class Renderer<ArchitectureType::PARALLEL>;

};  // namespace vve

