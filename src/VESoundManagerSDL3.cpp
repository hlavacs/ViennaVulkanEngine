#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	/**
	 * @brief Constructs a sound manager using SDL3 Mixer and registers event callbacks
	 * @param systemName Name of the sound manager system
	 * @param engine Reference to the engine instance
	 */
	SoundManager::SoundManager(std::string systemName, Engine& engine ) : System(systemName, engine){
		m_engine.RegisterCallbacks( { 
 		  	{this, 1000, "PLAY_SOUND", [this](Message& message){ return OnPlaySound(message);} },
 		  	{this,    0, "SET_VOLUME", [this](Message& message){ return OnSetVolume(message);} },
 		  	{this,    0, "QUIT", [this](Message& message){ return OnQuit(message);} }
		} );
        SoundManager::m_soundManager = this;
        m_availableCodecs = Mix_Init(MIX_INIT_WAVPACK | MIX_INIT_MP3 | MIX_INIT_OGG | MIX_INIT_FLAC );
        Mix_OpenAudio(0, NULL);
        Mix_AllocateChannels(128);  //audio channels
    };

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

        if(cont >= 1 || cont == -1) {
            sound().m_cont = cont;
            if( sound().m_chunk == nullptr) {
                sound().m_chunk = Mix_LoadWAV(filepath().c_str());
            }
            sound().m_channel = Mix_PlayChannel(-1, sound().m_chunk, cont == -1 ? cont : cont -1 );
            return true;
        }
        Mix_HaltChannel(sound().m_channel);
		return false;
    }

	/**
	 * @brief Sets the global volume for all sound channels and music
	 * @param message Message containing volume level
	 * @return false to continue message propagation
	 */
	bool SoundManager::OnSetVolume(Message& message) {
		auto msg = message.template GetData<MsgSetVolume>();
		m_volume = static_cast<float>(msg.m_volume);
        Mix_Volume(-1, static_cast<int>(m_volume));
        Mix_VolumeMusic(static_cast<int>(m_volume));
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
            if(sound().m_chunk) Mix_FreeChunk(sound().m_chunk);
            if(sound().m_music) Mix_FreeMusic(sound().m_music);
        }
        Mix_Quit();
		return false;
    }

}   // namespace vve