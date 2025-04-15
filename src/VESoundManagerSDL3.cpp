#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	SoundManager::SoundManager(std::string systemName, Engine& engine ) : System(systemName, engine){
		m_engine.RegisterCallbacks( { 
 		  	//{this,    0, "UPDATE", [this](Message& message){ return OnUpdate(message);} },
 		  	{this, 1000, "PLAY_SOUND", [this](Message& message){ return OnPlaySound(message);} },
 		  	{this,    0, "SET_VOLUME", [this](Message& message){ return OnSetVolume(message);} },
 		  	{this,    0, "QUIT", [this](Message& message){ return OnQuit(message);} }
		} );
        SoundManager::m_soundManager = this;
    };


    bool SoundManager::OnPlaySound(Message& message) {
        auto msg = message.template GetData<MsgPlaySound>();
        Filename filepath = msg.m_filepath;
        auto soundHandle = msg.m_soundHandle;
        int cont = msg.m_cont;

        auto sound = m_registry.template Get<SoundState&>(soundHandle);

        if(cont == 1 || cont == 2) {
            sound().m_cont = cont;
            if( sound().m_wavBuffer == nullptr) {
                SDL_LoadWAV(filepath().c_str(), &sound().m_wavSpec, &sound().m_wavBuffer, &sound().m_wavLength);
                sound().m_deviceId = SDL_OpenAudioDevice(0, &sound().m_wavSpec);
            }
			auto s = sound();
            sound().m_playLength = sound().m_wavLength;
            sound().m_playedLength = 0;
			sound().m_volume = msg.m_volume;
            int success = SDL_QueueAudio(sound().m_deviceId, sound().m_wavBuffer, sound().m_wavLength);
            SDL_PauseAudioDevice(sound().m_deviceId, 0);
            return true;
        }

        if(sound().m_cont == 0) return false;
        SDL_PauseAudioDevice(sound().m_deviceId, 1);
        SDL_ClearQueuedAudio(sound().m_deviceId);
        sound().m_cont = 0;
		return false;
    }

	bool SoundManager::OnSetVolume(Message& message) {
		auto msg = message.template GetData<MsgSetVolume>();
		m_volume = msg.m_volume;
		return false;
	}

    bool SoundManager::OnUpdate(Message message) {
        auto msg = message.template GetData<MsgUpdate>();
        for(auto sound : m_registry.template GetView<SoundState&>()) {
            if(sound().m_playLength == 0) {
                if(sound().m_cont == 1) {
                    SDL_PauseAudioDevice(sound().m_deviceId, 1);
                    sound().m_cont = 0;
                }
                if(sound().m_cont == 2) {
                    sound().m_playLength = sound().m_wavLength;
                    sound().m_playedLength = 0;
                    int success = SDL_QueueAudio(sound().m_deviceId, sound().m_wavBuffer, sound().m_wavLength);
                }
            }
        }
		return false;
    }

    bool SoundManager::OnQuit(Message message) {
        auto msg = message.template GetData<MsgQuit>();
        for(auto sound : m_registry.template GetView<SoundState&>()) {
            if(sound().m_deviceId) SDL_CloseAudioDevice(sound().m_deviceId);
            SDL_FreeWAV(sound().m_wavBuffer);
        }
		return false;
    }

}   // namespace vve