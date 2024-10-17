

#include "VERendererForward.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    RendererForward<ATYPE>::RendererForward(Engine<ATYPE>& engine) : Renderer<ATYPE>(engine) {};

   	template<ArchitectureType ATYPE>
    RendererForward<ATYPE>::~RendererForward(){};

   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::Render(){};

    template class RendererForward<ArchitectureType::SEQUENTIAL>;
    template class RendererForward<ArchitectureType::PARALLEL>;

};   // namespace vve


