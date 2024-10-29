

#include "VHInclude.h"
#include "VERendererVulkan.h"
#include "VESystem.h"
#include "VEInclude.h"
#include "VHInclude.h"
#include "VEEngine.h"
#include "VEWindow.h"


namespace vve {

    template<ArchitectureType ATYPE>
    RendererVulkan<ATYPE>::RendererVulkan(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name ) 
        : Renderer<ATYPE>(engine, window, name) {}
    
    template<ArchitectureType ATYPE>
    RendererVulkan<ATYPE>::~RendererVulkan() {}

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnInit(Message message) {}

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnPrepareNextFrame(Message message) {}

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnRenderNextFrame(Message message) {}
    
    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnQuit(Message message) { }

    template class RendererVulkan<ArchitectureType::SEQUENTIAL>;
    template class RendererVulkan<ArchitectureType::PARALLEL>;

};   // namespace vve