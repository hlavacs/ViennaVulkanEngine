#pragma once
#include <filesystem>


namespace vve {

    class AssetManager : public System {

    public:
        /**
         * @brief Constructor for the AssetManager class
         * @param systemName Name of the system
         * @param engine Reference to the engine
         */
        AssetManager(std::string systemName, Engine& engine );

        /**
         * @brief Destructor for the AssetManager class
         */
        virtual ~AssetManager();

    private:
		/**
		 * @brief Handle scene creation message
		 * @param message Message containing scene creation data
		 * @return True if message was handled
		 */
		bool OnSceneCreate(Message& message);

		/**
		 * @brief Handle scene load message
		 * @param message Message containing scene load data
		 * @return True if message was handled
		 */
		bool OnSceneLoad(Message& message);

		/**
		 * @brief Handle scene release message
		 * @param message Message containing scene release data
		 * @return True if message was handled
		 */
		bool OnSceneRelease(Message& message);

		/**
		 * @brief Handle object creation message
		 * @param message Message containing object creation data
		 * @return True if message was handled
		 */
		bool OnObjectCreate(Message message);

		/**
		 * @brief Handle texture creation message
		 * @param message Message containing texture creation data
		 * @return True if message was handled
		 */
		bool OnTextureCreate(Message message);

		/**
		 * @brief Handle texture release message
		 * @param message Message containing texture release data
		 * @return True if message was handled
		 */
		bool OnTextureRelease(Message message);

		/**
		 * @brief Handle play sound message
		 * @param message Message containing sound playback data
		 * @return True if message was handled
		 */
		bool OnPlaySound(Message& message);

		/**
		 * @brief Load texture data from file
		 * @param handle Handle to the texture to load
		 * @return Pointer to loaded texture data
		 */
		auto LoadTexture(TextureHandle handle) -> stbi_uc*;

		/**
		 * @brief Load a scene from file
		 * @param sceneName Filename of the scene
		 * @param scene Pointer to the Assimp scene structure
		 * @return True if scene was loaded successfully
		 */
		bool SceneLoad(Filename sceneName, const C_STRUCT aiScene* scene);

	private:
		std::unordered_multimap<std::filesystem::path, std::string> m_fileNameMap; //from path to string
    };

};  // namespace vve

