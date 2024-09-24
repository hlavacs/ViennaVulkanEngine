#pragma once

#include "VeWindow.h"


namespace vve {

    class VeWindowSDL : public VeWindow {
    
    public:
        VeWindowSDL(std::string windowName, int width, int height);
        virtual ~VeWindowSDL();
    
    private:
    
    };

}  // namespace vve

