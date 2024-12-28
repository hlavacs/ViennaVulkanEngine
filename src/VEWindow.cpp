#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    Window<ATYPE>::Window(std::string systemName, Engine<ATYPE>& engine,std::string windowName, int width, int height ) 
            : System<ATYPE>(systemName, engine), m_width(width), m_height(height), m_windowName(windowName) {
    }

    template<ArchitectureType ATYPE>
    Window<ATYPE>::~Window(){}

    template<ArchitectureType ATYPE>
    void Window<ATYPE>::SetClearColor(glm::vec4 clearColor){ m_clearColor = clearColor; };

    template class Window<ENGINETYPE_SEQUENTIAL>;
    template class Window<ENGINETYPE_PARALLEL>;

};   // namespace vve
