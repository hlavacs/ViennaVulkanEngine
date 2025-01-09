#pragma once

namespace vve {

    class AssetManager : public System {

    public:
        AssetManager(std::string systemName, Engine& engine );
        virtual ~AssetManager();
		auto LoadTexture(Name filenName)-> TextureHandle;
		auto LoadOBJ(Name filenName) -> GeometryHandle;
		auto LoadGLTF(Name filenName) -> vecs::Handle;
		auto GetAsset(Name filenName) -> vecs::Handle;

    private:
		bool OnInit(Message message);
		bool OnObjectLoad(Message message);
		bool OnQuit( Message message );
		auto GetAssetHandle(Name name) -> vecs::Handle&;	

		std::unordered_map<Name, vecs::Handle> m_handleMap;
    };

};  // namespace vve

