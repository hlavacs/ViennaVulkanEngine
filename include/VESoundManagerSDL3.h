#pragma once

namespace vve {

	//-------------------------------------------------------------------------------------------------------

    /**
     * @brief Sound state for SDL3
     */
    struct SoundState {
        std::string     m_filepath{};
        int             m_cont{0};
		Uint32          m_volume{MIX_MAX_VOLUME};
        int             m_channel;
        Mix_Chunk *     m_chunk{nullptr};
        Mix_Music *     m_music{nullptr};
        SDL_AudioSpec   m_wavSpec;
        //SDL_AudioDeviceID m_deviceId{0};
    };

	/**
	 * @brief Sound manager implementation using SDL3
	 */
	class SoundManager : public System {

	public:
        /**
         * @brief Constructor for Sound Manager
         * @param systemName Name of the system
         * @param engine Reference to the engine
         */
        SoundManager(std::string systemName, Engine& engine);
    	/**
    	 * @brief Destructor for Sound Manager
    	 */
    	~SoundManager() {};
		/**
		 * @brief Get current volume
		 * @return Volume level
		 */
		int GetVolume() { return (int)m_volume; }

        static inline SoundManager* m_soundManager{nullptr};

    private:
        bool OnUpdate(Message message);
        bool OnPlaySound(Message& message);
        bool OnSetVolume(Message& message);
        bool OnQuit(Message message);
        void AudioCallback(vecs::Handle handle, Uint8 *stream, int len);
		float m_volume{50.0f};
        MIX_InitFlags m_availableCodecs{0};
    };

};  // namespace vve
