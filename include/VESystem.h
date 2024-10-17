#pragma once

#include <vector>
#include <cstdint>
#include <variant>
#include <mutex>
#include <functional>
#include "VEInclude.h"

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
        KEY_REPEAT,
        LAST
    };

    struct MessageBase {
        MessageType m_type;
    };

    struct MessageFrameStart : public MessageBase { MessageFrameStart(double dt): MessageBase{FRAME_START}, m_dt{dt} {}; double m_dt; };
    struct MessageUpdate : public MessageBase { MessageUpdate(double dt): MessageBase{UPDATE}, m_dt{dt} {}; double m_dt; };
    struct MessageFrameEnd : public MessageBase { MessageFrameEnd(double dt): MessageBase{FRAME_END}, m_dt{dt} {}; double m_dt; };
    struct MessageDelete : public MessageBase { MessageDelete(): MessageBase{DELETED} {}; void* m_ptr; uint64_t m_id; };
    struct MessageDrawGUI : public MessageBase { MessageDrawGUI(): MessageBase{DRAW_GUI} {}; };
    struct MessageMouseMove : public MessageBase { MessageMouseMove(int x, int y): MessageBase{MOUSE_MOVE}, m_x{x}, m_y{y} {}; int m_x; int m_y; };
    
    struct MessageMouseButtonDown : public MessageBase { MessageMouseButtonDown(int button): MessageBase{MOUSE_BUTTON_DOWN}, m_button{button} {}; int m_button; };
    struct MessageMouseButtonUp : public MessageBase { MessageMouseButtonUp(int button): MessageBase{MOUSE_BUTTON_UP}, m_button{button} {}; int m_button; };
    struct MessageMouseButtonRepeat : public MessageBase { MessageMouseButtonRepeat(int button): MessageBase{MOUSE_BUTTON_REPEAT}, m_button{button} {}; int m_button; };
    struct MessageMouseWheel : public MessageBase { MessageMouseWheel(int x, int y): MessageBase{MOUSE_WHEEL}, m_x{x}, m_y{y} {}; int m_x; int m_y; };
    
    struct MessageKeyDown : public MessageBase { MessageKeyDown(int key): MessageBase{KEY_DOWN}, m_key{key} {}; int m_key; };
    struct MessageKeyUp : public MessageBase { MessageKeyUp(int key): MessageBase{KEY_UP}, m_key{key} {}; int m_key; };
    struct MessageKeyRepeat : public MessageBase { MessageKeyRepeat(int key): MessageBase{KEY_REPEAT}, m_key{key} {}; int m_key; };    


    struct Message {
        static const size_t MAX_SIZE = 100;

        template<typename T>
        Message(const T&& msg ) {
            std::memcpy(m_data, &msg, sizeof(T));
        };

        auto getType() -> MessageType {
            return reinterpret_cast<MessageBase*>(&m_data)->m_type;
        };

        template<typename T>
            requires (std::is_base_of_v<MessageBase, T> && sizeof(T) <= MAX_SIZE)
        auto getData() -> T& {
            return *reinterpret_cast<T*>(m_data);
        };

    private:
        uint8_t m_data[MAX_SIZE];
    };


   	template<ArchitectureType ATYPE>
    class Engine;

   	template<ArchitectureType ATYPE>
    class System
    {
        friend class Engine<ATYPE>;

    public:
        System( Engine<ATYPE>& engine );
        virtual ~System();
        virtual void receiveMessage(Message message);

    protected:
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

        std::vector<std::function<void(Message)>> m_onFunctions;
        Engine<ATYPE>& m_engine;
        Mutex<ATYPE> m_mutex;
        std::vector<Message> m_messages;
    };

};

