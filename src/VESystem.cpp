

#include "VESystem.h"


namespace vve {

    VeSystem::VeSystem(){};
    VeSystem::~VeSystem(){};

    void VeSystem::onRegistered(VeMessage message){};
    void VeSystem::onUnregistered(VeMessage message){};
    void VeSystem::onFrameStart(VeMessage message){};
    void VeSystem::onUpdate(VeMessage message){};
    void VeSystem::onFrameEnd(VeMessage message){};
    void VeSystem::onDrawGUI(VeMessage message){};

};   // namespace vve


