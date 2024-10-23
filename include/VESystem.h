#pragma once

#include <vector>
#include <cstdint>
#include <variant>
#include <mutex>
#include <functional>
#include <typeindex>
#include <typeinfo>
#include "VEInclude.h"

namespace vve {

    enum MessageType {
        INIT = 0,
        FRAME_START,
        POLL_EVENTS,
        UPDATE,
        PREPARE_NEXT_FRAME,
        RENDER_NEXT_FRAME,
        RECORD_NEXT_FRAME,
        PRESENT_NEXT_FRAME,
        FRAME_END,
        DELETED,
        MOUSE_MOVE,
        MOUSE_BUTTON_DOWN,
        MOUSE_BUTTON_UP,
        MOUSE_BUTTON_REPEAT,
        MOUSE_WHEEL,
        KEY_DOWN,
        KEY_UP,
        KEY_REPEAT,
        QUIT,
        LAST
    };

    struct MessageTypePhase {
        MessageType m_type;
        int m_phase;
    };

    struct MessageBase {
        MessageType m_type;
        void* m_sender{nullptr};
        void* m_receiver{nullptr};
        double m_dt;
        int m_phase; //can register the same system for more than one phase
    };

    struct MessageInit : public MessageBase { MessageInit(void* s, void* r); };
    struct MessageFrameStart : public MessageBase { MessageFrameStart(void* s, void* r, double dt); };
    struct MessagePollEvents : public MessageBase { MessagePollEvents(void* s, void* r, double dt); };
    struct MessageUpdate : public MessageBase { MessageUpdate(void* s, void* r, double dt); double m_dt; };
    struct MessagePrepareNextFrame : public MessageBase { MessagePrepareNextFrame(void* s, void* r, double dt); };
    struct MessageRenderNextFrame : public MessageBase { MessageRenderNextFrame(void* s, void* r, double dt);  };
    struct MessageRecordNextFrame : public MessageBase { MessageRecordNextFrame(void* s, void* r, double dt ); };
    struct MessagePresentNextFrame : public MessageBase { MessagePresentNextFrame(void* s, void* r, double dt);  };
    struct MessageFrameEnd : public MessageBase { MessageFrameEnd(void* s, void* r, double dt); };
    struct MessageDelete : public MessageBase { MessageDelete(void* s, void* r, double dt ); void* m_ptr; uint64_t m_id; };
    struct MessageMouseMove : public MessageBase { MessageMouseMove(void* s, void* r, double dt, int x, int y); int m_x; int m_y; };
    
    struct MessageMouseButtonDown : public MessageBase { MessageMouseButtonDown(void* s, void* r, double dt, int button); int m_button; };
    struct MessageMouseButtonUp : public MessageBase { MessageMouseButtonUp(void* s, void* r, double dt, int button);  int m_button; };
    struct MessageMouseButtonRepeat : public MessageBase { MessageMouseButtonRepeat(void* s, void* r, double dt, int button);  int m_button; };
    struct MessageMouseWheel : public MessageBase { MessageMouseWheel(void* s, void* r, double dt, int x, int y);  int m_x; int m_y; };
    
    struct MessageKeyDown : public MessageBase { MessageKeyDown(void* s, void* r, double dt, int key);  int m_key; };
    struct MessageKeyUp : public MessageBase { MessageKeyUp(void* s, void* r, double dt, int key);  int m_key; };
    struct MessageKeyRepeat : public MessageBase { MessageKeyRepeat(void* s, void* r, double dt, int key); int m_key; };

    struct MessageQuit : public MessageBase { MessageQuit(void* s, void* r); };


    struct Message {
        static const size_t MAX_SIZE = 64;

        template<typename T>
            requires (std::is_base_of_v<MessageBase, T> && sizeof(T) <= MAX_SIZE)
        Message(const T&& msg ) {
            m_typeID = std::type_index(typeid(T)).hash_code();
            std::memcpy(m_data, &msg, sizeof(T));
        };

        auto GetType() -> MessageType { return reinterpret_cast<MessageBase*>(m_data)->m_type; };
        auto GetSender() -> void* { return reinterpret_cast<MessageBase*>(m_data)->m_sender; };
        auto GetReceiver() -> void* { return reinterpret_cast<MessageBase*>(m_data)->m_receiver; };
        auto GetDt() -> double { return reinterpret_cast<MessageBase*>(m_data)->m_dt; };
        void SetPhase(int priority) { reinterpret_cast<MessageBase*>(m_data)->m_phase = priority; };
        auto GetPhase() -> int { return reinterpret_cast<MessageBase*>(m_data)->m_phase; };

        template<typename T>
            requires (std::is_base_of_v<MessageBase, T> && sizeof(T) <= MAX_SIZE)
        auto GetData() -> T& {
            assert(m_typeID == std::type_index(typeid(T)).hash_code() );
            return *reinterpret_cast<T*>(m_data);
        };

    private:
        size_t m_typeID{};
        uint8_t m_data[MAX_SIZE];
    };


   	template<ArchitectureType ATYPE>
    class Engine;

   	template<ArchitectureType ATYPE>
    class System
    {
        friend class Engine<ATYPE>;

    public:
        System( std::string name, Engine<ATYPE>& engine );
        virtual ~System();
        virtual void ReceiveMessage(Message message);

    protected:
        virtual void OnInit(Message message);
        virtual void OnFrameStart(Message message);
        virtual void OnPollEvents(Message message);
        virtual void OnUpdate(Message message);
        virtual void OnPrepareNextFrame(Message message);
        virtual void OnRenderNextFrame(Message message);
        virtual void OnRecordNextFrame(Message message);
        virtual void OnPresentNextFrame(Message message);
        virtual void OnFrameEnd(Message message);
        virtual void OnDelete(Message message);
        virtual void OnMouseMove(Message message);
        virtual void OnMouseButtonDown(Message message);
        virtual void OnMouseButtonUp(Message message);
        virtual void OnMouseButtonRepeat(Message message);
        virtual void OnMouseWheel(Message message);
        virtual void OnKeyDown(Message message);
        virtual void OnKeyUp(Message message);
        virtual void OnKeyRepeat(Message message);
        virtual void OnQuit(Message message);

        std::string m_name;
        std::unordered_map<MessageType, std::function<void(Message)>> m_onFunctions;
        Engine<ATYPE>& m_engine;
        Mutex<ATYPE> m_mutex;
        std::vector<Message> m_messages;
    };

};

