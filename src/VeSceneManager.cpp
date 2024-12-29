#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {
	

	//-------------------------------------------------------------------------------------------------------

   	template<ArchitectureType ATYPE>
    SceneManager<ATYPE>::SceneManager(std::string systemName, Engine<ATYPE>& engine ) : System<ATYPE>{systemName, engine } {
		engine.RegisterCallback( { 
			{this,  2000, "INIT", [this](Message message){this->OnInit(message);} },
			{this, std::numeric_limits<int>::max(), "UPDATE", [this](Message message){this->OnUpdate(message);} },
			{this, std::numeric_limits<int>::max(), "FILE_LOAD_OBJECT", [this](Message message){this->OnLoadObject(message);} },
		} );
	}

   	template<ArchitectureType ATYPE>
    SceneManager<ATYPE>::~SceneManager() {}

   	template<ArchitectureType ATYPE>
    void SceneManager<ATYPE>::OnInit(Message message) {
		m_handleMap[m_rootName] = m_registry.Insert(SceneNodeWrapper{}); //insert root node
	}

   	template<ArchitectureType ATYPE>
    void SceneManager<ATYPE>::OnUpdate(Message message) {
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

	template<ArchitectureType ATYPE>
    void SceneManager<ATYPE>::OnLoadObject(Message message) {
		auto msg = message.template GetData<MsgFileLoadObject>();
		auto tHandle = LoadTexture(msg.m_txtName);
		auto oHandle = LoadOBJ(msg.m_objName);
		auto nHandle = m_registry.Insert( GeometryHandle{oHandle}, TextureHandle{tHandle}, SceneNodeHandle{m_handleMap[m_rootName]} );
		m_engine.SendMessage( MsgObjectCreate{this, nullptr, nHandle} );
	}

   	template<ArchitectureType ATYPE>
	auto SceneManager<ATYPE>::LoadTexture(std::string filenName) -> vecs::Handle {
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

   	template<ArchitectureType ATYPE>
	auto SceneManager<ATYPE>::LoadOBJ(std::string fileName) -> vecs::Handle {
		if( m_handleMap.contains(fileName) ) return m_handleMap[fileName];
		
		vh::Geometry geometry;
		vh::loadModel(fileName, geometry);
		auto handle = m_registry.Insert(fileName, geometry);
		m_handleMap[fileName] = handle;
		return handle;
	}
	
   	template<ArchitectureType ATYPE>
	auto SceneManager<ATYPE>::LoadGLTF(std::string filename) -> vecs::Handle {
		//m_files[filenName] = handle;
		return {};
	}

   	template<ArchitectureType ATYPE>
	auto SceneManager<ATYPE>::GetAsset(std::string filename) -> vecs::Handle {
		if( !m_handleMap.contains(filename) ) return {};
		return m_handleMap[filename]; 
	}

    template class SceneManager<ENGINETYPE_SEQUENTIAL>;
    template class SceneManager<ENGINETYPE_PARALLEL>;

};  // namespace vve

