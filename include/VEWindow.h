#pragma once

#include <string>

namespace vve {

    class VeWindow
    {
    public:
        VeWindow(std::string windowName, int width, int height);
        virtual ~VeWindow();

    private:
    };

}   // namespace vve

