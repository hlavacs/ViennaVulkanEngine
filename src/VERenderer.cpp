

#include "VERenderer.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    Renderer<ATYPE>::Renderer(Engine<ATYPE>& m_engine, std::weak_ptr<Window<ATYPE>> window) 
        : m_engine{m_engine}, m_window{window} {};

   	template<ArchitectureType ATYPE>
    Renderer<ATYPE>::~Renderer(){};

   	template<ArchitectureType ATYPE>
    void Renderer<ATYPE>::PrepareRender(){};

   	template<ArchitectureType ATYPE>
    void Renderer<ATYPE>::Render(){};

    template class Renderer<ArchitectureType::SEQUENTIAL>;
    template class Renderer<ArchitectureType::PARALLEL>;

};  // namespace vve

