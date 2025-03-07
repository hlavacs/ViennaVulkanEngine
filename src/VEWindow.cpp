#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    auto GetWindowState(vecs::Registry& registry) -> std::tuple<vecs::Handle, vecs::Ref<WindowState>> {
        return *registry.template GetView<vecs::Handle, WindowState&>().begin();
    }

    Window::Window(std::string systemName, Engine& engine,std::string windowName, int width, int height ) 
            : System(systemName, engine) {

        m_windowStateHandle = m_registry.Insert(WindowState{width, height, windowName});
    }

    Window::~Window(){}

    auto Window::GetWindowState2() -> vecs::Ref<WindowState> {
        if(!m_windowStateHandle.IsValid()) {
            auto [handle, state] = GetWindowState(m_registry);
            m_windowStateHandle = handle;
            return state;
        }
        return m_registry.template Get<WindowState&>(m_windowStateHandle);
    }

};   // namespace vve
