#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {
	

	//-------------------------------------------------------------------------------------------------------

    SceneManager::SceneManager(std::string systemName, Engine& engine ) : System{systemName, engine } {
		engine.RegisterCallback( { 
			{this,  2000, "INIT", [this](Message message){ return OnInit(message);} },
			{this, std::numeric_limits<int>::max(), "UPDATE", [this](Message message){ return OnUpdate(message);} },
			{this, std::numeric_limits<int>::max(), "FILE_LOAD_OBJECT", [this](Message message){ return OnLoadObject(message);} },
			{this,      0, "SDL_KEY_DOWN", [this](Message message){ return OnKeyDown(message);} },
			{this,      0, "SDL_KEY_REPEAT", [this](Message message){ return OnKeyRepeat(message);} }		
		} );
	}

    SceneManager::~SceneManager() {}

    bool SceneManager::OnInit(Message message) {
		m_handleMap[Name{m_rootName}] = m_registry.Insert(
												Name{m_rootName},
												ParentHandle{},
												Children{},
												Position{glm::vec3(0.0f, 0.0f, 0.0f)},
												LocalToWorldMatrix{mat4_t{1.0f}} ); //insert root node

		// Create camera
		auto window = m_engine.GetWindow(m_windowName);
        auto view = glm::inverse( glm::lookAt(glm::vec3(4.0f, 1.9f, 3.8f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)) );

		auto nHandle = m_registry.Insert(
								Name(m_cameraNodeName),
								ParentHandle{GetHandle(Name{m_rootName})},
								Children{},
								Position{glm::vec3(view[3])}, 
								Orientation{glm::quat_cast(view)}, 
								Scale{vec3_t{1.0f, 1.0f, 1.0f}}, 
								LocalToParentMatrix{mat4_t{1.0f}}, 
								LocalToWorldMatrix{mat4_t{1.0f}} );

		m_registry.template Get<Children&>(GetHandle(Name{m_rootName}))().push_back(nHandle);

		auto cHandle = m_registry.Insert(
								Name(m_cameraName),
								ParentHandle{nHandle},
								Camera{(real_t)window->GetWidth() / (real_t)window->GetHeight()}, 
								Position{}, 
								Orientation{}, 
								Scale{vec3_t{1.0f, 1.0f, 1.0f}}, 
								LocalToParentMatrix{mat4_t{1.0f}}, 
								LocalToWorldMatrix{mat4_t{1.0f}}, 
								ViewMatrix{view} );

		m_registry.template Get<Children&>(nHandle)().push_back(cHandle);
		return false;
	}

    bool SceneManager::OnUpdate(Message message) {
		auto children = m_registry.template Get<Children&>(GetHandle(Name{m_rootName}));

		auto update = [](auto& registry, mat4_t& parentToWorld, vecs::Handle& handle, auto& self) -> void {
			auto [p, o, s, LtoP, LtoW] = registry.template Get<Position, Orientation, Scale, LocalToParentMatrix&, LocalToWorldMatrix&>(handle);
			LtoP = glm::translate(mat4_t{1.0f}, p()) * glm::mat4_cast(o()) * glm::scale(mat4_t{1.0f}, s());
			LtoW = parentToWorld * LtoP();

			if( registry.template Has<Camera>(handle) ) {
				auto camera = registry.template Get<Camera&>(handle);
				registry.Put(handle, ViewMatrix{glm::inverse(LtoW())});
				registry.Put(handle, ProjectionMatrix{camera.Matrix()});
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
		return false;
	}

    bool SceneManager::OnLoadObject(Message message) {
		auto msg = message.template GetData<MsgFileLoadObject>();
		auto tHandle = LoadTexture(Name{msg.m_txtName});
		auto oHandle = LoadOBJ(Name{msg.m_objName});

		auto nHandle = m_registry.Insert(
									Name(msg.m_objName),
									ParentHandle{GetHandle(Name{m_rootName})},
									Children{},
									Position{glm::vec3(-0.5f, 0.5f, 0.5f)}, 
									Orientation{}, 
									Scale{vec3_t{1.0f, 1.0f, 1.0f}}, 
									LocalToParentMatrix{mat4_t{1.0f}}, 
									LocalToWorldMatrix{mat4_t{1.0f}},
									GeometryHandle{oHandle}, 
									TextureHandle{tHandle} );

		m_registry.Get<Children&>(GetHandle(Name{m_rootName}))().push_back(nHandle);

		m_engine.SendMessage( MsgObjectCreate{this, nullptr, nHandle} );
		return false;
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

	bool SceneManager::OnKeyDown(Message message) {
		auto key = message.template GetData<MsgKeyDown>().m_key;

		if( key == SDL_SCANCODE_ESCAPE  ) {
			m_engine.Stop();
			return false;
		}

		switch( key )  {

			case SDL_SCANCODE_W : {
				auto camera = GetHandle(Name{m_cameraName});
				auto [p, o, s] = m_registry.Get<Position, Orientation, Scale>(camera);
				auto forward = glm::rotate(o(), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::vec3(0.0f, 1.0f, 0.0f);
				m_registry.Put(camera, Position{p() + forward});
				break;
			}

		}


        std::cout << "Key down: " << message.template GetData<MsgKeyDown>().m_key << std::endl;
		return false;
    }

    bool SceneManager::OnKeyRepeat(Message message) {
		auto key = message.template GetData<MsgKeyRepeat>().m_key;
		return OnKeyDown(MsgKeyDown{this, nullptr, message.GetDt(), key});

        //std::cout << "Key repeat: " << message.template GetData<MsgKeyRepeat>().m_key << std::endl;
		//return false;
    }


};  // namespace vve

namespace std {
	size_t hash<vve::Name>::operator()(vve::Name const& name) const {
		return std::hash<std::string>{}(name());
	}

}


