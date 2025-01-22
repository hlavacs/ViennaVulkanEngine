#pragma once
#include <filesystem>


namespace vve {

    class AssetManager : public System {

    public:
        AssetManager(std::string systemName, Engine& engine );
        virtual ~AssetManager();
		auto LoadTexture(Name filenName)-> TextureHandle;
		auto GetAsset(Name filenName) -> vecs::Handle;
		auto GetAssetHandle(Name name) -> vecs::Handle&;	

    private:
		bool OnInit(Message message);
		bool OnSceneLoad(Message& message);
		bool OnSceneLoad2(Message& message);
		bool OnObjectCreate(Message message);
		bool OnTextureCreate(Message message);
		bool OnQuit( Message message );

		std::unordered_map<Name, vecs::Handle> m_handleMap;
		std::unordered_multimap<std::filesystem::path, Name> m_fileNameMap;
    };

};  // namespace vve

