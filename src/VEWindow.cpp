#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    auto Window::GetState(vecs::Registry& registry, const std::string& windowName) -> std::tuple<vecs::Handle, vecs::Ref<WindowState>> {
        for( auto ret: registry.template GetView<vecs::Handle, WindowState&>() ) {
            auto [handle, wstate] = ret;
            if( windowName.empty() ) return ret;
            if( wstate().m_windowName == windowName ) return ret;
        }
        std::cout << "Window not found: " << windowName << std::endl;
        assert(false);
        exit(-1);   
        return { {}, {} };
    }

    auto Window::GetState2() -> vecs::Ref<WindowState> {
        return m_registry.template Get<WindowState&>(m_windowStateHandle);
    }    
    
    
    Window::Window(std::string systemName, Engine& engine,std::string windowName, int width, int height ) 
            : System(systemName, engine) {
    }

    Window::~Window(){}

};   // namespace vve
