
#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {
	

	//-------------------------------------------------------------------------------------------------------

    AssetManager::AssetManager(std::string systemName, Engine& engine ) : System{systemName, engine } {
		engine.RegisterCallback( { 
			{this,                               0, "SCENE_LOAD", [this](Message& message){ return OnSceneLoad(message);} },
			{this,                               0, "SCENE_CREATE", [this](Message& message){ return OnSceneCreate(message);} },
			{this, std::numeric_limits<int>::max(), "SCENE_CREATE", [this](Message& message){ return OnSceneCreate(message);} },
			{this,                               0, "OBJECT_CREATE", [this](Message& message){ return OnObjectCreate(message);} },
			{this, 								 0, "TEXTURE_CREATE", [this](Message& message){ return OnTextureCreate(message);} },
			{this, std::numeric_limits<int>::max(), "TEXTURE_CREATE", [this](Message& message){ return OnTextureRelease(message);} },
			{this, 								 0, "PLAY_SOUND", [this](Message& message){ return OnPlaySound(message);} },
		} );
	}

    AssetManager::~AssetManager() {}

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

    bool AssetManager::OnSceneLoad(Message& message) {
		auto& msg = message.template GetData<MsgSceneLoad>();
		auto flags = msg.m_ai_flags;
		const aiScene * scene = aiImportFile(msg.m_sceneName().c_str(),  aiProcessPreset_TargetRealtime_Fast | aiProcess_FlipUVs | flags); //need this for scenemanager to create nodes
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {std::cerr << "Assimp Error: " << aiGetErrorString() << std::endl;}
		else if( !m_fileNameMap.contains(msg.m_sceneName())) { SceneLoad(msg.m_sceneName, scene); }
		aiReleaseImport(scene);
		return true; //the message is consumed -> no more processing allowed
	}

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

		    aiColor3D color;
		    if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
		        std::cout << "Ambient Color: " << color.r << ", " << color.g << ", " << color.b << std::endl;
		    }
		    if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
		        std::cout << "Diffuse Color: " << color.r << ", " << color.g << ", " << color.b << std::endl;
		    }

		    aiString texturePath;
		    if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
				auto texturePathStr = directory.string() + "/" + std::string{texturePath.C_Str()};
		        std::cout << "Diffuse Texture: " << texturePathStr << std::endl;	
				
				auto tHandle = TextureHandle{m_registry.Insert(Name{texturePathStr})};
				auto pixels = LoadTexture(tHandle);
				if( pixels != nullptr) m_engine.SendMessage( MsgTextureCreate{tHandle, this } );
				m_fileNameMap.insert( std::make_pair(filepath, (Name{texturePathStr})) );
		    }
		}

		// Process meshes
		for (unsigned int i = 0; i <scene->mNumMeshes; i++) {
		    aiMesh* mesh = scene->mMeshes[i];
			assert(mesh->HasPositions() && mesh->HasNormals());

			Name name{ (filepath.string() + "/" + mesh->mName.C_Str())};
		    std::cout << "Mesh " << i << " " << name() << " has " << mesh->mNumVertices << " vertices." << std::endl;
			if( m_engine.ContainsHandle(name) ) continue;

			vh::Mesh VVEMesh{};
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
			m_engine.SendMessage( MsgMeshCreate{MeshHandle{gHandle}} );
		}
		return false;
	}

    bool AssetManager::OnObjectCreate(Message message) {
		auto msg = message.template GetData<MsgObjectCreate>();
		if( m_registry.Has<MeshName>(msg.m_object) ) {
			auto meshName = m_registry.Get<MeshName>(msg.m_object);
			m_registry.Put(	msg.m_object, MeshHandle{ m_engine.GetHandle(meshName) } );
		}
		if( m_registry.Has<TextureName>(msg.m_object) ) {
			auto textureName = m_registry.Get<TextureName>(msg.m_object);
			m_registry.Put(	msg.m_object, TextureHandle{m_engine.GetHandle(textureName)} );
		}
		return false;
	}

	bool AssetManager::OnTextureCreate(Message message) {
		auto msg = message.template GetData<MsgTextureCreate>();
		if( msg.m_sender == this ) return false;
		if( LoadTexture(TextureHandle{msg.m_handle}) != nullptr ) return true;
		return false;
	}

	bool AssetManager::OnTextureRelease(Message message) {
		auto msg = message.template GetData<MsgTextureCreate>();
		auto texture = m_registry.template Get<vh::Map&>(msg.m_handle);
		stbi_image_free(texture().m_pixels); //last thing release resources
		return true;
	}

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

	auto AssetManager::LoadTexture(TextureHandle tHandle) -> stbi_uc* {
		auto fileName = m_registry.Get<Name&>(tHandle);
		if( m_engine.ContainsHandle(fileName()) ) return nullptr;

		int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(fileName().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        if (!pixels) { return nullptr; }

		m_engine.SetHandle(fileName(), tHandle );
		m_registry.Put(tHandle, vh::Map{texWidth, texHeight, imageSize, pixels});
		return pixels;
	}


};  // namespace vve



