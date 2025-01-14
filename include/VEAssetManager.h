#pragma once

namespace vve {

    class AssetManager : public System {

    public:
        AssetManager(std::string systemName, Engine& engine );
        virtual ~AssetManager();
		auto LoadTexture(Name filenName)-> TextureHandle;
		auto LoadOBJ(Name filenName) -> GeometryHandle;
		auto GetAsset(Name filenName) -> vecs::Handle;
		auto GetAssetHandle(Name name) -> vecs::Handle&;	

    private:
		bool OnInit(Message message);
		bool OnSceneLoad(Message& message);
		bool OnSceneLoad2(Message& message);
		bool OnObjectLoad(Message message);
		bool OnObjectLoad2(Message message);
		bool OnQuit( Message message );

		std::unordered_map<Name, vecs::Handle> m_handleMap;
    };

};  // namespace vve

