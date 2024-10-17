
#include "VERendererImgui.h"
#include "VEWindow.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::RendererImgui(Engine<ATYPE>& engine, std::shared_ptr<Window<ATYPE>> window) 
        : Renderer<ATYPE>(engine, window) {};

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::~RendererImgui(){};

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::Render(){};

    template class RendererImgui<ArchitectureType::SEQUENTIAL>;
    template class RendererImgui<ArchitectureType::PARALLEL>;

};   // namespace vve

