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
        DRAW_GUI
    };

    struct Message {
        MessageType m_type;
    };

    struct MessageFrameStart : public Message { MessageFrameStart(double dt): Message{FRAME_START}, m_dt{dt} {}; double m_dt; };
    struct MessageUpdate : public Message { MessageUpdate(double dt): Message{UPDATE}, m_dt{dt} {}; double m_dt; };
    struct MessageFrameEnd : public Message { MessageFrameEnd(double dt): Message{FRAME_END}, m_dt{dt} {}; double m_dt; };
    struct MessageDelete : public Message { MessageDelete(): Message{DELETED} {}; void* m_ptr; uint64_t m_id; };
    struct MessageDrawGUI : public Message { MessageDrawGUI(): Message{DRAW_GUI} {}; };

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
    };

};

