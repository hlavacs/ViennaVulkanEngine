#pragma once
#include <filesystem>


namespace vve {

    class AssetManager : public System {

    public:
        AssetManager(std::string systemName, Engine& engine );
        virtual ~AssetManager();

    private:
		bool OnSceneCreate(Message& message);
		bool OnSceneLoad(Message& message);
		bool OnSceneRelease(Message& message);
		bool OnObjectCreate(Message message);
		bool OnTextureCreate(Message message);
		bool OnTextureRelease(Message message);
		bool OnPlaySound(Message& message);
		auto LoadTexture(TextureHandle handle) -> stbi_uc*;
		
		bool SceneLoad(Filename sceneName, const C_STRUCT aiScene* scene);

	private:
		std::unordered_multimap<std::filesystem::path, std::string> m_fileNameMap; //from path to string
    };

};  // namespace vve

