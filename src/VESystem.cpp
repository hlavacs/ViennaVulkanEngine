

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
        case MessageType::MOUSE_MOVE:
            onMouseMove(message);
            break;
        case MessageType::MOUSE_BUTTON_DOWN:
            onMouseButtonDown(message);
            break;
        case MessageType::MOUSE_BUTTON_UP:
            onMouseButtonUp(message);
            break;
        case MessageType::MOUSE_BUTTON_REPEAT:
            onMouseButtonRepeat(message);
            break;
        case MessageType::MOUSE_WHEEL:
            onMouseWheel(message);
            break;
        case MessageType::KEY_DOWN:
            onKeyDown(message);
            break;
        case MessageType::KEY_UP:
            onKeyUp(message);
            break;
        case MessageType::KEY_REPEAT:
            onKeyRepeat(message);
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
    void System::onMouseMove(Message message){};
    void System::onMouseButtonDown(Message message){};
    void System::onMouseButtonUp(Message message){};
    void System::onMouseButtonRepeat(Message message){};
    void System::onMouseWheel(Message message){};
    void System::onKeyDown(Message message){};
    void System::onKeyUp(Message message){};
    void System::onKeyRepeat(Message message){};

};   // namespace vve


