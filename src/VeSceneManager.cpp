#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {
	

	//-------------------------------------------------------------------------------------------------------

    SceneManager::SceneManager(std::string systemName, Engine& engine ) : System{systemName, engine } {
		engine.RegisterCallback( { 
			{this,  2000, "INIT", [this](Message message){OnInit(message);} },
			{this, std::numeric_limits<int>::max(), "UPDATE", [this](Message message){OnUpdate(message);} },
			{this, std::numeric_limits<int>::max(), "FILE_LOAD_OBJECT", [this](Message message){OnLoadObject(message);} },
		} );
	}

    SceneManager::~SceneManager() {}

    void SceneManager::OnInit(Message message) {
		m_handleMap[m_rootName] = m_registry.Insert(SceneNodeWrapper{}); //insert root node
	}

    void SceneManager::OnUpdate(Message message) {
		auto node = m_registry.template Get<SceneNodeWrapper&>(m_handleMap[m_rootName]);

		auto func = [&](mat4_t& parentTransform, SceneNodeWrapper& node, auto& self) -> void {
			auto& transform = node().m_localToParentT;
			node().m_localToWorldM = parentTransform * transform.Matrix();
			for( auto handle : node().m_children ) {
				auto child = m_registry.template Get<SceneNodeWrapper&>(handle);
				self(node().m_localToWorldM, child, self);
			}
		};

		for( auto handle : node().m_children ) {
			auto child = m_registry.template Get<SceneNodeWrapper&>(handle);
			func(node().m_localToWorldM, child, func);
		}
	}

    void SceneManager::OnLoadObject(Message message) {
		auto msg = message.template GetData<MsgFileLoadObject>();
		auto tHandle = LoadTexture(msg.m_txtName);
		auto oHandle = LoadOBJ(msg.m_objName);
		auto nHandle = m_registry.Insert( GeometryHandle{oHandle}, TextureHandle{tHandle}, SceneNodeHandle{m_handleMap[m_rootName]} );
		m_engine.SendMessage( MsgObjectCreate{this, nullptr, nHandle} );
	}

	auto SceneManager::LoadTexture(std::string filenName) -> vecs::Handle {
		if( m_handleMap.contains(filenName) ) return m_handleMap[filenName];

		int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(filenName.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        if (!pixels) { return {}; }

		vh::Texture texture{texWidth, texHeight, imageSize, pixels};
		auto handle = m_registry.Insert(filenName, texture);
		m_handleMap[filenName] = handle;
		return handle;
	}

	auto SceneManager::LoadOBJ(std::string fileName) -> vecs::Handle {
		if( m_handleMap.contains(fileName) ) return m_handleMap[fileName];
		
		vh::Geometry geometry;
		vh::loadModel(fileName, geometry);
		auto handle = m_registry.Insert(fileName, geometry);
		m_handleMap[fileName] = handle;
		return handle;
	}
	
	auto SceneManager::LoadGLTF(std::string filename) -> vecs::Handle {
		//m_files[filenName] = handle;
		return {};
	}

	auto SceneManager::GetAsset(std::string filename) -> vecs::Handle {
		if( !m_handleMap.contains(filename) ) return {};
		return m_handleMap[filename]; 
	}


};  // namespace vve

