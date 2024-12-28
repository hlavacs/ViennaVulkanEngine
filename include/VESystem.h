#pragma once

#include <vector>
#include <cstdint>
#include <variant>
#include <mutex>
#include <functional>
#include <typeindex>
#include <typeinfo>
#include <type_traits>
#include <unordered_set>


namespace vve {

    const std::unordered_set<std::string> MsgTypeNames {
        "INIT",			//initialize the system
        "ANNOUNCE", //
        "FRAME_START", //
        "POLL_EVENTS", //
        "UPDATE", //
        "PREPARE_NEXT_FRAME",	// prepare the next frame
        "RECORD_NEXT_FRAME",	// record the next frame	
        "RENDER_NEXT_FRAME",	// render the next frame
        "PRESENT_NEXT_FRAME", // present the next frame
        "FRAME_END", //
        "DELETED", //React to something being deleted
        "SDL", // 
        "SDL_MOUSE_MOVE", //
        "SDL_MOUSE_BUTTON_DOWN", //
        "SDL_MOUSE_BUTTON_UP", //
        "SDL_MOUSE_BUTTON_REPEAT", //
        "SDL_MOUSE_WHEEL", //
        "SDL_KEY_DOWN", //
        "SDL_KEY_UP", //
        "SDL_KEY_REPEAT", //
        "QUIT", //
		//---------------------
		"OBJECT_CREATE",
		"FILE_LOAD_OBJECT",	
		"TEXTURE_CREATE",  
		"TEXTURE_DESTROY", 
		"GEOMETRY_CREATE",	
		"GEOMETRY_DESTROY",	
        "LAST" //
    };


    struct MsgTypePhase {
        size_t m_type;
        int m_phase;
    };

    struct MsgBase {
        size_t m_type;
        void* m_sender{nullptr};
        void* m_receiver{nullptr};
        double m_dt{0};
        int m_phase{0}; //is set when delivering the message, NOT by sender!
    };

    struct MsgInit : public MsgBase { MsgInit(void* s, void* r=nullptr); };
    struct MsgAnnounce : public MsgBase { MsgAnnounce(void* s); };
    struct MsgFrameStart : public MsgBase { MsgFrameStart(void* s, void* r, double dt); };
    struct MsgPollEvents : public MsgBase { MsgPollEvents(void* s, void* r, double dt); };
    struct MsgUpdate : public MsgBase { MsgUpdate(void* s, void* r, double dt); double m_dt; };
    struct MsgPrepareNextFrame : public MsgBase { MsgPrepareNextFrame(void* s, void* r, double dt); };
    struct MsgRenderNextFrame : public MsgBase { MsgRenderNextFrame(void* s, void* r, double dt);  };
    struct MsgRecordNextFrame : public MsgBase { MsgRecordNextFrame(void* s, void* r, double dt ); };
    struct MsgPresentNextFrame : public MsgBase { MsgPresentNextFrame(void* s, void* r, double dt);  };
    struct MsgFrameEnd : public MsgBase { MsgFrameEnd(void* s, void* r, double dt); };
    struct MsgDelete : public MsgBase { MsgDelete(void* s, void* r, double dt ); void* m_ptr; uint64_t m_id; };
    struct MsgMouseMove : public MsgBase { MsgMouseMove(void* s, void* r, double dt, int x, int y); int m_x; int m_y; };
    struct MsgMouseButtonDown : public MsgBase { MsgMouseButtonDown(void* s, void* r, double dt, int button); int m_button; };
    struct MsgMouseButtonUp : public MsgBase { MsgMouseButtonUp(void* s, void* r, double dt, int button);  int m_button; };
    struct MsgMouseButtonRepeat : public MsgBase { MsgMouseButtonRepeat(void* s, void* r, double dt, int button);  int m_button; };
    struct MsgMouseWheel : public MsgBase { MsgMouseWheel(void* s, void* r, double dt, int x, int y);  int m_x; int m_y; };
    struct MsgKeyDown : public MsgBase { MsgKeyDown(void* s, void* r, double dt, int key);  int m_key; };
    struct MsgKeyUp : public MsgBase { MsgKeyUp(void* s, void* r, double dt, int key);  int m_key; };
    struct MsgKeyRepeat : public MsgBase { MsgKeyRepeat(void* s, void* r, double dt, int key); int m_key; };
    struct MsgSDL : public MsgBase { MsgSDL(void* s, void* r, double dt, SDL_Event event ); double m_dt; SDL_Event m_event; };
	struct MsgQuit : public MsgBase { MsgQuit(void* s, void* r=nullptr); };
	
    struct MsgFileLoadObject : public MsgBase { MsgFileLoadObject(void* s, void* r, std::string txtName, std::string objName); std::string m_txtName; std::string m_objName; };
	struct MsgObjectCreate : public MsgBase { MsgObjectCreate(void* s, void* r, vecs::Handle handle); vecs::Handle m_handle; };
	
	struct MsgTextureCreate : public MsgBase { MsgTextureCreate(void* s, void* r, void *pixels, vecs::Handle handle); void* m_pixels; vecs::Handle m_handle; };
    struct MsgTextureDestroy : public MsgBase { MsgTextureDestroy(void* s, void* r, vecs::Handle handle); vecs::Handle m_handle; };
    struct MsgGeometryCreate : public MsgBase { MsgGeometryCreate(void* s, void* r, vecs::Handle handle); vecs::Handle m_handle; };
    struct MsgGeometryDestroy : public MsgBase { MsgGeometryDestroy(void* s, void* r, vecs::Handle handle); vecs::Handle m_handle; };
    

    struct Message {
        static const size_t MAX_SIZE = 128;

        template<typename T>
            requires (std::is_base_of_v<MsgBase, T> && sizeof(T) <= MAX_SIZE)
        Message(const T&& msg ) {
            m_typeID = std::type_index(typeid(T)).hash_code();
            std::memcpy(m_data, &msg, sizeof(T));
        };

        auto GetType() -> size_t { return reinterpret_cast<MsgBase*>(m_data)->m_type; };
        auto GetSender() -> void* { return reinterpret_cast<MsgBase*>(m_data)->m_sender; };
        auto GetReceiver() -> void* { return reinterpret_cast<MsgBase*>(m_data)->m_receiver; };
        auto GetDt() -> double { return reinterpret_cast<MsgBase*>(m_data)->m_dt; };
        void SetPhase(int phase) { reinterpret_cast<MsgBase*>(m_data)->m_phase = phase; };
        auto GetPhase() -> int { return reinterpret_cast<MsgBase*>(m_data)->m_phase; };

        template<typename T>
            requires (std::is_base_of_v<MsgBase, T> && sizeof(T) <= MAX_SIZE)
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
    class System {

    public:
        System( std::string systemName, Engine<ATYPE>& engine );
        virtual ~System();
        auto GetName() -> std::string { return m_name; };

    protected:
        std::string 			m_name;
        Engine<ATYPE>& 			m_engine;
		vecs::Registry<ATYPE>&	m_registry;
    };

};

namespace std {

    template<> struct hash<vve::System<vve::ENGINETYPE_SEQUENTIAL>> {
        size_t operator()(vve::System<vve::ENGINETYPE_SEQUENTIAL> & system) ; 
    };

    template<> struct hash<vve::System<vve::ENGINETYPE_PARALLEL>> {
        size_t operator()(vve::System<vve::ENGINETYPE_PARALLEL> & system) ; 
    };
};

