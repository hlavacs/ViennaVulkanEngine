
#include "VEInclude.h"
#include "VHInclude.h"
#include "VHVulkan.h"
#include "VESystem.h"
#include "VEEngine.h"
#include "VESceneManager.h"

namespace vve {
	

	//-------------------------------------------------------------------------------------------------------

   	template<ArchitectureType ATYPE>
    SceneManager<ATYPE>::SceneManager(std::string systemName, Engine<ATYPE>* engine ) : System<ATYPE>{systemName, engine } {
		engine->RegisterCallback( { 
			{this, 0, "INIT", [this](Message message){this->OnInit(message);} },
			{this, std::numeric_limits<int>::max(), "UPDATE", [this](Message message){this->OnUpdate(message);} },
		} );
	}

   	template<ArchitectureType ATYPE>
    SceneManager<ATYPE>::~SceneManager() {}

   	template<ArchitectureType ATYPE>
    void SceneManager<ATYPE>::OnInit(Message message) {
		m_registry = &m_engine->GetRegistry();
		m_handleMap[m_rootName] = m_registry->Insert(SceneNodeWrapper{}); //insert root node
	}

   	template<ArchitectureType ATYPE>
    void SceneManager<ATYPE>::OnUpdate(Message message) {
		for( auto [handle, node] : m_registry->template GetView<vecs::Handle, SceneNodeWrapper&>() ) {
			mat4_t parentToWorldM{1.0};

			if( node().m_parent.IsValid() ) {
				auto& parent = m_registry->template Get<SceneNodeWrapper&>(node().m_parent);
				parentToWorldM = parent().m_localToWorldM;
			}
			node().m_localToWorldM = parentToWorldM * node().m_localToParentT.Matrix();
		}
	}

   	template<ArchitectureType ATYPE>
	auto SceneManager<ATYPE>::LoadTexture(std::string filenName) -> vecs::Handle {
		int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(filenName.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        if (!pixels) { return {}; }

		vh::Texture texture{texWidth, texHeight, imageSize};
		auto handle = m_registry->Insert(filenName, texture);
		m_handleMap[filenName] = handle;
		m_engine->SendMessage( MsgTextureCreate{this, nullptr, pixels, handle} );
		stbi_image_free(pixels);
		return handle;
	}

   	template<ArchitectureType ATYPE>
	auto SceneManager<ATYPE>::LoadOBJ(std::string fileName) -> vecs::Handle {
		vh::Geometry geometry;
		vh::loadModel(fileName, geometry);
		auto handle = m_registry->Insert(fileName, geometry);
		m_handleMap[fileName] = handle;
		m_engine->SendMessage( MsgGeometryCreate{this, nullptr, handle} );
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

