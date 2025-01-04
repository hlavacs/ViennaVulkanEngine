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
		m_handleMap[m_rootName] = m_registry.Insert(
												Name(m_rootName),
												Parent{},
												Children{},
												Position{glm::vec3(0.0f, 0.0f, 0.0f)},
												LocalToWorldMatrix{mat4_t{1.0f}},
												SceneNode{}); //insert root node

		// Create camera
		auto window = m_engine.GetWindow(m_windowName);
		//Camera camera{(float)window->GetWidth() / (float)window->GetHeight()};
        auto view = glm::inverse( glm::lookAt(glm::vec3(2.0f, 1.9f, 1.8f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)) );
		//auto quat = glm::quat_cast(view);
		//auto pos = glm::vec3(view[3]);
		//auto rot = glm::mat3_cast(quat);
		//Transform transform{{glm::vec3(view[3]), glm::quat_cast(view)}};
		auto cHandle = m_registry.Insert(
								std::string(m_cameraName), 
								Name(m_cameraName),
								Parent{m_handleMap[m_rootName]},
								Camera{(float)window->GetWidth() / (float)window->GetHeight()}, 
								Position{glm::vec3(view[3])}, 
								Orientation{glm::quat_cast(view)}, 
								Scale{vec3_t{1.0f, 1.0f, 1.0f}}, 
								LocalToParentMatrix{}, 
								LocalToWorldMatrix{}, 
								ViewMatrix{view},
								Transform{glm::vec3(view[3]), glm::quat_cast(view)}, 
								SceneNode{{glm::vec3(view[3]), glm::quat_cast(view)}, m_handleMap[m_rootName]} );

		m_registry.Get<Children&>(m_handleMap[m_rootName])().push_back(cHandle);

	}

    void SceneManager::OnUpdate(Message message) {
		auto node = m_registry.template Get<SceneNode&>(m_handleMap[m_rootName]);

		auto update = [](auto& registry, mat4_t& parentTransform, SceneNode& currentNode, auto& self) -> void {
			currentNode.m_localToWorldM = parentTransform * currentNode.m_localToParentT.Matrix();
			for( auto handle : currentNode.m_children ) {
				auto child = registry.template Get<SceneNode&>(handle);
				self(registry, currentNode.m_localToWorldM, child, self);
			}
		};

		for( auto handle : node.m_children ) {
			auto child = m_registry.template Get<SceneNode&>(handle);
			update(m_registry, node.m_localToWorldM, child, update);
		}

		for( auto [name, sn] : m_registry.template GetView<std::string, SceneNode&>() ) {
			std::cout << name << std::endl << sn.m_localToWorldM << std::endl;
		}

		for( auto [name, camera, sn] : m_registry.template GetView<std::string, Camera&, SceneNode&>() ) {
			sn.m_localToWorldM = glm::inverse(sn.m_localToWorldM);
			std::cout << name << std::endl << sn.m_localToWorldM << std::endl;
		}
	}

    void SceneManager::OnLoadObject(Message message) {
		auto msg = message.template GetData<MsgFileLoadObject>();
		auto tHandle = LoadTexture(msg.m_txtName);
		auto oHandle = LoadOBJ(msg.m_objName);

		auto nHandle = m_registry.Insert(
									Name(msg.m_objName),
									Parent{m_handleMap[m_rootName]},
									Children{},
									Position{glm::vec3()}, 
									Orientation{}, 
									Scale{vec3_t{1.0f, 1.0f, 1.0f}}, 
									LocalToParentMatrix{}, 
									LocalToWorldMatrix{},
									GeometryHandle{oHandle}, 
									TextureHandle{tHandle}, 
									SceneNode{} );

		m_registry.Get<Children&>(m_handleMap[m_rootName])().push_back(nHandle);

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

