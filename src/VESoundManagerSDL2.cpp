#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	/**
	 * @brief Constructs a sound manager using SDL2 audio and registers event callbacks
	 * @param systemName Name of the sound manager system
	 * @param engine Reference to the engine instance
	 */
	SoundManager::SoundManager(std::string systemName, Engine& engine ) : System(systemName, engine){
		m_engine.RegisterCallbacks( { 
 		  	{this,    0, "UPDATE", [this](Message& message){ return OnUpdate(message);} },
 		  	{this, 1000, "PLAY_SOUND", [this](Message& message){ return OnPlaySound(message);} },
 		  	{this,    0, "SET_VOLUME", [this](Message& message){ return OnSetVolume(message);} },
 		  	{this,    0, "QUIT", [this](Message& message){ return OnQuit(message);} }
		} );
        SoundManager::m_soundManager = this;
    };

    /**
     * @brief Static wrapper for SDL audio callback that delegates to instance method
     * @param userdata User data pointer containing sound handle
     * @param stream Audio stream buffer to fill
     * @param len Length of audio data requested
     */
    void SoundManager::SDL3AudioCallback(void *userdata, Uint8 *stream, int len) {
        size_t i = reinterpret_cast<std::uintptr_t>(userdata);
        SoundManager::m_soundManager->AudioCallback(vecs::Handle{i}, stream, len);
    }

    /**
     * @brief Fills audio stream with sound data and applies volume scaling
     * @param handle Handle to the sound being played
     * @param stream Audio stream buffer to fill
     * @param len Length of audio data requested
     */
    void SoundManager::AudioCallback(vecs::Handle handle, Uint8 *stream, int len) {
        auto sound = m_registry.template Get<SoundState&>(handle);
        if(sound().m_playLength == 0 ) { return; }
        len = ( len > sound().m_playLength ? sound().m_playLength : len );
        SDL_memcpy(stream, sound().m_wavBuffer + sound().m_playedLength, len);
		float vol = sound().m_volume * m_volume / (10000.0f);
		for( int i=0; i<len; i+=2) {
			if(sound().m_wavSpec.format == AUDIO_U16LSB) *(reinterpret_cast<uint16_t*>(&stream[i])) *= vol;
			if(sound().m_wavSpec.format == AUDIO_S16LSB) *(reinterpret_cast<int16_t*>(&stream[i])) *= vol;
			if(sound().m_wavSpec.format == AUDIO_S32LSB) *(reinterpret_cast<int32_t*>(&stream[i])) *= vol;
		}
        sound().m_playedLength += len;
        sound().m_playLength -= len;
    }

    /**
     * @brief Plays a sound from a file or stops it based on continuation parameter
     * @param message Message containing sound playback parameters
     * @return true if sound was started, false otherwise
     */
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
                sound().m_wavSpec.callback = SDL3AudioCallback;               
                sound().m_wavSpec.userdata = reinterpret_cast<void*>(soundHandle().GetValue()); // turn handle to void*
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

	/**
	 * @brief Sets the global volume for all sounds
	 * @param message Message containing volume level
	 * @return false to continue message propagation
	 */
	bool SoundManager::OnSetVolume(Message& message) {
		auto msg = message.template GetData<MsgSetVolume>();
		m_volume = msg.m_volume;
		return false;
	}

    /**
     * @brief Updates sound playback states and handles looping sounds
     * @param message Update message
     * @return false to continue message propagation
     */
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

    /**
     * @brief Cleans up all sound resources when shutting down
     * @param message Quit message
     * @return false to continue message propagation
     */
    bool SoundManager::OnQuit(Message message) {
        auto msg = message.template GetData<MsgQuit>();
        for(auto sound : m_registry.template GetView<SoundState&>()) {
            if(sound().m_deviceId) SDL_CloseAudioDevice(sound().m_deviceId);
            SDL_FreeWAV(sound().m_wavBuffer);
        }
		return false;
    }

}   // namespace vve