
#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {
	

	//-------------------------------------------------------------------------------------------------------

    AssetManager::AssetManager(std::string systemName, Engine& engine ) : System{systemName, engine } {
		engine.RegisterCallback( { 
			{this,                            2000, "INIT", [this](Message& message){ return OnInit(message);} },
			{this,                               0, "SCENE_LOAD", [this](Message& message){ return OnSceneLoad(message);} },
			{this, std::numeric_limits<int>::max(), "SCENE_LOAD", [this](Message& message){ return OnSceneLoad2(message);} },
			{this,                               0, "OBJECT_LOAD", [this](Message& message){ return OnObjectLoad(message);} },
			{this, std::numeric_limits<int>::max(), "TEXTURE_CREATE",   [this](Message& message){ return OnTextureCreate(message);} },
			{this,                               0, "QUIT", [this](Message& message){ return OnQuit(message);} },
		} );
	}

    AssetManager::~AssetManager() {}

    bool AssetManager::OnInit(Message message) {
		return false;
	}

    bool AssetManager::OnSceneLoad(Message& message) {
		auto& msg = message.template GetData<MsgSceneLoad>();
		auto path = msg.m_sceneName;
		std::filesystem::path filepath = path();
		auto directory = filepath.parent_path();
		
		msg.m_scene = aiImportFile(path().c_str(), aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_FlipUVs);

		//Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
		//Assimp::Importer importer;
		//msg.m_scene = importer.ReadFile(path().c_str(), aiProcess_Triangulate | aiProcess_GenNormals);
		if (!msg.m_scene || msg.m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !msg.m_scene->mRootNode) {
		    //std::cerr << "Assimp Error: " << importer.GetErrorString() << std::endl;
		    std::cerr << "Assimp Error: " << aiGetErrorString() << std::endl;
		}

		// Process materials

		for (unsigned int i = 0; i < msg.m_scene->mNumMaterials; i++) {
		    aiMaterial* material = msg.m_scene->mMaterials[i];

		    aiString name;
		    if (material->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
		        std::cout << "Material: " << name.C_Str() << std::endl;
		    }

		    aiColor3D color;
		    if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
		        std::cout << "Diffuse Color: " << color.r << ", " << color.g << ", " << color.b << std::endl;
		    }

		    aiString texturePath;
		    if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
				auto texturePathStr = directory / std::string{texturePath.C_Str()};
		        std::cout << "Diffuse Texture: " << texturePathStr.string() << std::endl;
				LoadTexture( Name{texturePathStr.string().c_str()});
		    }
		}

		// Process meshes
		for (unsigned int i = 0; i <msg.m_scene->mNumMeshes; i++) {
		    aiMesh* mesh = msg.m_scene->mMeshes[i];
			assert(mesh->HasPositions());

		    std::cout << "Mesh " << i << " " << mesh->mName.C_Str() << " has " << mesh->mNumVertices << " vertices." << std::endl;
			Name name{mesh->mName.C_Str()};
			if( m_handleMap.contains(name) ) continue;
			auto gHandle = m_registry.Insert( name );
			m_handleMap[name] = gHandle;

			vh::Geometry geometry{};
		    for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
		        aiVector3D vertex = mesh->mVertices[j];
				geometry.m_vertices.push_back(vh::Vertex{ {vertex.x, vertex.y, vertex.z} });
			
				if (mesh->HasNormals()) {
		            aiVector3D normal = mesh->mNormals[j];
					geometry.m_vertices[j].normal = vec3_t{normal.x, normal.y, normal.z};
				}

				if( mesh->HasTangentsAndBitangents() ) {
					aiVector3D tangent = mesh->mTangents[j];
					geometry.m_vertices[j].tangent = vec3_t{tangent.x, tangent.y, tangent.z};
				}

				if (mesh->HasTextureCoords(0)) { 
			        aiVector3D texCoord = mesh->mTextureCoords[0][j];
					geometry.m_vertices[j].texCoord = vec2_t{texCoord.x, texCoord.y};
				}

				if (mesh->HasVertexColors(0)) { 
				    aiColor4D color = mesh->mColors[0][j];
					geometry.m_vertices[j].color = vec4_t{color.r, color.g, color.b, color.a};
				}
		    }

			for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		        aiFace& face = mesh->mFaces[i];
        		// Ensure it's a triangle
        		if (face.mNumIndices == 3) {
            		geometry.m_indices.push_back(face.mIndices[0]);
            		geometry.m_indices.push_back(face.mIndices[1]);
            		geometry.m_indices.push_back(face.mIndices[2]);
        		}
			}
	
			m_registry.Put( gHandle, geometry );
		}
		return false;
	}

    bool AssetManager::OnSceneLoad2(Message& message) {
		auto msg = message.template GetData<MsgSceneLoad>();
		aiReleaseImport(msg.m_scene);
		return true;
	}

    bool AssetManager::OnObjectLoad(Message message) {
		auto msg = message.template GetData<MsgObjectLoad>();
		m_registry.Put(	msg.m_object, GeometryHandle{m_handleMap[msg.m_geomName]}, TextureHandle{m_handleMap[msg.m_txtName]} );
		return false;
	}

	bool AssetManager::OnTextureCreate(Message message) {
		auto msg = message.template GetData<MsgTextureCreate>();
		stbi_image_free(msg.m_pixels);
		return true;
	}

	bool AssetManager::OnQuit( Message message ) {
		return false;
	}

	auto AssetManager::LoadTexture(Name fileName) -> TextureHandle {
		if( m_handleMap.contains(fileName) ) return TextureHandle{m_handleMap[fileName]};

		int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(fileName().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        if (!pixels) { return {}; }

		auto handle = m_registry.Insert(fileName, vh::Texture{texWidth, texHeight, imageSize, pixels});
		m_handleMap[fileName] = handle;
		m_engine.SendMessage( MsgTextureCreate{this, nullptr, pixels, TextureHandle{handle} } );
		return TextureHandle{handle};
	}

	auto AssetManager::GetAsset(Name fileName) -> vecs::Handle {
		if( !m_handleMap.contains(fileName) ) return {};
		return m_handleMap[fileName]; 
	}

	auto AssetManager::GetAssetHandle(Name name) -> vecs::Handle& { 
		return m_handleMap[name]; 
	}


};  // namespace vve



