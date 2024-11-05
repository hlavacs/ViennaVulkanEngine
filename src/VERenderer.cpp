
#include <any>
#include "VESystem.h"
#include "VERenderer.h"


namespace vve {
    
   	template<ArchitectureType ATYPE>
    Renderer<ATYPE>::Renderer(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name ) 
        : System<ATYPE>{engine, name }, m_window{window} {};

   	template<ArchitectureType ATYPE>
    Renderer<ATYPE>::~Renderer(){};

   	template<ArchitectureType ATYPE>
    auto Renderer<ATYPE>::GetState() -> std::any { 
        return std::any(m_engine->GetState()); 
    }

    template class Renderer<ArchitectureType::SEQUENTIAL>;
    template class Renderer<ArchitectureType::PARALLEL>;

};  // namespace vve

