#pragma once

#include <cstdint>
#include <variant>

namespace vve
{

    enum MessageType {
        REGISTERED = 0,
        UNREGISTERED,
        FRAME_START,
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

    struct VeMessage {
        MessageType m_type;
        MessageData m_data;
    };


    class VeSystem
    {
    public:
        VeSystem();
        virtual ~VeSystem();

        void onRegistered(VeMessage message);
        void onUnregistered(VeMessage message);
        void onFrameStart(VeMessage message);
        void onUpdate(VeMessage message);
        void onFrameEnd(VeMessage message);
        void onDrawGUI(VeMessage message);
    
    private:

    };

};

