#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {
	

	//-------------------------------------------------------------------------------------------------------

    AssetManager::AssetManager(std::string systemName, Engine& engine ) : System{systemName, engine } {
		engine.RegisterCallback( { 
			{this,  2000, "INIT", [this](Message& message){ return OnInit(message);} },
			{this,     0, "SCENE_LOAD", [this](Message& message){ return OnSceneLoad(message);} },
			{this,     0, "OBJECT_LOAD", [this](Message& message){ return OnObjectLoad(message);} },
			{this,     0, "QUIT", [this](Message& message){ return OnQuit(message);} },
		} );
	}

    AssetManager::~AssetManager() {}

    bool AssetManager::OnInit(Message message) {
		return false;
	}

    bool AssetManager::OnSceneLoad(Message& message) {
		auto& msg = message.template GetData<MsgSceneLoad>();
		auto path = msg.m_sceneName;

		msg.m_scene = aiImportFile(path().c_str(), aiProcess_Triangulate | aiProcess_GenNormals);

		//Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
		//Assimp::Importer importer;
		//msg.m_scene = importer.ReadFile(path().c_str(), aiProcess_Triangulate | aiProcess_GenNormals);
		if (!msg.m_scene || msg.m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !msg.m_scene->mRootNode) {
		    //std::cerr << "Assimp Error: " << importer.GetErrorString() << std::endl;
		    std::cerr << "Assimp Error: " << aiGetErrorString() << std::endl;
		}

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
		        std::cout << "Diffuse Texture: " << texturePath.C_Str() << std::endl;
				LoadTexture( Name{texturePath.C_Str()});
		    }
		}

		ProcessNode(msg.m_scene->mRootNode, msg.m_scene);




		return false;
	}

    bool AssetManager::OnObjectLoad(Message message) {
		auto msg = message.template GetData<MsgObjectLoad>();
		ObjectHandle nHandle = msg.m_object;
		TextureHandle tHandle = LoadTexture(msg.m_txtName);
		GeometryHandle gHandle = LoadOBJ(msg.m_geomName);
		m_registry.Put(	nHandle, gHandle, tHandle );
		return false;
	}

	bool AssetManager::OnQuit( Message message ) {
		return false;
	}

	void AssetManager::ProcessNode(aiNode* node, const aiScene* scene) {
			for (unsigned int i = 0; i <scene->mNumMeshes; i++) {
		    aiMesh* mesh = scene->mMeshes[i];
			vh::Geometry geometry{};

		    std::cout << "Mesh " << i << " " << mesh->mName.C_Str() << " has " << mesh->mNumVertices << " vertices." << std::endl;

		    for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
		        aiVector3D vertex = mesh->mVertices[j];
				geometry.m_vertices.push_back(vh::Vertex{ {vertex.x, vertex.y, vertex.z} });
			
				if (mesh->HasNormals()) {
		            aiVector3D normal = mesh->mNormals[j];
					geometry.m_vertices[j].normal = vec3_t{normal.x, normal.y, normal.z};
				}

				if (mesh->mTextureCoords[0]) { // Assumes the first set of texture coordinates
			        aiVector3D texCoord = mesh->mTextureCoords[0][j];
					geometry.m_vertices[j].texCoord = vec2_t{texCoord.x, texCoord.y};
				}

				if (mesh->mColors[0]) { // Assumes the first set of vertex colors
				    aiColor4D color = mesh->mColors[0][j];
					geometry.m_vertices[j].color = vec4_t{color.r, color.g, color.b, color.a};
				}
		    }
			Name name{mesh->mName.C_Str()};
			auto handle = m_registry.Insert( name, geometry);
			m_handleMap[name] = handle;
		}
	}

	auto AssetManager::LoadTexture(Name fileName) -> TextureHandle {
		if( m_handleMap.contains(fileName) ) return TextureHandle{m_handleMap[fileName]};

		int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(fileName().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        if (!pixels) { return {}; }

		vh::Texture texture{texWidth, texHeight, imageSize, pixels};
		auto handle = m_registry.Insert(fileName, texture);
		m_handleMap[fileName] = handle;
		return TextureHandle{handle};
	}

	auto AssetManager::LoadOBJ(Name fileName) -> GeometryHandle {
		if( m_handleMap.contains(fileName) ) return GeometryHandle{m_handleMap[fileName]};
		
		vh::Geometry geometry{};
		vh::loadModel(fileName(), geometry);
		auto handle = m_registry.Insert(fileName, geometry);
		m_handleMap[fileName] = handle;
		return GeometryHandle{handle};
	}
	

	auto AssetManager::GetAsset(Name fileName) -> vecs::Handle {
		if( !m_handleMap.contains(fileName) ) return {};
		return m_handleMap[fileName]; 
	}

	auto AssetManager::GetAssetHandle(Name name) -> vecs::Handle& { 
		return m_handleMap[name]; 
	}


};  // namespace vve




