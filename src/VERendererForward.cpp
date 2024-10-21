
#include "VESystem.h"
#include "VERendererForward.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    RendererForward<ATYPE>::RendererForward(std::string name, Engine<ATYPE>& engine, Window<ATYPE>* window) 
        : Renderer<ATYPE>(name, engine, window) {};

   	template<ArchitectureType ATYPE>
    RendererForward<ATYPE>::~RendererForward(){};

    template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::PrepareRender() {};

   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::Render(){};

    template class RendererForward<ArchitectureType::SEQUENTIAL>;
    template class RendererForward<ArchitectureType::PARALLEL>;

};   // namespace vve


