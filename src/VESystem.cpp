

#include "VESystem.h"


namespace vve {

    System::System(){};
    System::~System(){};

    void System::onMessage(Message message) {
        switch (message.m_type) {
        case MessageType::FRAME_START:
            onFrameStart(message);
            break;
        case MessageType::UPDATE:
            onUpdate(message);
            break;
        case MessageType::FRAME_END:
            onFrameEnd(message);
            break;
        case MessageType::DELETED:
            onDelete(message);
            break;
        case MessageType::DRAW_GUI:
            onDrawGUI(message);
            break;
        default:
            break;
        }
    };
    void System::onFrameStart(Message message){};
    void System::onUpdate(Message message){};
    void System::onFrameEnd(Message message){};
    void System::onDelete(Message message){};
    void System::onDrawGUI(Message message){};

};   // namespace vve


