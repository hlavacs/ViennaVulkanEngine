#pragma once

#include <cstdint>
#include <variant>

namespace vve
{

    enum MessageType {
        FRAME_START = 0,
        UPDATE,
        FRAME_END,
        DELETED,
        DRAW_GUI,
        MOUSE_MOVE,
        MOUSE_BUTTON_DOWN,
        MOUSE_BUTTON_UP,
        MOUSE_BUTTON_REPEAT,
        MOUSE_WHEEL,
        KEY_DOWN,
        KEY_UP,
        KEY_REPEAT
    };

    struct Message {
        MessageType m_type;
    };

    struct MessageFrameStart : public Message { MessageFrameStart(double dt): Message{FRAME_START}, m_dt{dt} {}; double m_dt; };
    struct MessageUpdate : public Message { MessageUpdate(double dt): Message{UPDATE}, m_dt{dt} {}; double m_dt; };
    struct MessageFrameEnd : public Message { MessageFrameEnd(double dt): Message{FRAME_END}, m_dt{dt} {}; double m_dt; };
    struct MessageDelete : public Message { MessageDelete(): Message{DELETED} {}; void* m_ptr; uint64_t m_id; };
    struct MessageDrawGUI : public Message { MessageDrawGUI(): Message{DRAW_GUI} {}; };
    struct MessageMouseMove : public Message { MessageMouseMove(int x, int y): Message{MOUSE_MOVE}, m_x{x}, m_y{y} {}; int m_x; int m_y; };
    struct MessageMouseButtonDown : public Message { MessageMouseButtonDown(int x, int y): Message{MOUSE_BUTTON_DOWN}, m_x{x}, m_y{y} {}; int m_x; int m_y; };
    struct MessageMouseButtonUp : public Message { MessageMouseButtonUp(int x, int y): Message{MOUSE_BUTTON_UP}, m_x{x}, m_y{y} {}; int m_x; int m_y; };
    struct MessageMouseButtonRepeat : public Message { MessageMouseButtonRepeat(int x, int y): Message{MOUSE_BUTTON_REPEAT}, m_x{x}, m_y{y} {}; int m_x; int m_y; };
    struct MessageMouseWheel : public Message { MessageMouseWheel(int x, int y): Message{MOUSE_WHEEL}, m_x{x}, m_y{y} {}; int m_x; int m_y; };
    struct MessageKeyDown : public Message { MessageKeyDown(int key): Message{KEY_DOWN}, m_key{key} {}; int m_key; };
    struct MessageKeyUp : public Message { MessageKeyUp(int key): Message{KEY_UP}, m_key{key} {}; int m_key; };
    struct MessageKeyRepeat : public Message { MessageKeyRepeat(int key): Message{KEY_REPEAT}, m_key{key} {}; int m_key; };    

    class System
    {
        friend class Engine;

    public:
        System();
        virtual ~System();
        virtual void onMessage(Message message);

    private:
        virtual void onFrameStart(Message message);
        virtual void onUpdate(Message message);
        virtual void onFrameEnd(Message message);
        virtual void onDelete(Message message);
        virtual void onDrawGUI(Message message);
        virtual void onMouseMove(Message message);
        virtual void onMouseButtonDown(Message message);
        virtual void onMouseButtonUp(Message message);
        virtual void onMouseButtonRepeat(Message message);
        virtual void onMouseWheel(Message message);
        virtual void onKeyDown(Message message);
        virtual void onKeyUp(Message message);
        virtual void onKeyRepeat(Message message);

    };

};

