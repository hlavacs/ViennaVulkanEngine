#pragma once

#include "VEInclude.h"
#include "VESystem.h"
#include "VECS.h"

namespace vve {

	//-------------------------------------------------------------------------------------------------------

    struct SoundState {
        std::string m_filepath{};
        int m_cont{0};
        SDL_AudioSpec m_wavSpec;
        Uint32 m_playLength{0};
        Uint32 m_playedLength{0};
        Uint32 m_wavLength{0};
        Uint8 *m_wavBuffer{nullptr};
		Uint32 m_volume{100};
        SDL_AudioDeviceID m_deviceId{0};
    };

	class SoundManager : public System {

	public:
        SoundManager(std::string systemName, Engine& engine);
    	~SoundManager() {};
		int GetVolume() { return (int)m_volume; }

        static inline SoundManager* m_soundManager{nullptr};
        static inline void SDL2AudioCallback(void *userdata, Uint8 *stream, int len);

    private:
        bool OnUpdate(Message message);
        bool OnPlaySound(Message& message);
        bool OnSetVolume(Message& message);
        bool OnQuit(Message message);
        void AudioCallback(vecs::Handle handle, Uint8 *stream, int len);
		float m_volume{50.0f};
    };

};  // namespace vve
