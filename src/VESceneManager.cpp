#include <filesystem>

#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {
	

	//-------------------------------------------------------------------------------------------------------
	//Scene Manager class
	
    SceneManager::SceneManager(std::string systemName, Engine& engine ) : System{systemName, engine } {
		engine.RegisterCallbacks( { 
			{this,  						  2000,	"INIT", [this](Message& message){ return OnInit(message);} },
			{this,      						 0, "LOAD_LEVEL", [this](Message& message){ return OnLoadLevel(message);} },
			{this,  							 0, "WINDOW_SIZE", [this](Message& message){ return OnWindowSize(message);} },
			{this, std::numeric_limits<int>::max(), "UPDATE", [this](Message& message){ return OnUpdate(message);} },
			{this,                            1000, "SCENE_CREATE", [this](Message& message){ return OnSceneCreate(message);} },
			{this,                               0, "OBJECT_CREATE", [this](Message& message){ return OnObjectCreate(message);} },
			{this, std::numeric_limits<int>::max(), "OBJECT_SET_PARENT", [this](Message& message){ return OnObjectSetParent(message);} },
			{this,                               0, "OBJECT_DESTROY", [this](Message& message){ return OnObjectDestroy(message);} },
			{this,                           20000, "OBJECT_DESTROY", [this](Message& message){ return OnObjectDestroy(message);} }
		} );
	}

    SceneManager::~SceneManager() {}

	bool SceneManager::OnInit(Message message) {
		m_worldHandle = m_registry.Insert( Name{m_worldName}, LocalToWorldMatrix{mat4_t{1.0f}} ); //no parent -> direct insert
								
		m_rootHandle = m_engine.CreateSceneNode(Name{m_rootName}, ParentHandle{m_worldHandle}); //insert root node
		m_engine.SetHandle(m_rootName, m_rootHandle); //save in handle map

		// Create camera node and camera
        auto view = glm::inverse( glm::lookAt(glm::vec3(0.0f, -2.0f, 3.5f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)) );
		m_cameraNodeHandle = m_engine.CreateSceneNode(Name{m_cameraNodeName}, ParentHandle{m_rootHandle}, Position{glm::vec3(view[3])}); //insert root node
		auto [handle, wstate] = Window::GetState(m_registry);
		m_cameraHandle = m_engine.CreateCamera(Name{m_cameraName}, ParentHandle{m_cameraNodeHandle},
										Camera{(real_t)(wstate().m_width) / (real_t)(wstate().m_height)},  
										Position{vec3_t{0.0f, 0.0f, 0.0f}}, Rotation{mat3_t{view}}); //insert root node

		return false;
	}

    bool SceneManager::OnLoadLevel(Message message) {

		vvh::Color color{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.9f, 0.1f, 0.1f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };
		m_engine.SendMsg( MsgSceneLoad{ vve::Filename{"assets/standard/sphere.obj"} });		
		
		float intensity1 = 0.8f;
		auto nodehandle = m_engine.CreateObject(Name{"LightSphere"}, ParentHandle{m_rootHandle}, 
												MeshName{"assets/standard/sphere.obj/sphere"}, color, 
												Position{glm::vec3(0.0f, 10.0f, 10.0f)}, 
												Rotation{mat3_t{1.0f}},
												Scale{vec3_t{0.01f, 0.01f, 0.01f}});

		auto lightHandle = m_engine.CreatePointLight( Name{"Light1"}, ParentHandle{nodehandle},
												PointLight{vvh::LightParams{
													glm::vec3(0.9f, 0.1f, 0.1f), 
													glm::vec4(0.0f, intensity1, 10.0, 0.1f), 
													glm::vec3(1.0f, 0.01f, 0.005f), 
												}});

		float intensity2 = 0.8f;
		auto lightHandle2 = m_engine.CreateDirectionalLight(
			Name{"Light2"}, ParentHandle{m_rootHandle},
			DirectionalLight{vvh::LightParams{
				glm::vec3(0.1f, 0.5f, 0.1f), glm::vec4(0.0f, intensity2, 10.0, 0.1f), glm::vec3(1.0f, 0.01f, 0.005f), 
			}},
			Position{glm::vec3(10.0f, 10.0f, 10.0f)},
			Rotation{mat3_t{glm::rotate(glm::mat4(1.0f), -3.14152f / 10.0f, glm::vec3(1.0f,0.0f,0.0f)) }}
		);

		vvh::Color color3{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.1f, 0.1f, 0.9f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };
		float intensity3 = 0.9f;

		auto lightHandle3 = m_engine.CreateObject(
			Name{"Light3"}, ParentHandle{m_rootHandle},
			vve::MeshName{"assets/standard/sphere.obj/sphere"}, color3,
			Position{glm::vec3(20.0f, -10.0f, 10.0f)},
			Rotation{mat3_t{glm::rotate(glm::mat4(1.0f), -3.14152f / 5.0f, glm::vec3(1.0f,0.0f,0.0f)) }},
			Scale{vec3_t{0.01f, 0.05f, 0.01f}}
		);

		m_registry.Put(lightHandle3, 
			SpotLight{vvh::LightParams{
				glm::vec3(0.5f, 0.5f, 0.9f), glm::vec4(0.0f, intensity3, 10.0, 0.1f), glm::vec3(1.0f, 0.01f, 0.005f), 
			}});

		return false;
	}

    bool SceneManager::OnWindowSize(Message message) {
		auto [name, camera] = m_registry.template Get<Name&, Camera&>(m_cameraHandle);
		auto [handle, wstate] = Window::GetState(m_registry);
		camera().m_aspect = (real_t)wstate().m_width / (real_t)wstate().m_height;
		return false;
	}


    bool SceneManager::OnUpdate(Message message) {
		auto children = m_registry.template Get<Children&>(m_rootHandle);

		auto update = [](auto& registry, mat4_t& parentToWorld, vecs::Handle& handle, auto& self) -> void {
			auto [name, p, r, s, LtoP, LtoW] = registry.template Get<Name, Position&, Rotation&, Scale&, LocalToParentMatrix&, LocalToWorldMatrix&>(handle);
			LtoP() = glm::translate(mat4_t{1.0f}, p()) * mat4_t(r()) * glm::scale(mat4_t{1.0f}, s());
			LtoW() = parentToWorld * LtoP();
			//std::cout << "Handle: " << handle << " Name: " << name() << " Position: " << p().x << ", " << p().y << ", " << p().z << std::endl;

			if( registry.template Has<Camera>(handle) ) {
				auto [name, camera] = registry.template Get<Name&, Camera&>(handle);
				auto cam = camera();
				registry.Put(handle, ViewMatrix{glm::inverse(LtoW())});
				registry.Put(handle, ProjectionMatrix{camera().Matrix()});
				//std::cout << "Camera: " << name() << " R: " << camera.m_aspect << std::endl;
			}

			if( registry.template Has<Children&>(handle) ) {
				auto children = registry.template Get<Children&>(handle);
				for( auto child : children() ) {
					self(registry, LtoW(), child, self);
				}
			}
		};

		for( auto child : children() ) {
			update(m_registry, LocalToWorldMatrix{mat4_t{1.0f}}, child, update);
		}
		return false;
	}

	bool SceneManager::OnObjectCreate(Message message) {
		auto& msg = message.template GetData<MsgObjectCreate>();
		if( msg.m_sender == this ) return false;
		ObjectHandle oHandle = msg.m_object;
		assert( oHandle().IsValid() );
		ParentHandle pHandle = msg.m_parent;
		if( !pHandle().IsValid() ) { pHandle = ParentHandle{ m_rootHandle }; }
		SetParent(oHandle, pHandle);
		return false;
	}

	bool SceneManager::OnSceneCreate(Message message) {
		auto& msg = message.template GetData<MsgSceneCreate>();
		ObjectHandle oHandle = msg.m_object;
		assert( oHandle().IsValid() );
		ParentHandle pHandle = msg.m_parent;
		if( !pHandle().IsValid() ) { pHandle = ParentHandle{ m_rootHandle }; }
		SetParent(oHandle, pHandle);

		auto exists = [&]<typename T>( vecs::Handle handle, T&& component ) {
			if( !m_registry.template Has<T>(handle) ) { 
				m_registry.Put(pHandle, std::forward<T>(component)); 
			}
		};

		exists(oHandle, Name{"Node0"});
		exists(oHandle, Position{vec3_t{0.0f}});
		exists(oHandle, Rotation{mat3_t{1.0f}});
		exists(oHandle, Scale{vec3_t{1.0f}});
		exists(oHandle, LocalToParentMatrix{mat4_t{1.0f}});
		exists(oHandle, LocalToWorldMatrix{mat4_t{0.0f}});

		std::filesystem::path filepath = msg.m_sceneName();
		uint64_t id = 1;
		ProcessNode(msg.m_scene->mRootNode, ParentHandle(oHandle), filepath, msg.m_scene, id);
		return false;
	}

	void SceneManager::ProcessNode(aiNode* node, ParentHandle parent, std::filesystem::path& filepath, const aiScene* scene, uint64_t& id) {
		auto directory = filepath.parent_path();

		auto transform = node->mTransformation;
		aiVector3D scaling, position;
    	aiQuaternion rotation;
    	transform.Decompose(scaling, rotation, position);
		aiMatrix3x3 rotMat = rotation.GetMatrix();
		mat3_t rotMat3x3 = mat3_t(rotMat.a1, rotMat.a2, rotMat.a3, rotMat.b1, rotMat.b2, rotMat.b3, rotMat.c1, rotMat.c2, rotMat.c3);

		std::cout << "\nCreate Node: " << node->mName.C_Str() << std::endl;

		auto nHandle = m_registry.Insert(
								node->mName.C_Str()[0] != 0 ? Name{node->mName.C_Str()} : Name{"Node" + std::to_string(id++)},
								Children{},
								Position{ { position.x, position.y, position.z } }, 
								Rotation{rotMat3x3}, 
								Scale{{ scaling.x, scaling.y, scaling.z }},
								LocalToParentMatrix{mat4_t{1.0f}}, 
								LocalToWorldMatrix{mat4_t{0.0f}});

		SetParent(ObjectHandle{nHandle}, parent);

		if ( node->mNumMeshes > 0) {
		    auto mesh = scene->mMeshes[node->mMeshes[0]];
			m_registry.Put(nHandle, MeshName{(filepath.string() + "/" + mesh->mName.C_Str())});
			
			auto material = scene->mMaterials[mesh->mMaterialIndex];
		    aiString texturePath;
			std::string texturePathStr{};
		    if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
				texturePathStr = (directory.string() + "/" + std::string{texturePath.C_Str()});
		        std::cout << "Diffuse Texture: " << texturePathStr << std::endl;
				m_registry.Put(nHandle, TextureName{texturePathStr});
			}

			vvh::Color color;
			bool hasColor = false;
			aiColor4D ambientColor;
			if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_AMBIENT, ambientColor)) {
				hasColor = true;
				color.m_ambientColor = to_vec4(ambientColor);
		        std::cout << "Ambient Color: " << ambientColor.r << ambientColor .g<< ambientColor.b << ambientColor.a << std::endl;
			}
			aiColor4D diffuseColor;
			if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor)) {
				hasColor = true;
				color.m_diffuseColor = to_vec4(diffuseColor);
		        std::cout << "Diffuse Color: " << diffuseColor.r << diffuseColor.g << diffuseColor.b << diffuseColor.a << std::endl;
			}
			if( hasColor ) {
				m_registry.Put(nHandle, color);
			}

			// Material: Metallic, Rougness
			vvh::Material mat;
			bool hasMat = false;
			aiColor4D metallicFactor;
			if (AI_SUCCESS == material->Get(AI_MATKEY_METALLIC_FACTOR, metallicFactor)) {
				hasMat = true;
				mat.m_material[0] = metallicFactor.r;
				std::cout << "Metallic Factor: " << metallicFactor.r << std::endl;
			}
			aiColor4D rougnessFactor;
			if (AI_SUCCESS == material->Get(AI_MATKEY_ROUGHNESS_FACTOR, rougnessFactor)) {
				hasMat = true;
				mat.m_material[1] = rougnessFactor.r;
				std::cout << "Roughness Factor: " << rougnessFactor.r << std::endl;
			}
			if (hasMat) {
				m_registry.Put(nHandle, mat);
			}

			m_engine.SendMsg( MsgObjectCreate{ObjectHandle{nHandle}, ParentHandle{parent}, this }); 
		}

		for (unsigned int i = 0; i < node->mNumChildren; i++) {
			ProcessNode(node->mChildren[i], ParentHandle{nHandle}, filepath, scene, id);
		}
	}

    bool SceneManager::OnObjectSetParent(Message message) {
		auto msg = message.template GetData<MsgObjectSetParent>();
		SetParent(msg.m_object, msg.m_parent);
		return false;
	}

	void SceneManager::SetParent(ObjectHandle oHandle, ParentHandle pHandle) {
		auto parent = m_registry.template Get<ParentHandle&>(oHandle);
		if( parent().IsValid() ) {
			auto childrenOld = m_registry.template Get<Children&>(parent());
			childrenOld().erase(std::remove(childrenOld().begin(), childrenOld().end(), oHandle), childrenOld().end());
		} else {
			//pHandle = ParentHandle{m_rootHandle};
		}

		auto childrenNew = m_registry.template Get<Children&>(pHandle);
		childrenNew().push_back(oHandle);
		parent() = pHandle;
	}

    bool SceneManager::OnObjectDestroy(Message message) {
		auto msg = message.template GetData<MsgObjectDestroy>();
		if( msg.m_phase > 0) { //last phase -> Uniform Buffers have been deallocated
			m_registry.Erase(msg.m_handle);
			return false;
		}

		auto parent = m_registry.template Get<ParentHandle&>(msg.m_handle);
		if( parent().IsValid() ) {
			auto children = m_registry.template Get<Children&>(parent());
			children().erase(std::remove(children().begin(), children().end(), msg.m_handle), children().end());
		}

		auto children = m_registry.template Get<Children>(msg.m_handle);
		for( auto child : children() ) {
			m_engine.SendMsg(MsgObjectDestroy(ObjectHandle(child)));
		}
		return false;
	}


};  // namespace vve




