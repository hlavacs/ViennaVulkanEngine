#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    auto GetWindowState(vecs::Registry& registry) -> std::tuple<vecs::Handle, vecs::Ref<WindowState>> {
        return *registry.template GetView<vecs::Handle, WindowState&>().begin();
    }

    Window::Window(std::string systemName, Engine& engine,std::string windowName, int width, int height ) 
            : System(systemName, engine), m_width(width), m_height(height), m_windowName(windowName) {
    }

    Window::~Window(){}

    void Window::SetClearColor(glm::vec4 clearColor){ m_clearColor = clearColor; };

    auto Window::GetWindowState2() -> vecs::Ref<WindowState> {
        if(!m_windowStateHandle.IsValid()) {
            auto [handle, state] = GetWindowState(m_registry);
            m_windowStateHandle = handle;
            return state;
        }
        return m_registry.template Get<WindowState&>(m_windowStateHandle);
    }

};   // namespace vve
