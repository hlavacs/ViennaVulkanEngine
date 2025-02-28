#pragma once

#include "VEInclude.h"
#include "VESystem.h"
#include "VECS.h"

namespace vve {

	//-------------------------------------------------------------------------------------------------------

    struct PlayingSound {
        std::string m_filepath{};
        int m_cont{0};
        SDL_AudioSpec m_wavSpec;
        Uint32 m_wavLength{0};
        Uint8 *m_wavBuffer{nullptr};
        SDL_AudioDeviceID m_deviceId{0};
    };

	class Sound : public System {

	public:
        Sound(std::string systemName, Engine& engine);
    	~Sound() {};

    private:
        std::map<Name, PlayingSound> m_sounds;

        bool OnAnnounce(Message message);
        bool OnUpdate(Message message);
        bool OnPlaySound(Message message);
        bool OnQuit(Message message);
    };

};  // namespace vve
