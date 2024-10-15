

#include "VEWindow.h"


namespace vve {

    VeWindow::VeWindow(VeEngine& engine, VkInstance instance, std::string windowName, int width, int height, std::vector<const char*> instance_extensions) : m_engine(engine) {}

    VeWindow::~VeWindow(){}

    auto VeWindow::getSurface() -> VkSurfaceKHR { return m_surface; }

};   // namespace vve
