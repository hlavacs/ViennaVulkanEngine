
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {
	

	//-------------------------------------------------------------------------------------------------------

    AssetManager::AssetManager(std::string systemName, Engine& engine ) : System{systemName, engine } {
		engine.RegisterCallback( { 
			{this,  2000, "INIT", [this](Message message){ return OnInit(message);} },
			{this,     0, "OBJECT_LOAD", [this](Message message){ return OnObjectLoad(message);} },
			{this,     0, "QUIT", [this](Message message){ return OnQuit(message);} },
		} );
	}

    AssetManager::~AssetManager() {}

    bool AssetManager::OnInit(Message message) {
		return false;
	}

    bool AssetManager::OnObjectLoad(Message message) {
		return false;
		auto msg = message.template GetData<MsgObjectLoad>();
		auto nHandle = msg.m_object;
		auto pHandle = msg.m_parent;
		auto tHandle = LoadTexture(Name{msg.m_txtName});
		auto oHandle = LoadOBJ(Name{msg.m_objName});
		return false;
	}

	bool AssetManager::OnQuit( Message message ) {
		return false;
	}

	auto AssetManager::LoadTexture(Name fileName) -> vecs::Handle {
		if( m_handleMap.contains(fileName) ) return m_handleMap[fileName];

		int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(fileName().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        if (!pixels) { return {}; }

		vh::Texture texture{texWidth, texHeight, imageSize, pixels};
		auto handle = m_registry.Insert(fileName, texture);
		m_handleMap[fileName] = handle;
		return handle;
	}

	auto AssetManager::LoadOBJ(Name fileName) -> vecs::Handle {
		if( m_handleMap.contains(fileName) ) return m_handleMap[fileName];
		
		vh::Geometry geometry{};
		vh::loadModel(fileName(), geometry);
		auto handle = m_registry.Insert(fileName, geometry);
		m_handleMap[fileName] = handle;
		return handle;
	}
	
	auto AssetManager::LoadGLTF(Name fileName) -> vecs::Handle {
		//m_files[fileName] = handle;
		return {};
	}

	auto AssetManager::GetAsset(Name fileName) -> vecs::Handle {
		if( !m_handleMap.contains(fileName) ) return {};
		return m_handleMap[fileName]; 
	}

	auto AssetManager::GetAssetHandle(Name name) -> vecs::Handle& { 
		return m_handleMap[name]; 
	}


};  // namespace vve




