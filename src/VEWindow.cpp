

#include "VEWindow.h"


namespace vve {

    Window::Window(Engine& engine, VkInstance instance, std::string windowName
        , int width, int height, std::vector<const char*>& instance_extensions) : m_engine(engine) {}

    Window::~Window(){}

    void Window::setClearColor(glm::vec4 clearColor){ m_clearColor = clearColor; };


};   // namespace vve
