
#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {


	//-------------------------------------------------------------------------------------------------------

	/**
	 * @brief Constructor for the AssetManager class
	 * @param systemName Name of the system
	 * @param engine Reference to the engine
	 */
    AssetManager::AssetManager(std::string systemName, Engine& engine ) : System{systemName, engine } {
		engine.RegisterCallbacks( { 
			{this,                               0, "SCENE_LOAD", [this](Message& message){ return OnSceneLoad(message);} },
			{this,                               0, "SCENE_CREATE", [this](Message& message){ return OnSceneCreate(message);} },
			{this, std::numeric_limits<int>::max(), "SCENE_CREATE", [this](Message& message){ return OnSceneCreate(message);} },
			{this,                               0, "OBJECT_CREATE", [this](Message& message){ return OnObjectCreate(message);} },
			{this, 								 0, "TEXTURE_CREATE", [this](Message& message){ return OnTextureCreate(message);} },
			{this, std::numeric_limits<int>::max(), "TEXTURE_CREATE", [this](Message& message){ return OnTextureRelease(message);} },
			{this, 								 0, "PLAY_SOUND", [this](Message& message){ return OnPlaySound(message);} },
		} );
	}

	/**
	 * @brief Destructor for the AssetManager class
	 */
    AssetManager::~AssetManager() {}

	/**
	 * @brief Handle scene creation message
	 * @param message Message containing scene creation data
	 * @return True if message was handled
	 */
	bool AssetManager::OnSceneCreate( Message& message ) {
		auto& msg = message.template GetData<MsgSceneCreate>();
		auto flags = msg.m_ai_flags;
		auto phase = message.GetPhase(); //called in 2 phases, 0 and std::numeric_limits<int>::max()

		if( phase < std::numeric_limits<int>::max() ) { //first phase?  
			msg.m_scene = aiImportFile(msg.m_sceneName().c_str(),  aiProcessPreset_TargetRealtime_Fast | aiProcess_FlipUVs | flags); //need this for scenemanager to create nodes
			if (!msg.m_scene || msg.m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !msg.m_scene->mRootNode) {std::cerr << "Assimp Error: " << aiGetErrorString() << std::endl;}
			else if( !m_fileNameMap.contains(msg.m_sceneName())) { SceneLoad(msg.m_sceneName, msg.m_scene); }
			return false;
		}
		aiReleaseImport(msg.m_scene); //last phase -> release the scene
		return true;
	}

	/**
	 * @brief Handle scene load message
	 * @param message Message containing scene load data
	 * @return True if message was handled
	 */
    bool AssetManager::OnSceneLoad(Message& message) {
		auto& msg = message.template GetData<MsgSceneLoad>();
		auto flags = msg.m_ai_flags;
		const aiScene * scene = aiImportFile(msg.m_sceneName().c_str(),  aiProcessPreset_TargetRealtime_Fast | aiProcess_FlipUVs | flags); //need this for scenemanager to create nodes
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {std::cerr << "Assimp Error: " << aiGetErrorString() << std::endl;}
		else if( !m_fileNameMap.contains(msg.m_sceneName())) { SceneLoad(msg.m_sceneName, scene); }
		aiReleaseImport(scene);
		return true; //the message is consumed -> no more processing allowed
	}

	/**
	 * @brief Load a scene from file
	 * @param sceneName Filename of the scene
	 * @param scene Pointer to the Assimp scene structure
	 * @return True if scene was loaded successfully
	 */
	bool AssetManager::SceneLoad(Filename sceneName, const C_STRUCT aiScene* scene) {
		std::filesystem::path filepath = sceneName();
		auto directory = filepath.parent_path();

		// Process materials
		for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
		    aiMaterial* material = scene->mMaterials[i];

		    aiString name;
		    if (material->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
		        std::cout << "Material: " << name.C_Str() << std::endl;
		    }

			Name nameMat{ (filepath.string() + "/" + std::string(name.C_Str()) + "/Material")};
			if (m_engine.ContainsHandle(nameMat)) continue;

			vvh::VRTMaterial VRTmaterial;

		    aiColor3D color;
		    if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
		        std::cout << "Ambient Color: " << color.r << ", " << color.g << ", " << color.b << std::endl;
		    }
		    if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
		        std::cout << "Diffuse Color: " << color.r << ", " << color.g << ", " << color.b << std::endl;
				VRTmaterial.albedo = glm::vec3(color.r, color.g, color.b);
		    }
			if (material->Get(AI_MATKEY_METALLIC_FACTOR, color) == AI_SUCCESS) {
				std::cout << "Metallic Factor: " << color.r << ", ";
				VRTmaterial.metallic = color.r;
			}
			if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, color) == AI_SUCCESS) {
				std::cout << "Roughness Factor: " << color.r << std::endl;
				VRTmaterial.roughness = color.r;
			}
			if (material->Get(AI_MATKEY_SPECULAR_FACTOR, color) == AI_SUCCESS) {
				std::cout << "Specular Factor: " << color.r << std::endl;
				VRTmaterial.ior = color.r;
			}
			if (material->Get(AI_MATKEY_OPACITY, color) == AI_SUCCESS) {
				std::cout << "Opacity Factor: " << color.r << std::endl;
				VRTmaterial.alpha = color.r;
			}

		    aiString texturePath;
		    if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
				auto texturePathStr = directory.string() + "/" + std::string{texturePath.C_Str()};
		        std::cout << "Diffuse Texture: " << texturePathStr << std::endl;	
				
				auto tHandle = TextureHandle{m_registry.Insert(Name{texturePathStr})};
				auto pixels = LoadTexture(tHandle, vvh::ColorSpace::SRGB);
				if( pixels != nullptr) m_engine.SendMsg( MsgTextureCreate{tHandle, this } );
				m_fileNameMap.insert( std::make_pair(filepath, (Name{texturePathStr})) );

				VRTmaterial.albedoTextureName = texturePathStr;
		    }

			//added aditional textures:

			if (material->GetTexture(aiTextureType_NORMALS, 0, &texturePath) == AI_SUCCESS) {
				auto texturePathStr = directory.string() + "/" + std::string{ texturePath.C_Str() };
				std::cout << "Normal Texture: " << texturePathStr << std::endl;

				auto tHandle = TextureHandle{ m_registry.Insert(Name{texturePathStr}) };
				auto pixels = LoadTexture(tHandle, vvh::ColorSpace::LINEAR);
				if (pixels != nullptr) m_engine.SendMsg(MsgTextureCreate{ tHandle, this });
				m_fileNameMap.insert(std::make_pair(filepath, (Name{ texturePathStr })));

				VRTmaterial.normalTextureName = texturePathStr;
			}

			if (material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &texturePath) == AI_SUCCESS) {
				auto texturePathStr = directory.string() + "/" + std::string{ texturePath.C_Str() };
				std::cout << "Roughness Texture: " << texturePathStr << std::endl;

				auto tHandle = TextureHandle{ m_registry.Insert(Name{texturePathStr}) };
				auto pixels = LoadTexture(tHandle, vvh::ColorSpace::LINEAR);
				if (pixels != nullptr) m_engine.SendMsg(MsgTextureCreate{ tHandle, this });
				m_fileNameMap.insert(std::make_pair(filepath, (Name{ texturePathStr })));

				VRTmaterial.roughnessTextureName = texturePathStr;
			}

			if (material->GetTexture(aiTextureType_METALNESS, 0, &texturePath) == AI_SUCCESS) {
				auto texturePathStr = directory.string() + "/" + std::string{ texturePath.C_Str() };
				std::cout << "Metalness Texture: " << texturePathStr << std::endl;

				auto tHandle = TextureHandle{ m_registry.Insert(Name{texturePathStr}) };
				auto pixels = LoadTexture(tHandle, vvh::ColorSpace::LINEAR);
				if (pixels != nullptr) m_engine.SendMsg(MsgTextureCreate{ tHandle, this });
				m_fileNameMap.insert(std::make_pair(filepath, (Name{ texturePathStr })));

				VRTmaterial.metallicTextureName = texturePathStr;
			}

			if (material->GetTexture(aiTextureType_SPECULAR, 0, &texturePath) == AI_SUCCESS) {
				auto texturePathStr = directory.string() + "/" + std::string{ texturePath.C_Str() };
				std::cout << "IOR Texture: " << texturePathStr << std::endl;

				auto tHandle = TextureHandle{ m_registry.Insert(Name{texturePathStr}) };
				auto pixels = LoadTexture(tHandle, vvh::ColorSpace::LINEAR);
				if (pixels != nullptr) m_engine.SendMsg(MsgTextureCreate{ tHandle, this });
				m_fileNameMap.insert(std::make_pair(filepath, (Name{ texturePathStr })));

				VRTmaterial.iorTextureName = texturePathStr;
			}

			if (material->GetTexture(aiTextureType_OPACITY, 0, &texturePath) == AI_SUCCESS) {
				auto texturePathStr = directory.string() + "/" + std::string{ texturePath.C_Str() };
				std::cout << "Alpha Texture: " << texturePathStr << std::endl;

				auto tHandle = TextureHandle{ m_registry.Insert(Name{texturePathStr}) };
				auto pixels = LoadTexture(tHandle, vvh::ColorSpace::LINEAR);
				if (pixels != nullptr) m_engine.SendMsg(MsgTextureCreate{ tHandle, this });
				m_fileNameMap.insert(std::make_pair(filepath, (Name{ texturePathStr })));

				VRTmaterial.alphaTextureName = texturePathStr;
			}
			
			/*
			m_engine.SetHandle(fileName(), tHandle);
			m_registry.Put(tHandle, vvh::Image{ texWidth, texHeight, 1, imageSize, pixels });
			*/

			auto mHandle = m_registry.Insert(nameMat);
			m_engine.SetHandle(nameMat, mHandle);
			m_registry.Put(mHandle, VRTmaterial);

			m_engine.SendMsg(MsgMaterialCreate{ MaterialHandle{mHandle} });

			std::cout << "MaterialName: " << (filepath.string() + "/" + std::string(name.C_Str()) + "/Material") << "\n";
		}

		// Process meshes
		for (unsigned int i = 0; i <scene->mNumMeshes; i++) {
		    aiMesh* mesh = scene->mMeshes[i];
			assert(mesh->HasPositions() && mesh->HasNormals());

			Name name{ (filepath.string() + "/" + mesh->mName.C_Str())};
		    std::cout << "Mesh " << i << " " << name() << " has " << mesh->mNumVertices << " vertices." << std::endl;
			if( m_engine.ContainsHandle(name) ) continue;

			vvh::Mesh VVEMesh{};
		    for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
		        aiVector3D vertex = mesh->mVertices[j];
				VVEMesh.m_verticesData.m_positions.push_back({vertex.x, vertex.y, vertex.z});
			
				if (mesh->HasNormals()) {
		            aiVector3D normal = mesh->mNormals[j];
					VVEMesh.m_verticesData.m_normals.push_back({normal.x, normal.y, normal.z});
				}

				if( mesh->HasTangentsAndBitangents() ) {
					aiVector3D tangent = mesh->mTangents[j];
					VVEMesh.m_verticesData.m_tangents.push_back({tangent.x, tangent.y, tangent.z});
				}

				if (mesh->HasTextureCoords(0)) { 
			        aiVector3D texCoord = mesh->mTextureCoords[0][j];
					VVEMesh.m_verticesData.m_texCoords.push_back({texCoord.x, texCoord.y});
				}

				if (mesh->HasVertexColors(0)) { 
				    aiColor4D color = mesh->mColors[0][j];
					VVEMesh.m_verticesData.m_colors.push_back({color.r, color.g, color.b, color.a});
				}
		    }

			for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		        aiFace& face = mesh->mFaces[i];
        		// Ensure it's a triangle
        		if (face.mNumIndices == 3) {
            		VVEMesh.m_indices.push_back(face.mIndices[0]);
            		VVEMesh.m_indices.push_back(face.mIndices[1]);
            		VVEMesh.m_indices.push_back(face.mIndices[2]);
        		}
			}

			auto gHandle = m_registry.Insert( name, VVEMesh );
			m_engine.SetHandle(name, gHandle);
			m_fileNameMap.insert( std::make_pair(filepath, name) );
			m_engine.SendMsg( MsgMeshCreate{MeshHandle{gHandle}} );
		}
		return false;
	}

	/**
	 * @brief Handle object creation message
	 * @param message Message containing object creation data
	 * @return True if message was handled
	 */
    bool AssetManager::OnObjectCreate(Message message) {
		auto msg = message.template GetData<MsgObjectCreate>();
		if( m_registry.Has<MeshName>(msg.m_object) ) {
			auto meshName = m_registry.Get<MeshName>(msg.m_object);
			MeshHandle gHandle = MeshHandle{ m_engine.GetHandle(meshName) };
			m_registry.Put(	msg.m_object, gHandle);
		}
		if( m_registry.Has<TextureName>(msg.m_object) ) {
			auto textureName = m_registry.Get<TextureName>(msg.m_object);
			m_registry.Put(	msg.m_object, TextureHandle{m_engine.GetHandle(textureName)} );
		}
		if (m_registry.Has<MaterialName>(msg.m_object)) {
			auto materialName = m_registry.Get<MaterialName>(msg.m_object);
			m_registry.Put(msg.m_object, MaterialHandle{ m_engine.GetHandle(materialName) });
		}
		return false;
	}

	/**
	 * @brief Handle texture creation message
	 * @param message Message containing texture creation data
	 * @return True if message was handled
	 */
	bool AssetManager::OnTextureCreate(Message message) {
		auto msg = message.template GetData<MsgTextureCreate>();
		if( msg.m_sender == this ) return false;
		if( LoadTexture(TextureHandle{msg.m_handle}) != nullptr ) return true;
		return false;
	}

	/**
	 * @brief Handle texture release message
	 * @param message Message containing texture release data
	 * @return True if message was handled
	 */
	bool AssetManager::OnTextureRelease(Message message) {
		auto msg = message.template GetData<MsgTextureCreate>();
		auto texture = m_registry.template Get<vvh::Image&>(msg.m_handle);
		stbi_image_free(texture().m_pixels); //last thing release resources
		return true;
	}

	/**
	 * @brief Handle play sound message
	 * @param message Message containing sound playback data
	 * @return True if message was handled
	 */
	bool AssetManager::OnPlaySound(Message& message) {
		auto& msg = message.template GetData<MsgPlaySound>();
		auto fileName = msg.m_filepath();
		if( m_engine.ContainsHandle(fileName) ) {
			msg.m_soundHandle = m_engine.GetHandle(fileName);
			return false;
		}

		SoundState state{};
		state.m_filepath = fileName;
		msg.m_soundHandle = m_registry.Insert(state);
		m_engine.SetHandle(fileName, msg.m_soundHandle);
		return false;
	}

	/**
	 * @brief Load texture data from file
	 * @param tHandle Handle to the texture to load
	 * @return Pointer to loaded texture data
	 */
	auto AssetManager::LoadTexture(TextureHandle tHandle) -> stbi_uc* {
		auto fileName = m_registry.Get<Name&>(tHandle);
		if( m_engine.ContainsHandle(fileName()) ) return nullptr;

		int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(fileName().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        if (!pixels) { return nullptr; }

		m_engine.SetHandle(fileName(), tHandle );
		m_registry.Put(tHandle, vvh::Image{texWidth, texHeight, 1, imageSize, pixels, vvh::ColorSpace::SRGB});
		return pixels;
	}

	auto AssetManager::LoadTexture(TextureHandle tHandle, vvh::ColorSpace colorSpace) -> stbi_uc* {
		auto fileName = m_registry.Get<Name&>(tHandle);
		if (m_engine.ContainsHandle(fileName())) return nullptr;

		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(fileName().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		VkDeviceSize imageSize = texWidth * texHeight * 4;
		if (!pixels) { return nullptr; }

		m_engine.SetHandle(fileName(), tHandle);
		m_registry.Put(tHandle, vvh::Image{ texWidth, texHeight, 1, imageSize, pixels, colorSpace });
		return pixels;
	}


};  // namespace vve



