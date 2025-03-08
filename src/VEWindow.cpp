#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    auto GetWindowState(vecs::Registry& registry, const std::string&& windowName) -> std::tuple<vecs::Handle, vecs::Ref<WindowState>> {
        for( auto ret: registry.template GetView<vecs::Handle, WindowState&>() ) {
            auto [handle, wstate] = ret;
            if( windowName.empty() ) return ret;
            if( wstate().m_windowName == windowName ) return ret;
        }
        std::cout << "Window not found: " << windowName << std::endl;
        exit(-1);   
        return { {}, {} };
    }

    Window::Window(std::string systemName, Engine& engine,std::string windowName, int width, int height ) 
            : System(systemName, engine) {

        m_windowStateHandle = m_registry.Insert(WindowState{width, height, windowName}, Name{windowName});
    }

    Window::~Window(){}

    auto Window::GetWindowState2() -> vecs::Ref<WindowState> {
        return m_registry.template Get<WindowState&>(m_windowStateHandle);
    }

};   // namespace vve
