
#include "VEInclude.h"
#include "VHInclude.h"
#include "VHVulkan.h"
#include "VESystem.h"
#include "VEEngine.h"
#include "VESceneManager.h"

namespace vve {
	
	MsgTextureCreate::MsgTextureCreate(void* s, void* r, void *pixels, vecs::Handle handle) : MsgBase{MsgType::TEXTURE_CREATE, s, r}, m_pixels{pixels}, m_handle{handle} {};
    MsgTextureDestroy::MsgTextureDestroy(void* s, void* r, vecs::Handle handle) : MsgBase{MsgType::TEXTURE_DESTROY, s, r}, m_handle{handle} {};
	MsgGeometryCreate::MsgGeometryCreate(void* s, void* r, vecs::Handle handle) : MsgBase{MsgType::GEOMETRY_CREATE, s, r}, m_handle{handle} {};
    MsgGeometryDestroy::MsgGeometryDestroy(void* s, void* r, vecs::Handle handle) : MsgBase{MsgType::GEOMETRY_DESTROY, s, r}, m_handle{handle} {};

   	template<ArchitectureType ATYPE>
    SceneManager<ATYPE>::SceneManager(std::string systemName, Engine<ATYPE>* engine ) 
		: System<ATYPE>{systemName, engine } {
	}

   	template<ArchitectureType ATYPE>
    SceneManager<ATYPE>::~SceneManager() {}

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
	auto SceneManager<ATYPE>::Transform::GetMatrix() -> glm::mat4 {
		glm::mat4 matrix = glm::mat4(1.0f);
		matrix = glm::translate(matrix, m_position);
		matrix = matrix * glm::mat4_cast(m_rotation);
		matrix = glm::scale(matrix, m_scale);
		return matrix;
	}

   	template<ArchitectureType ATYPE>
	auto SceneManager<ATYPE>::GetAsset(std::string filename) -> vecs::Handle {
		if( !m_files.contains(filename) ) return {};
		return m_files[filename]; 
	}

    template class SceneManager<ENGINETYPE_SEQUENTIAL>;
    template class SceneManager<ENGINETYPE_PARALLEL>;

};  // namespace vve

