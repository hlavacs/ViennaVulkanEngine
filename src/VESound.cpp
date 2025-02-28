#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	Sound::Sound(std::string systemName, Engine& engine ) : System(systemName, engine){
		m_engine.RegisterCallback( { 
 		  	{this, 0, "ANNOUNCE", [this](Message& message){ return OnAnnounce(message);} },
 		  	{this, 0, "UPDATE", [this](Message& message){ return OnUpdate(message);} },
 		  	{this, 0, "PLAY_SOUND", [this](Message& message){ return OnPlaySound(message);} },
 		  	{this, 0, "QUIT", [this](Message& message){ return OnQuit(message);} },
		} );
	};

    bool Sound::OnAnnounce(Message message) {
		auto msg = message.template GetData<MsgAnnounce>();
		return false;
	}

    bool Sound::OnPlaySound(Message message) {
        auto msg = message.template GetData<MsgPlaySound>();
        Name filepath = msg.m_filepath;
        int cont = msg.m_cont;

        PlayingSound& sound = m_sounds[filepath];
        if(cont == 1 || cont == 2) {
            sound.m_cont = cont;
            if( sound.m_wavBuffer == nullptr) SDL_LoadWAV(filepath().c_str(), &sound.m_wavSpec, &sound.m_wavBuffer, &sound.m_wavLength);
            sound.m_deviceId = SDL_OpenAudioDevice(NULL, 0, &sound.m_wavSpec, NULL, 0);
            int success = SDL_QueueAudio(sound.m_deviceId, sound.m_wavBuffer, sound.m_wavLength);
            SDL_PauseAudioDevice(sound.m_deviceId, 0);
            return true;
        }

        if(sound.m_cont == 0) return false;
        sound.m_cont = 0;
        SDL_CloseAudioDevice(sound.m_deviceId);
		return false;
    }

    bool Sound::OnUpdate(Message message) {
        auto msg = message.template GetData<MsgUpdate>();
		return false;
    }

    bool Sound::OnQuit(Message message) {
        auto msg = message.template GetData<MsgQuit>();
        for(auto& [name, sound] : m_sounds) {
            if(sound.m_deviceId) SDL_CloseAudioDevice(sound.m_deviceId);
            SDL_FreeWAV(sound.m_wavBuffer);
        }
		return false;
    }

}   // namespace vve