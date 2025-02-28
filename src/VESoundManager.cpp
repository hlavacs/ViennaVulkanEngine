#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	SoundManager::SoundManager(std::string systemName, Engine& engine ) : System(systemName, engine){
		m_engine.RegisterCallback( { 
 		  	{this, 0, "ANNOUNCE", [this](Message& message){ return OnAnnounce(message);} },
 		  	{this, 0, "UPDATE", [this](Message& message){ return OnUpdate(message);} },
 		  	{this, 0, "PLAY_SOUND", [this](Message& message){ return OnPlaySound(message);} },
 		  	{this, 0, "QUIT", [this](Message& message){ return OnQuit(message);} },
		} );
	};

    bool SoundManager::OnAnnounce(Message message) {
		auto msg = message.template GetData<MsgAnnounce>();
		return false;
	}

    void my_audio_callback(void *userdata, Uint8 *stream, int len) {
        PlayingSound* sound = (PlayingSound*)userdata;
        if(sound->m_playLength == 0 ) {
            return;
        }
        len = ( len > sound->m_playLength ? sound->m_playLength : len );
        SDL_memcpy(stream, sound->m_wavBuffer + sound->m_playedLength, len);
        sound->m_playedLength += len;
        sound->m_playLength -= len;
    }

    bool SoundManager::OnPlaySound(Message message) {
        auto msg = message.template GetData<MsgPlaySound>();
        Name filepath = msg.m_filepath;
        int cont = msg.m_cont;

        PlayingSound& sound = m_sounds[filepath];
        if(cont == 1 || cont == 2) {
            sound.m_cont = cont;
            if( sound.m_wavBuffer == nullptr) {
                SDL_LoadWAV(filepath().c_str(), &sound.m_wavSpec, &sound.m_wavBuffer, &sound.m_wavLength);
                sound.m_wavSpec.callback = my_audio_callback;
                sound.m_wavSpec.userdata = &sound;
                sound.m_deviceId = SDL_OpenAudioDevice(NULL, 0, &sound.m_wavSpec, NULL, 0);
            }
            sound.m_playLength = sound.m_wavLength;
            sound.m_playedLength = 0;
            int success = SDL_QueueAudio(sound.m_deviceId, sound.m_wavBuffer, sound.m_wavLength);
            SDL_PauseAudioDevice(sound.m_deviceId, 0);
            return true;
        }

        if(sound.m_cont == 0) return false;
        SDL_PauseAudioDevice(sound.m_deviceId, 1);
        SDL_ClearQueuedAudio(sound.m_deviceId);
        sound.m_cont = 0;
		return false;
    }

    bool SoundManager::OnUpdate(Message message) {
        auto msg = message.template GetData<MsgUpdate>();
        for(auto& [name, sound] : m_sounds) {
            if(sound.m_playLength == 0) {
                if(sound.m_cont == 1) {
                    SDL_PauseAudioDevice(sound.m_deviceId, 1);
                    sound.m_cont = 0;
                }
                if(sound.m_cont == 2) {
                    sound.m_playLength = sound.m_wavLength;
                    sound.m_playedLength = 0;
                    int success = SDL_QueueAudio(sound.m_deviceId, sound.m_wavBuffer, sound.m_wavLength);
                }
            }
        }
		return false;
    }

    bool SoundManager::OnQuit(Message message) {
        auto msg = message.template GetData<MsgQuit>();
        for(auto& [name, sound] : m_sounds) {
            if(sound.m_deviceId) SDL_CloseAudioDevice(sound.m_deviceId);
            SDL_FreeWAV(sound.m_wavBuffer);
        }
		return false;
    }

}   // namespace vve