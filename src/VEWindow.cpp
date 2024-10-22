
#include "VESystem.h"
#include "VEEngine.h"
#include "VERenderer.h"
#include "VEWindow.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    Window<ATYPE>::Window(std::string name, Engine<ATYPE>& engine, VkInstance instance, std::string windowName
        , int width, int height, std::vector<const char*>& instance_extensions) : System<ATYPE>(name, engine) {

    	engine.RegisterSystem( this, 0, {MessageType::POLL_EVENTS, MessageType::PREPARE_NEXT_FRAME, MessageType::RENDER_NEXT_FRAME, MessageType::PRESENT_NEXT_FRAME} );
    }

   	template<ArchitectureType ATYPE>
    Window<ATYPE>::~Window(){}

   	template<ArchitectureType ATYPE>
    void Window<ATYPE>::AddRenderer(int64_t priority, std::shared_ptr<Renderer<ATYPE>> renderer) { m_renderer[priority] = renderer; };

   	template<ArchitectureType ATYPE>
    void Window<ATYPE>::SetClearColor(glm::vec4 clearColor){ m_clearColor = clearColor; };

    template class Window<ArchitectureType::SEQUENTIAL>;
    template class Window<ArchitectureType::PARALLEL>;

};   // namespace vve
