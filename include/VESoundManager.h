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
        Uint32 m_playLength{0};
        Uint32 m_playedLength{0};
        Uint32 m_wavLength{0};
        Uint8 *m_wavBuffer{nullptr};
        SDL_AudioDeviceID m_deviceId{0};
    };

	class SoundManager : public System {

	public:
        SoundManager(std::string systemName, Engine& engine);
    	~SoundManager() {};

    private:
        std::map<Filename, PlayingSound> m_sounds;

        bool OnAnnounce(Message message);
        bool OnUpdate(Message message);
        bool OnPlaySound(Message message);
        bool OnQuit(Message message);
    };

};  // namespace vve
