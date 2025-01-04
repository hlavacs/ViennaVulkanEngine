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
		m_handleMap[Name{m_rootName}] = m_registry.Insert(
												Name{m_rootName},
												Parent{},
												Children{},
												Position{glm::vec3(0.0f, 0.0f, 0.0f)},
												LocalToWorldMatrix{mat4_t{1.0f}},
												SceneNode{}); //insert root node

		// Create camera
		auto window = m_engine.GetWindow(m_windowName);
        auto view = glm::inverse( glm::lookAt(glm::vec3(4.0f, 1.9f, 3.8f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)) );
		auto cHandle = m_registry.Insert(
								Name(m_cameraName),
								Parent{GetHandle(Name{m_rootName})},
								Camera{(float)window->GetWidth() / (float)window->GetHeight()}, 
								Position{glm::vec3(view[3])}, 
								Orientation{glm::quat_cast(view)}, 
								Scale{vec3_t{1.0f, 1.0f, 1.0f}}, 
								LocalToParentMatrix{mat4_t{1.0f}}, 
								LocalToWorldMatrix{mat4_t{1.0f}}, 
								ViewMatrix{view},
								Transform{glm::vec3(view[3]), glm::quat_cast(view)}, 
								SceneNode{{glm::vec3(view[3]), glm::quat_cast(view)}, GetHandle(Name{m_rootName})} );

		m_registry.Get<Children&>(GetHandle(Name{m_rootName}))().push_back(cHandle);

	}

    void SceneManager::OnUpdate(Message message) {
		auto children = m_registry.template Get<Children&>(GetHandle(Name{m_rootName}));

		auto update = [](auto& registry, mat4_t& parentToWorld, vecs::Handle& handle, auto& self) -> void {
			auto [p, o, s, LtoP, LtoW] = registry.template Get<Position, Orientation, Scale, LocalToParentMatrix&, LocalToWorldMatrix&>(handle);
			LtoP = glm::translate(mat4_t{1.0f}, p()) * glm::mat4_cast(o()) * glm::scale(mat4_t{1.0f}, s());
			LtoW = parentToWorld * LtoP();

			if( registry.template Has<ViewMatrix&>(handle) ) {
				registry.template Put(handle, ViewMatrix{glm::inverse(LtoW())});
			}

			if( registry.template Has<Children&>(handle) ) {
				auto& children = registry.template Get<Children&>(handle);
				for( auto child : children() ) {
					self(registry, LtoW, child, self);
				}
			}
		};

		for( auto child : children() ) {
			update(m_registry, LocalToWorldMatrix{mat4_t{1.0f}}, child, update);
		}
	}

    void SceneManager::OnLoadObject(Message message) {
		auto msg = message.template GetData<MsgFileLoadObject>();
		auto tHandle = LoadTexture(Name{msg.m_txtName});
		auto oHandle = LoadOBJ(Name{msg.m_objName});

		auto nHandle = m_registry.Insert(
									Name(msg.m_objName),
									Parent{GetHandle(Name{m_rootName})},
									Children{},
									Position{glm::vec3(-0.5f, 0.5f, 0.5f)}, 
									Orientation{}, 
									Scale{vec3_t{1.0f, 1.0f, 1.0f}}, 
									LocalToParentMatrix{mat4_t{1.0f}}, 
									LocalToWorldMatrix{mat4_t{1.0f}},
									GeometryHandle{oHandle}, 
									TextureHandle{tHandle}, 
									SceneNode{} );

		m_registry.Get<Children&>(GetHandle(Name{m_rootName}))().push_back(nHandle);

		m_engine.SendMessage( MsgObjectCreate{this, nullptr, nHandle} );
	}

	auto SceneManager::LoadTexture(Name fileName) -> vecs::Handle {
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

	auto SceneManager::LoadOBJ(Name fileName) -> vecs::Handle {
		if( m_handleMap.contains(fileName) ) return m_handleMap[fileName];
		
		vh::Geometry geometry{};
		vh::loadModel(fileName(), geometry);
		auto handle = m_registry.Insert(fileName, geometry);
		m_handleMap[fileName] = handle;
		return handle;
	}
	
	auto SceneManager::LoadGLTF(Name fileName) -> vecs::Handle {
		//m_files[fileName] = handle;
		return {};
	}

	auto SceneManager::GetAsset(Name fileName) -> vecs::Handle {
		if( !m_handleMap.contains(fileName) ) return {};
		return m_handleMap[fileName]; 
	}

	auto SceneManager::GetHandle(Name name) -> vecs::Handle& { 
		return m_handleMap[name]; 
	}	


};  // namespace vve

namespace std {
	size_t hash<vve::Name>::operator()(vve::Name const& name) const {
		return std::hash<std::string>{}(name());
	}

}


