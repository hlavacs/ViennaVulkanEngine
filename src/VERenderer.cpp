

#include "VERenderer.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    Renderer<ATYPE>::Renderer(Engine<ATYPE>& m_engine) : m_engine{m_engine} {};

   	template<ArchitectureType ATYPE>
    Renderer<ATYPE>::~Renderer(){};

   	template<ArchitectureType ATYPE>
    void Renderer<ATYPE>::Render(){};

    template class Renderer<ArchitectureType::SEQUENTIAL>;
    template class Renderer<ArchitectureType::PARALLEL>;

};  // namespace vve

