#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    auto GetWindowState(vecs::Registry& registry) -> std::tuple<vecs::Handle, vecs::Ref<WindowState>> {
        return *registry.template GetView<vecs::Handle, WindowState&>().begin();
    }

    Window::Window(std::string systemName, Engine& engine,std::string windowName, int width, int height ) 
            : System(systemName, engine), m_width(width), m_height(height), m_windowName(windowName) {

        auto handle = m_registry.Insert(WindowState{width, height, windowName});
    }

    Window::~Window(){}
    void Window::SetClearColor(glm::vec4 clearColor){ m_clearColor = clearColor; };
};   // namespace vve
