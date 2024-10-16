

#include "VESystem.h"


namespace vve {

    System::System(){};
    System::~System(){};

    void System::onRegistered(Message message){};
    void System::onUnregistered(Message message){};
    void System::onFrameStart(Message message){};
    void System::onUpdate(Message message){};
    void System::onFrameEnd(Message message){};
    void System::onDrawGUI(Message message){};

};   // namespace vve


