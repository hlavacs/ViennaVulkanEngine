
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
			{this, 0, MsgType::INIT, [this](Message message){this->OnInit(message);} },
			{this, std::numeric_limits<int>::max(), MsgType::UPDATE, [this](Message message){this->OnUpdate(message);} },
		} );
	}

   	template<ArchitectureType ATYPE>
    SceneManager<ATYPE>::~SceneManager() {}

   	template<ArchitectureType ATYPE>
    void SceneManager<ATYPE>::OnInit(Message message) {
		m_rootNode = m_engine->GetRegistry().Insert(SceneNode{});
	}

   	template<ArchitectureType ATYPE>
    void SceneManager<ATYPE>::OnUpdate(Message message) {
	}

   	template<ArchitectureType ATYPE>
	auto SceneManager<ATYPE>::LoadTexture(std::string filenName) -> vecs::Handle {
		int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(filenName.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

		vh::Texture texture{texWidth, texHeight, imageSize};
        //m_renderer->CreateTexture(pixels, texWidth, texHeight, imageSize, texture);
		auto handle = m_engine->GetRegistry().Insert(filenName, texture);
		m_files[filenName] = handle;
		m_engine->SendMessage( MsgTextureCreate{this, nullptr, pixels, handle} );
		stbi_image_free(pixels);
		return handle;
	}

   	template<ArchitectureType ATYPE>
	auto SceneManager<ATYPE>::LoadOBJ(std::string fileName) -> vecs::Handle {
		vh::Geometry geometry;
		vh::loadModel(fileName, geometry);
		auto handle = m_engine->GetRegistry().Insert(fileName, geometry);
		m_files[fileName] = handle;
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
		if( !m_files.contains(filename) ) return {};
		return m_files[filename]; 
	}

    template class SceneManager<ENGINETYPE_SEQUENTIAL>;
    template class SceneManager<ENGINETYPE_PARALLEL>;

};  // namespace vve

