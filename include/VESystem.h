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

    struct MessageDataRegistered { };
    struct MessageDataUnregistered { };
    struct MessageDataFrameStarted { double m_dt; };
    struct MessageDataUpdate { double m_dt; };
    struct MessageDataFrameEnded { double m_dt; };
    struct MessageDataDeleted { void* m_ptr; uint64_t m_id;};
    struct MessageDataDrawGUI { };

    using MessageData = std::variant<MessageDataRegistered, MessageDataUnregistered, MessageDataFrameStarted
        , MessageDataUpdate, MessageDataFrameEnded, MessageDataDeleted, MessageDataDrawGUI>;

    struct Message {
        MessageType m_type;
        MessageData m_data;
    };


    class System
    {
    public:
        System();
        virtual ~System();

        void onRegistered(Message message);
        void onUnregistered(Message message);
        void onFrameStart(Message message);
        void onUpdate(Message message);
        void onFrameEnd(Message message);
        void onDrawGUI(Message message);
    
    private:

    };

};

