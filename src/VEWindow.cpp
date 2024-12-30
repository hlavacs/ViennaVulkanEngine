#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    Window::Window(std::string systemName, Engine& engine,std::string windowName, int width, int height ) 
            : System(systemName, engine), m_width(width), m_height(height), m_windowName(windowName) {
    }

    Window::~Window(){}

    void Window::SetClearColor(glm::vec4 clearColor){ m_clearColor = clearColor; };


};   // namespace vve
