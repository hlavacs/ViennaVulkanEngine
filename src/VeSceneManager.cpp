#include <filesystem>

#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {
	

	//-------------------------------------------------------------------------------------------------------

    SceneManager::SceneManager(std::string systemName, Engine& engine ) : System{systemName, engine } {
		engine.RegisterCallback( { 
			{this,  2000, "INIT", [this](Message& message){ return OnInit(message);} },
			{this, std::numeric_limits<int>::max(), "UPDATE", [this](Message& message){ return OnUpdate(message);} },
			{this,                            1000, "SCENE_LOAD", [this](Message& message){ return OnSceneLoad(message);} },
			{this, std::numeric_limits<int>::max(), "OBJECT_SET_PARENT", [this](Message& message){ return OnObjectSetParent(message);} },
			{this, std::numeric_limits<int>::max(), "SDL_KEY_DOWN", [this](Message& message){ return OnKeyDown(message);} },
			{this, std::numeric_limits<int>::max(), "SDL_KEY_REPEAT", [this](Message& message){ return OnKeyDown(message);} },
			{this, std::numeric_limits<int>::max(), "SDL_MOUSE_BUTTON_DOWN", [this](Message& message){ m_mouseButtonDown = true; m_x = m_y = -1; return false;} },
			{this, std::numeric_limits<int>::max(), "SDL_MOUSE_BUTTON_UP", [this](Message& message){ m_mouseButtonDown = false; return false;} },
			{this, std::numeric_limits<int>::max(), "SDL_MOUSE_MOVE", [this](Message& message){ return OnMouseMove(message); } }
		} );
	}

    SceneManager::~SceneManager() {}

    bool SceneManager::OnInit(Message message) {
		m_rootHandle = m_registry.Insert(
									Name{m_rootName},
									ParentHandle{},
									Children{},
									Position{glm::vec3(0.0f, 0.0f, 0.0f)},
									LocalToWorldMatrix{mat4_t{1.0f}} ); //insert root node

		// Create camera
		auto window = m_engine.GetWindow(m_windowName);
        auto view = glm::inverse( glm::lookAt(glm::vec3(4.0f, 1.9f, 3.8f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)) );

		m_cameraNodeHandle = m_registry.Insert(
								Name(m_cameraNodeName),
								ParentHandle{m_rootHandle},
								Children{},
								Position{glm::vec3(view[3])}, 
								Rotation{mat3_t{1.0f}},
								Scale{vec3_t{1.0f, 1.0f, 1.0f}}, 
								LocalToParentMatrix{mat4_t{1.0f}}, 
								LocalToWorldMatrix{mat4_t{1.0f}} );

		m_registry.template Get<Children&>(m_rootHandle)().push_back(m_cameraNodeHandle);

		m_cameraHandle = m_registry.Insert(
								Name(m_cameraName),
								ParentHandle{m_cameraNodeHandle},
								Camera{(real_t)window->GetWidth() / (real_t)window->GetHeight()}, 
								Position{vec3_t{0.0f, 0.0f, 0.0f}}, 
								Rotation{mat3_t{view}},
								Scale{vec3_t{1.0f, 1.0f, 1.0f}}, 
								LocalToParentMatrix{mat4_t{1.0f}}, 
								LocalToWorldMatrix{mat4_t{1.0f}}, 
								ViewMatrix{view} );

		m_registry.template Get<Children&>(m_cameraNodeHandle)().push_back(m_cameraHandle);
		return false;
	}

    bool SceneManager::OnUpdate(Message message) {
		auto children = m_registry.template Get<Children&>(m_rootHandle);

		auto update = [](auto& registry, mat4_t& parentToWorld, vecs::Handle& handle, auto& self) -> void {
			auto [name, p, r, s, LtoP, LtoW] = registry.template Get<Name&, Position&, Rotation&, Scale&, LocalToParentMatrix&, LocalToWorldMatrix&>(handle);
			LtoP = glm::translate(mat4_t{1.0f}, p()) * mat4_t(r()) * glm::scale(mat4_t{1.0f}, s());
			LtoW = parentToWorld * LtoP();
			//std::cout << "Handle: " << handle << " Name: " << name() << " Position: " << p().x << ", " << p().y << ", " << p().z << std::endl;

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

	bool SceneManager::OnSceneLoad(Message message) {
		auto& msg = message.template GetData<MsgSceneLoad>();
		ParentHandle pHandle = msg.m_parent;
		if( !pHandle().IsValid() ) pHandle = { m_rootHandle };

		std::filesystem::path filepath = msg.m_sceneName();
		auto directory = filepath.parent_path();

		static float x = 0.0f;
		uint64_t id = 0;
		ProcessNode(msg.m_scene->mRootNode, pHandle, directory, msg.m_scene, id, x);
		return false;
	}

	void SceneManager::ProcessNode(aiNode* node, ParentHandle parent, std::filesystem::path& directory, const aiScene* scene, uint64_t& id, float& x) {
		
		auto transform = node->mTransformation;
		aiVector3D scaling, position;
    	aiQuaternion rotation;
    	transform.Decompose(scaling, rotation, position);
		aiMatrix3x3 rotMat = rotation.GetMatrix();
		mat3_t rotMat3x3 = mat3_t(rotMat.a1, rotMat.a2, rotMat.a3, rotMat.b1, rotMat.b2, rotMat.b3, rotMat.c1, rotMat.c2, rotMat.c3);

		auto nHandle = m_registry.Insert(
								node->mName.C_Str()[0] != 0 ? Name{node->mName.C_Str()} : Name{"Node" + std::to_string(id++)},
								parent,
								Children{},
								Position{ { position.x, position.y + x, position.z } }, Rotation{rotMat3x3}, Scale{{ scaling.x, scaling.y, scaling.z }},
								LocalToParentMatrix{mat4_t{1.0f}}, 
								LocalToWorldMatrix{mat4_t{1.0f}});

		x += 2.0f;

		if(parent().IsValid()) {
			auto& children = m_registry.Get<Children&>(parent);
			children().push_back(nHandle);
		}

		for (unsigned int i = 0; i < std::min(1u, node->mNumMeshes); i++) {
		    auto mesh = scene->mMeshes[node->mMeshes[i]];
			auto material = scene->mMaterials[mesh->mMaterialIndex];
		    aiString texturePath;
			std::string texturePathStr{};
		    if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
				texturePathStr = (directory / std::string{texturePath.C_Str()}).string();
		        std::cout << "Diffuse Texture: " << texturePathStr << std::endl;
			} 

			m_engine.SendMessage( MsgObjectCreate{this, nullptr, ObjectHandle{nHandle}, 
				ParentHandle{parent}, Name{texturePathStr}, Name{mesh->mName.C_Str()} });
		}

		for (unsigned int i = 0; i < node->mNumChildren; i++) {
			float xx=0.0f;
			ProcessNode(node->mChildren[i], ParentHandle{nHandle}, directory, scene, id, xx);
		}
	}

    bool SceneManager::OnObjectSetParent(Message message) {
		auto msg = message.template GetData<MsgObjectSetParent>();
		auto oHandle = msg.m_object;
		auto pHandle = msg.m_parent;
		auto& parent = m_registry.template Get<ParentHandle&>(msg.m_object);
		auto& childrenOld = m_registry.template Get<Children&>(parent());
		auto& childrenNew = m_registry.template Get<Children&>(msg.m_parent);
		childrenOld().erase(std::remove(childrenOld().begin(), childrenOld().end(), oHandle), childrenOld().end());
		childrenNew().push_back(oHandle);
		parent = pHandle;
		return false;
	}

	bool SceneManager::OnKeyDown(Message message) {
		int key;
		real_t dt;
		if(message.HasType<MsgKeyDown>()) {
			auto msg = message.template GetData<MsgKeyDown>();
			key = msg.m_key;
			dt = (real_t)msg.m_dt;
		} else {
			auto msg = message.template GetData<MsgKeyRepeat>();
			key = msg.m_key;
			dt = (real_t)msg.m_dt;
		}

		if( key == SDL_SCANCODE_ESCAPE  ) { m_engine.Stop(); return false; }

		auto [pn, rn, sn] 		 = m_registry.template Get<Position&, Rotation&, Scale&>(m_cameraNodeHandle);
		auto [pc, rc, sc, LtoPc] = m_registry.template Get<Position&, Rotation&, Scale&, LocalToParentMatrix>(m_cameraHandle);		
	
		vec3_t translate = vec3_t(0.0f, 0.0f, 0.0f); //total translation
		vec3_t axis = vec3_t(1.0f); //total rotation around the axes, is 4d !
		float angle = 0.0f;
		float rotSpeed = 2.0f;

		switch( key )  {
			case SDL_SCANCODE_W : { translate = mat3_t{ LtoPc } * vec3_t(0.0f, 0.0f, -1.0f); break; }
			case SDL_SCANCODE_S : { translate = mat3_t{ LtoPc } * vec3_t(0.0f, 0.0f, 1.0f); break; }
			case SDL_SCANCODE_A : { translate = mat3_t{ LtoPc } * vec3_t(-1.0f, 0.0f, 0.0f); break; }
			case SDL_SCANCODE_Q : { translate = vec3_t(0.0f, 0.0f, -1.0f); break; }
			case SDL_SCANCODE_E : { translate = vec3_t(0.0f, 0.0f, 1.0f); break; }
			case SDL_SCANCODE_D : { translate = mat3_t{ LtoPc } * vec3_t(1.0f, 0.0f, 0.0f); break; }
			case SDL_SCANCODE_LEFT : { angle = rotSpeed * (float)dt * 1.0f; axis = glm::vec3(0.0, 0.0, 1.0); break; }
			case SDL_SCANCODE_RIGHT : { angle = rotSpeed * (float)dt * -1.0f; axis = glm::vec3(0.0, 0.0, 1.0); break; }
			case SDL_SCANCODE_UP : { angle = rotSpeed * (float)dt * -1.0f; 
									 axis = vec3_t{ LtoPc() * vec4_t{1.0f, 0.0f, 0.0f, 0.0f} }; break; }
			case SDL_SCANCODE_DOWN : { angle = rotSpeed * (float)dt * 1.0f;
									   axis = vec3_t{ LtoPc() * vec4_t{1.0f, 0.0f, 0.0f, 0.0f} }; break; }
		}

		float speed = 3.0f; ///add the new translation vector to the previous one
		pn = pn() + translate * (real_t)dt * speed;

		///combination of yaw and pitch, both wrt to parent space
		rc = mat3_t{ glm::rotate(mat4_t{1.0f}, angle, axis) * mat4_t{ rc } };
		return false;
    }


	bool SceneManager::OnMouseMove(Message message) {
		if( m_mouseButtonDown == false ) return false;

		auto msg = message.template GetData<MsgMouseMove>();
		real_t dt = (real_t)msg.m_dt;
		if( m_x==-1 ) { m_x = msg.m_x; m_y = msg.m_y; }
		int dx = msg.m_x - m_x;
		m_x = msg.m_x;
		int dy = msg.m_y - m_y;
		m_y = msg.m_y;

		auto [pn, rn, sn] = m_registry.template Get<Position&, Rotation&, Scale&>(m_cameraNodeHandle);
		auto [pc, rc, sc, LtoPc] = m_registry.template Get<Position&, Rotation&, Scale&, LocalToParentMatrix>(m_cameraHandle);		
		
		vec3_t translate = vec3_t(0.0f, 0.0f, 0.0f); //total translation
		vec3_t axis1 = vec3_t(1.0f); //total rotation around the axes, is 4d !
		float angle1 = 0.0f;
		vec3_t axis2 = vec3_t(1.0f); //total rotation around the axes, is 4d !
		float angle2 = 0.0f;
		float rotSpeed = 1.0f;

		//std::cout << "Mouse move: " << dx << ", " << dy << std::endl;

		angle1 = rotSpeed * (float)dt * -dx; //left right
		axis1 = glm::vec3(0.0, 0.0, 1.0);
		rc = mat3_t{ glm::rotate(mat4_t{1.0f}, angle1, axis1) * mat4_t{ rc } };

		angle2 = rotSpeed * (float)dt * -dy; //up down
		axis2 = vec3_t{ LtoPc() * vec4_t{1.0f, 0.0f, 0.0f, 0.0f} };
		rc = mat3_t{ glm::rotate(mat4_t{1.0f}, angle2, axis2) * mat4_t{ rc } };

		return false;
	}


};  // namespace vve




