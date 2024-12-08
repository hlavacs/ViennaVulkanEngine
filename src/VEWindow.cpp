
#include "VHInclude.h"
#include "VHVulkan.h"

#include "VEWindow.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    Window<ATYPE>::Window(Engine<ATYPE>* engine,std::string windowName, int width, int height, std::string name ) 
            : System<ATYPE>(engine, name), m_width(width), m_height(height), m_windowName(windowName) {
    }

        template<ArchitectureType ATYPE>
    Window<ATYPE>::~Window(){}

   	template<ArchitectureType ATYPE>
    void Window<ATYPE>::AddRenderer(std::unique_ptr<Renderer<ATYPE>>&& renderer) { 
		m_renderer.emplace_back(std::forward<std::unique_ptr<Renderer<ATYPE>>>(renderer)); 
	};

    template<ArchitectureType ATYPE>
    void Window<ATYPE>::SetClearColor(glm::vec4 clearColor){ m_clearColor = clearColor; };

    template class Window<ENGINETYPE_SEQUENTIAL>;
    template class Window<ENGINETYPE_PARALLEL>;

};   // namespace vve
