
#include "VERendererImgui.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::RendererImgui(Engine<ATYPE>& engine, Window<ATYPE>& window) 
        : Renderer<ATYPE>(engine), m_window(window) {};

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::~RendererImgui(){};

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::Render(){};

    template class RendererImgui<ArchitectureType::SEQUENTIAL>;
    template class RendererImgui<ArchitectureType::PARALLEL>;

};   // namespace vve

