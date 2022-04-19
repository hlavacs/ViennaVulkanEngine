/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VEInclude.h"

#define STANDARD_MESH_CUBE "media/models/standard/cube.obj/cube"
#define STANDARD_MESH_INVCUBE "media/models/standard/invcube.obj/cube"
#define STANDARD_MESH_PLANE "media/models/standard/plane.obj/plane"
#define STANDARD_MESH_SPHERE "media/models/standard/sphere.obj/sphere"

namespace ve
{
	VESceneManager *g_pVESceneManagerSingleton = nullptr; ///<Singleton pointer to the only VESceneManager instance

	VESceneManager::VESceneManager()
	{
		g_pVESceneManagerSingleton = this;
	}

	/**
		*
		* \brief Initializes the scene manager.
		*
		* In this function the scene manager loads standard shapes like cubes and planes.
		* Then it creates a standard camera system (camera + parent) and a standard light.
		*
		*/
	void VESceneManager::initSceneManager()
	{
		std::vector<vh::vhMemoryBlock *> emptyList;

		m_memoryBlockMap[VESceneObject::VE_OBJECT_TYPE_ENTITY] = emptyList;

		VECHECKRESULT(vh::vhMemBlockListInit(getEnginePointer()->getRenderer()->getDevice(),
			getEnginePointer()->getRenderer()->getVmaAllocator(),
			getEnginePointer()->getRenderer()->getDescriptorPool(),
			getEnginePointer()->getRenderer()->getDescriptorSetLayoutPerObject(),
			2048, sizeof(VEEntity::veUBOPerEntity_t),
			getEnginePointer()->getRenderer()->getSwapChainNumber(),
			m_memoryBlockMap[VESceneObject::VE_OBJECT_TYPE_ENTITY]));

		m_memoryBlockMap[VESceneObject::VE_OBJECT_TYPE_CAMERA] = emptyList;
		VECHECKRESULT(vh::vhMemBlockListInit(getEnginePointer()->getRenderer()->getDevice(),
			getEnginePointer()->getRenderer()->getVmaAllocator(),
			getEnginePointer()->getRenderer()->getDescriptorPool(),
			getEnginePointer()->getRenderer()->getDescriptorSetLayoutPerObject(),
			128, sizeof(VECamera::veUBOPerCamera_t),
			getEnginePointer()->getRenderer()->getSwapChainNumber(),
			m_memoryBlockMap[VESceneObject::VE_OBJECT_TYPE_CAMERA]));

		m_memoryBlockMap[VESceneObject::VE_OBJECT_TYPE_LIGHT] = emptyList;
		VECHECKRESULT(vh::vhMemBlockListInit(getEnginePointer()->getRenderer()->getDevice(),
			getEnginePointer()->getRenderer()->getVmaAllocator(),
			getEnginePointer()->getRenderer()->getDescriptorPool(),
			getEnginePointer()->getRenderer()->getDescriptorSetLayoutPerObject(),
			16, sizeof(VELight::veUBOPerLight_t),
			getEnginePointer()->getRenderer()->getSwapChainNumber(),
			m_memoryBlockMap[VESceneObject::VE_OBJECT_TYPE_LIGHT]));

		std::vector<VEMesh *> meshes;
		std::vector<VEMaterial *> materials;

		loadAssets("media/models/standard", "cube.obj", 0, meshes, materials);
		loadAssets("media/models/standard", "invcube.obj", aiProcess_FlipWindingOrder, meshes, materials);
		loadAssets("media/models/standard", "plane.obj", 0, meshes, materials);
		loadAssets("media/models/standard", "sphere.obj", 0, meshes, materials);

		m_rootSceneNode = new VESceneNode("RootSceneNode");
	};

	//-----------------------------------------------------------------------------------------------------------------------
	//load stuff using Assimp

	/**
		*
		* \brief Load assets from file ussing Assimp
		*
		* The scene manager loads assets from a file and creates the contained meshes and materials.
		* It does not create entities. Meshes and materials are stored in the scene manager's member variables.
		*
		* \param[in] basedir Name of directory the file is in
		* \param[in] filename Name of the file containing the assets
		* \param[in] aiFlags Import flags for Assimp, see code below for some examples
		* \param[out] meshes A list containing pointers to the loaded meshes
		* \param[out] materials A list of pointers to the loaded materials
		*
		*/
	const aiScene *VESceneManager::loadAssets(std::string basedir, std::string filename, uint32_t aiFlags, std::vector<VEMesh *> &meshes, std::vector<VEMaterial *> &materials)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		Assimp::Importer importer;

		std::string filekey = basedir + "/" + filename;

		const aiScene *pScene = importer.ReadFile(filekey,
			//aiProcess_FlipWindingOrder |
			//aiProcess_RemoveRedundantMaterials |
			//aiProcess_PreTransformVertices |
			aiProcess_GenNormals |
			aiProcess_CalcTangentSpace |
			aiProcess_Triangulate |
			//aiProcess_JoinIdenticalVertices |
			//aiProcess_FixInfacingNormals |
			aiFlags);

		VECHECKPOINTER((void *)pScene);

		createMeshes(pScene, filekey, meshes); //create new meshes if any
		createMaterials(pScene, basedir, filekey, materials); //create new materials if any

		return pScene;
	}

	/**
		*
		* \brief Load assets from file ussing Assimp, create entities from them
		*
		* The scene manager loads assets from a file and creates the contained meshes and materials.
		* Meshes and materials are stored in the scene manager's member variables. It then followsa the entity
		* tree recursively and creates the contained entities.
		*
		* \param[in] entityName The name of the new entity (its the parent of all created entities)
		* \param[in] basedir Name of directory the file is in
		* \param[in] filename Name of the file containing the assets
		* \param[in] aiFlags Import flags for Assimp, see code below for some examples
		* \param[in] parent Make the new entity a child of this parent entity
		*
		*/
	VESceneNode *VESceneManager::loadModel(std::string entityName,
		std::string basedir,
		std::string filename,
		uint32_t aiFlags,
		VESceneNode *parent)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_sceneNodes.count(entityName) > 0)
			return m_sceneNodes[entityName]; //if an entity with this name exists return it

		Assimp::Importer importer;

		std::string filekey = basedir + "/" + filename;

		const aiScene *pScene = importer.ReadFile(filekey,
			//aiProcess_FlipWindingOrder |
			//aiProcess_RemoveRedundantMaterials |
			//aiProcess_PreTransformVertices |
			aiProcess_GenNormals |
			aiProcess_CalcTangentSpace |
			aiProcess_Triangulate |
			//aiProcess_JoinIdenticalVertices |
			//aiProcess_FixInfacingNormals |
			aiFlags);

		if (!pScene)
		{
			std::string error = importer.GetErrorString();
		}
		VECHECKPOINTER((void *)pScene);

		VESceneNode *pMO = createSceneNode2(entityName,
			parent); //create a new scene node as parent of the whole scene

		std::vector<VEMesh *> meshes;
		createMeshes(pScene, filekey, meshes); //create the new meshes if any
		std::vector<VEMaterial *> materials;
		createMaterials(pScene, basedir, filekey, materials); //create the new materials if any

		copyAiNodes(pScene, meshes, materials, pScene->mRootNode,
			pMO); //create scene nodes and entities from the file

		sceneGraphChanged2(); //notify renderer to rerecord the cmd buffers
		return pMO;
	}

	/**
		*
		* \brief Follow the Assimp tree of nodes and create entities from them.
		*
		* Assimp returns a tree of nodes, each node having one or more meshes. Since an VEEntity
		* can have only one mesh, for each of the meshes one VEEntity is created and being made the child
		* of the current parent.
		*
		* \param[in] pScene A pointer to the Assimp scene
		* \param[in] meshes The meshes that were loaded by Assimp from the file
		* \param[in] materials The materials that were loaded by Assimp from the file
		* \param[in] node The Assimp node currently being processed
		* \param[in] parent The parent entity of the new entity
		*
		*/
	void VESceneManager::copyAiNodes(const aiScene *pScene,
		std::vector<VEMesh *> &meshes,
		std::vector<VEMaterial *> &materials,
		aiNode *node,
		VESceneNode *parent)
	{
		VESceneNode *pObject = createSceneNode2(parent->getName() + "/" + node->mName.C_Str(), parent);

		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{ //go through the meshes of the Assimp node

			VEMesh *pMesh = nullptr;
			VEMaterial *pMaterial = nullptr;

			uint32_t paiMeshIdx = node->mMeshes[i]; //get mesh index in global mesh list
			pMesh = meshes[paiMeshIdx]; //use index to get pointer to VEMesh
			aiMesh *paiMesh = pScene->mMeshes[paiMeshIdx]; //also get handle to the Assimp mesh

			uint32_t paiMatIdx = paiMesh->mMaterialIndex; //get the material index for this mesh
			pMaterial = materials[paiMatIdx]; //use the index to get the right VEMaterial

			glm::mat4 *pMatrix = (glm::mat4 *)&node->mTransformation;

			VEEntity *pEnt = createEntity2(pObject->getName() + "/Entity_" + std::to_string(i), //create the new entity
				VEEntity::VE_ENTITY_TYPE_NORMAL,
				pMesh, pMaterial, pObject, *pMatrix);
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
		{ //recursivly go down the node tree
			copyAiNodes(pScene, meshes, materials, node->mChildren[i], pObject);
		}
	}

	/**
		*
		* \brief Create all VEMesh instances from a file loaded by Assimp
		*
		* Once Assimp loaded a file it offers a global list of meshes. The function just
		* goes through this list and creates VEMesh instances, then stores pointers to the in the meshes list.
		*
		* \param[in] pScene Pointer to the Assimp scene.
		* \param[in] filekey Unique string identifying this file. Can be used for the mesh names.
		* \param[out] meshes List of new meshes.
		*
		*/
	void VESceneManager::createMeshes(const aiScene *pScene, std::string filekey, std::vector<VEMesh *> &meshes)
	{
		VEMesh *pMesh = nullptr;

		for (uint32_t i = 0; i < pScene->mNumMeshes; i++)
		{
			const aiMesh *paiMesh = pScene->mMeshes[i];
			std::string name = filekey + "/" + paiMesh->mName.C_Str();

			VEMesh *pMesh = nullptr;
			if (m_meshes.count(name) == 0)
			{
				pMesh = new VEMesh(name, paiMesh);
				m_meshes[name] = pMesh;
			}
			else
			{
				pMesh = m_meshes[name];
			}
			meshes.push_back(pMesh);
		}
	}

	/**
		*
		* \brief Create all VEMaterial instances from a file loaded by Assimp
		*
		* Once Assimp loaded a file it offers a global list of materials. The function just
		* goes through this list and creates VEMaterial instances, then stores pointers to the in the materials list.
		*
		* \param[in] pScene Pointer to the Assimp scene.
		* \param[in] basedir Name of the directory the file is in (for loading textures)
		* \param[in] filekey Unique string identifying this file. Can be used for the mesh names.
		* \param[out] materials List of new materials.
		*
		*/
	void VESceneManager::createMaterials(const aiScene *pScene, std::string basedir, std::string filekey, std::vector<VEMaterial *> &materials)
	{
		for (uint32_t i = 0; i < pScene->mNumMaterials; i++)
		{
			aiMaterial *paiMat = pScene->mMaterials[i];
			aiString matname("");
			paiMat->Get(AI_MATKEY_NAME, matname);

			std::string name = filekey + "/" + matname.C_Str();
			VEMaterial *pMat = nullptr;
			if (m_materials.count(name) == 0)
			{
				pMat = createMaterial2(name);

				int mode;
				paiMat->Get(AI_MATKEY_SHADING_MODEL, mode);
				pMat->shading = (aiShadingMode)mode;

				aiColor3D color(0.f, 0.f, 0.f);
				if (paiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
				{
					pMat->color.r = color.r;
					pMat->color.g = color.g;
					pMat->color.b = color.b;
					pMat->color.a = 1.0f;
				}

				/*for (uint32_t i = 0; i < paiMat->mNumProperties; i++) {
						aiMaterialProperty *paimatprop = paiMat->mProperties[i];
						aiString key = paimatprop->mKey;
						std::string skey(key.C_Str());
						uint32_t j = 0;
					}*/

				for (uint32_t i = 0; i < paiMat->GetTextureCount(aiTextureType_AMBIENT); i++)
				{
					aiString str;
					paiMat->GetTexture(aiTextureType_AMBIENT, i, &str);
				}
				for (uint32_t i = 0; i < paiMat->GetTextureCount(aiTextureType_DIFFUSE); i++)
				{
					aiString str;
					paiMat->GetTexture(aiTextureType_DIFFUSE, i, &str);

					std::string name(str.C_Str());
					if (name[0] == '*')
						continue;
					pMat->mapDiffuse = createTexture2(filekey + "/" + name, basedir, name);
				}
				for (uint32_t i = 0; i < paiMat->GetTextureCount(aiTextureType_SPECULAR); i++)
				{
					aiString str;
					paiMat->GetTexture(aiTextureType_SPECULAR, i, &str);
				}
				for (uint32_t i = 0; i < paiMat->GetTextureCount(aiTextureType_LIGHTMAP); i++)
				{
					aiString str;
					paiMat->GetTexture(aiTextureType_LIGHTMAP, i, &str);
				}
				for (uint32_t i = 0; i < paiMat->GetTextureCount(aiTextureType_NORMALS); i++)
				{
					aiString str;
					paiMat->GetTexture(aiTextureType_NORMALS, i, &str);

					std::string name(str.C_Str());
					if (name[0] == '*')
						continue;
					if (pMat->mapNormal == nullptr)
						pMat->mapNormal = createTexture2(filekey + "/" + name, basedir, name);
				}
				for (uint32_t i = 0; i < paiMat->GetTextureCount(aiTextureType_DISPLACEMENT); i++)
				{
					aiString str;
					paiMat->GetTexture(aiTextureType_DISPLACEMENT, i, &str);

					std::string name(str.C_Str());
					if (name[0] == '*')
						continue;
					if (pMat->mapBump == nullptr)
						pMat->mapBump = createTexture2(filekey + "/" + name, basedir, name);
				}
				for (uint32_t i = 0; i < paiMat->GetTextureCount(aiTextureType_HEIGHT); i++)
				{
					aiString str;
					paiMat->GetTexture(aiTextureType_HEIGHT, i, &str);

					std::string name(str.C_Str());
					if (name[0] == '*')
						continue;
					if (pMat->mapHeight == nullptr)
						pMat->mapHeight = createTexture2(filekey + "/" + name, basedir, name);
				}
			}
			else
			{
				pMat = m_materials[name];
			}

			materials.push_back(pMat);
		}
	}

	//-----------------------------------------------------------------------------------------
	//create complex scene nodes and entities

	/**
		* \brief Create a scene node
		*
		* Calls its own shadow function, so that it does not block itself when called by another public function
		*
		* \param[in] objectName The name of the new MO.
		* \param[in] transf Local to parent transform.
		* \param[in] parent Pointer to entity to be used as parent.
		* \returns a pointer to the new scene node
		*
		*/
	VESceneNode *VESceneManager::createSceneNode(std::string objectName, VESceneNode *parent, glm::mat4 transf)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return createSceneNode2(objectName, parent, transf);
	}

	/**
		* \brief Create a scene node
		*
		* This is a protected shadow funtion of the public API function. It can be called by other public
		* functions that lock the mutex.
		*
		* \param[in] objectName The name of the new MO.
		* \param[in] transf Local to parent transform.
		* \param[in] parent Pointer to entity to be used as parent.
		* \returns a pointer to the new scene node
		*
		*/
	VESceneNode *VESceneManager::createSceneNode2(std::string objectName,
		VESceneNode *parent,
		glm::mat4 transf)
	{
		if (m_sceneNodes.count(objectName) > 0)
			return m_sceneNodes[objectName];

		VESceneNode *pMO = new VESceneNode(objectName, transf);
		addSceneNodeAndChildren2(pMO, parent);
		sceneGraphChanged2();
		return pMO;
	}

	/**
		* \brief Create an entity
		*
		* \param[in] entityName The name of the new entity.
		* \param[in] pMesh Pointer the mesh for this entity.
		* \param[in] pMat Pointer to the material for this entity.
		* \param[in] transf Local to parent transform, given as GLM matrix.
		* \param[in] parent Pointer to entity to be used as parent.
		* \returns a pointer to the new entity
		*
		*/
	VEEntity *VESceneManager::createEntity(std::string entityName, VEMesh *pMesh, VEMaterial *pMat, VESceneNode *parent, glm::mat4 transf)
	{
		return createEntity(entityName, VEEntity::VE_ENTITY_TYPE_NORMAL, pMesh, pMat, parent, transf);
	}

	/**
		* \brief Create an entity
		*
		* \param[in] entityName The name of the new entity.
		* \param[in] type The entity type to be used.
		* \param[in] pMesh Pointer the mesh for this entity.
		* \param[in] pMat Pointer to the material for this entity.
		* \param[in] transf Local to parent transform, given as GLM matrix.
		* \param[in] parent Pointer to entity to be used as parent.
		* \returns a pointer to the new entity
		*
		*/
	VEEntity *VESceneManager::createEntity(std::string entityName, VEEntity::veEntityType type, VEMesh *pMesh, VEMaterial *pMat, VESceneNode *parent, glm::mat4 transf)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return createEntity2(entityName, type, pMesh, pMat, parent, transf);
	}

	/**
		* \brief Create an entity
		*
		* \param[in] entityName The name of the new entity.
		* \param[in] type The entity type to be used.
		* \param[in] pMesh Pointer the mesh for this entity.
		* \param[in] pMat Pointer to the material for this entity.
		* \param[in] transf Local to parent transform, given as GLM matrix.
		* \param[in] parent Pointer to entity to be used as parent.
		* \returns a pointer to the new entity
		*
		*/
	VEEntity *VESceneManager::createEntity2(std::string entityName, VEEntity::veEntityType type, VEMesh *pMesh, VEMaterial *pMat, VESceneNode *parent, glm::mat4 transf)
	{
		VEEntity *pEntity = new VEEntity(entityName, type, pMesh, pMat, transf);
		addSceneNodeAndChildren2(pEntity, parent); // store entity in the entity array

		if (pMesh != nullptr && pMat != nullptr)
		{
			getEnginePointer()->getRenderer()->addEntityToSubrenderer(pEntity);
		}
		sceneGraphChanged2();
		return pEntity;
	}

	/**
		* \brief Create a camera
		*
		* \param[in] cameraName The name of the new camera.
		* \param[in] type The camera type to be used.
		* \param[in] parent Pointer to entity to be used as parent.
		* \param[in] transf Local to parent transform, given as GLM matrix.
		* \returns a pointer to the new camera
		*
		*/
	VECamera *VESceneManager::createCamera(std::string cameraName, VECamera::veCameraType type, VESceneNode *parent, glm::mat4 transf)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		VECamera *pCam = nullptr;
		if (type == VECamera::VE_CAMERA_TYPE_PROJECTIVE)
		{
			pCam = new VECameraProjective(cameraName, transf);
		}
		else
		{
			pCam = new VECameraOrtho(cameraName, transf);
		}

		addSceneNodeAndChildren2(pCam, parent);
		return pCam;
	}

	/**
		* \brief Create a light
		*
		* \param[in] lightName The name of the new camera.
		* \param[in] type The light type to be used.
		* \param[in] parent Pointer to scene node to be used as parent.
		* \param[in] transf Local to parent transform, given as GLM matrix.
		* \returns a pointer to the new light
		*
		*/
	VELight *VESceneManager::createLight(std::string lightName, VELight::veLightType type, VESceneNode *parent, glm::mat4 transf)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		VELight *pLight = nullptr;
		if (type == VELight::VE_LIGHT_TYPE_DIRECTIONAL)
		{
			pLight = new VEDirectionalLight(lightName, transf);
		}
		else if (type == VELight::VE_LIGHT_TYPE_POINT)
		{
			pLight = new VEPointLight(lightName, transf);
		}
		else if (type == VELight::VE_LIGHT_TYPE_SPOT)
		{
			pLight = new VESpotLight(lightName, transf);
		}
		else
		{
			pLight = new VEAmbientLight(lightName);
		}

		addSceneNodeAndChildren2(pLight, parent);
		return pLight;
	}

	//-----------------------------------------------------------------------------------------
	//create complex entities

	/**
		*
		* \brief Create a plane that is projected to the far plane of the frustum
		*
		* Public API that locks the mutex and then calls its won shadow function
		*
		* \param[in] entityName Name of the new entity.
		* \param[in] basedir Name of the directory the texture file is in
		* \param[in] texName name of a texture file that contains the sky texture
		* \param[in] parent Pointer to the parent of this skyplane
		* \returns a pointer to the new entity
		*
		*/
	VEEntity *VESceneManager::createSkyplane(std::string entityName, std::string basedir, std::string texName, VESceneNode *parent)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return createSkyplane2(entityName, basedir, texName, parent);
	}

	/**
		*
		* \brief Create a plane that is projected to the far plane of the frustum
		*
		* Protected shadow function that creates  the plane.
		*
		* \param[in] entityName Name of the new entity.
		* \param[in] basedir Name of the directory the texture file is in
		* \param[in] texName name of a texture file that contains the sky texture
		* \param[in] parent Pointer to the parent of this skyplane
		* \returns a pointer to the new entity
		*
		*/
	VEEntity *VESceneManager::createSkyplane2(std::string entityName, std::string basedir, std::string texName, VESceneNode *parent)
	{
		std::string filekey = basedir + "/" + texName;
		VEMesh *pMesh;
		VECHECKPOINTER(pMesh = m_meshes[STANDARD_MESH_PLANE]);

		VEMaterial *pMat = nullptr;
		if (m_materials.count(filekey) == 0)
		{
			pMat = createMaterial2(filekey);
			pMat->mapDiffuse = createTexture2(filekey, basedir, texName);
		}
		else
		{
			pMat = m_materials[filekey];
		}

		VEEntity *pEntity = createEntity2(entityName, VEEntity::VE_ENTITY_TYPE_SKYPLANE, pMesh, pMat, parent);
		pEntity->m_castsShadow = false;

		sceneGraphChanged2();
		return pEntity;
	}

	/**
		*
		* \brief Create a skyplane based sky box
		*
		* This function loads 5 textures to use them as sky planes. The bottom plane is not loaded
		* The order of the tex names must be ft bk up dn rt lf
		*
		* \param[in] entityName Name of the new entity.
		* \param[in] basedir Name of the directory the texture file is in
		* \param[in] texNames List of 6 names of the texture files. Order must be ft bk up dn rt lf
		* \param[in] parent Pointer to a scene node that is the parent of this box, should be root node
		* \returns a pointer to the new entity, which is the parent of the planes
		*
		*/
	VESceneNode *VESceneManager::createSkybox(std::string entityName, std::string basedir, std::vector<std::string> texNames, VESceneNode *parent)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		std::string filekey = basedir + "/";
		std::string addstring = "";
		for (auto filename : texNames)
		{
			filekey += addstring + filename;
			addstring = "+";
		}

		VESceneNode *skybox = createSceneNode2(entityName, parent);
		float scale = 1000.0f;

		VEEntity *sp1 = getSceneManagerPointer()->createSkyplane2(filekey + "/Skyplane1", basedir, texNames[0], skybox);
		sp1->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(-scale, 1.0f, -scale)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), -(float)M_PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)));
		sp1->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, scale / 2.0f)));
		sp1->m_castsShadow = false;

		sp1 = getSceneManagerPointer()->createSkyplane2(filekey + "/Skyplane2", basedir, texNames[1], skybox);
		sp1->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(scale, 1.0f, scale)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), (float)M_PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)));
		sp1->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -scale / 2.0f)));
		sp1->m_castsShadow = false;

		sp1 = getSceneManagerPointer()->createSkyplane2(filekey + "/Skyplane3", basedir, texNames[2], skybox);
		sp1->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(scale, 1.0f, scale)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), (float)M_PI / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), (float)M_PI, glm::vec3(1.0f, 0.0f, 0.0f)));
		sp1->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, scale / 2.0f, 0.0f)));
		sp1->m_castsShadow = false;

		sp1 = getSceneManagerPointer()->createSkyplane2(filekey + "/Skyplane4", basedir, texNames[4], skybox);
		sp1->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(-scale, 1.0f, -scale)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), (float)M_PI / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), (float)M_PI / 2.0f, glm::vec3(0.0f, 0.0f, 01.0f)));
		sp1->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(scale / 2.0f, 0.0f, 0.0f)));
		sp1->m_castsShadow = false;

		sp1 = getSceneManagerPointer()->createSkyplane2(filekey + "/Skyplane5", basedir, texNames[5], skybox);
		sp1->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(scale, 1.0f, scale)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), (float)M_PI / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), -(float)M_PI / 2.0f, glm::vec3(0.0f, 0.0f, 1.0f)));
		sp1->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-scale / 2.0f, 0.0f, 0.0f)));
		sp1->m_castsShadow = false;

		sceneGraphChanged2();
		return skybox;
	}

	//----------------------------------------------------------------------------------------------------------------
	//scene management stuff

	/**
		* \brief This should be called whenever the scene graph ist changed - public API
		*/
	void VESceneManager::sceneGraphChanged()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		sceneGraphChanged3(); //if no auto record then app has to trigger rerecording itself
	}

	/**
		* \brief This should be called whenever the scene graph ist changed - internal version called automatically after changes
		*/
	void VESceneManager::sceneGraphChanged2()
	{
		if (m_autoRecord)
		{
			sceneGraphChanged3(); //if no auto record then app has to trigger rerecording itself
		}
	}

	/**
		* \brief This should be called whenever the scene graph ist changed
		*/
	void VESceneManager::sceneGraphChanged3()
	{
		for (auto pNode : m_deletedSceneNodes)
		{
			deleteSceneNodeAndChildren2(pNode);
		}
		m_deletedSceneNodes.clear();

		getEnginePointer()->getRenderer()->updateCmdBuffers();
	}

	/**
		* \brief Set the visibility of a whole subtree
		*/
	void VESceneManager::setVisibility(VESceneNode *pNode, bool flag)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		setVisibility2(pNode, flag);
	}

	/**
		* \brief Set the visibility of a whole subtree
		*/
	void VESceneManager::setVisibility2(VESceneNode *pNode, bool flag)
	{
		//std::cout << "Deleting node " << pNode->getName() << "\n";

		if (pNode->getNodeType() == VESceneNode::VE_NODE_TYPE_SCENEOBJECT &&
			((VESceneObject *)pNode)->getObjectType() == VESceneObject::VE_OBJECT_TYPE_ENTITY)
		{
			((VEEntity *)pNode)->m_visible = flag;
		}

		std::vector<VESceneNode *> children = pNode->getChildrenCopy();
		for (auto pChild : children)
		{
			setVisibility2(pChild, flag);
		}
	}

	/**
		*
		* \brief Find all scene nodes without a parent, then update them and their children
		*
		* Makes this nodes and their children to copy their data to the GPU
		*
		* \param[in] imageIndex Index of the swapchain image that is currently used.
		*
		*/
	void VESceneManager::updateSceneNodes(uint32_t imageIndex)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		m_lights.clear(); //light vector will be created dynamically

		updateSceneNodes2(getRoot(), glm::mat4(1.0f), imageIndex

		);

		while (m_updateFutures.size() > 0)
		{ //gets all futures from the threads and waits for them
			m_updateFutures.front().get();
			m_updateFutures.pop();
		}

		for (
			auto list : m_memoryBlockMap)
		{ //update all UBO buffers, i.e. copy them to the GPU
			vh::vhMemBlockUpdateBlockList(list.second, imageIndex);
		}
	}

	/**
	*
	* \brief Update this node and all its children
	*
	* Makes this nodes and their children to copy their data to the GPU
	*
	* \param[in] pNode Pointer to the node to start updating
	* \param[in] parentWorldMatrix World Matrix of the parent, used as a start
	* \param[in] imageIndex Index of the swapchain image that is currently used.
	*
	*/
	void VESceneManager::updateSceneNodes2(VESceneNode *pNode, glm::mat4 parentWorldMatrix, uint32_t imageIndex)
	{
		glm::mat4 worldMatrix = parentWorldMatrix * pNode->getTransform(); //compute the world matrix

		pNode->updateUBO(worldMatrix, imageIndex); //copy UBO data to the GPU

		if (pNode->getNodeType() == VESceneNode::VE_NODE_TYPE_SCENEOBJECT &&
			((VESceneObject *)pNode)->getObjectType() == VESceneObject::VE_OBJECT_TYPE_LIGHT)
		{
			m_lights.push_back((VELight *)pNode); //put a light into the light vector
		}

		if (pNode->m_children.size() > 0)
		{
			ThreadPool *tp = getEnginePointer()->getThreadPool();

			const uint32_t granularity = 200;
			if (tp->threadCount() > 1 && pNode->m_children.size() > granularity)
			{
				uint32_t numThreads = std::min((int)(pNode->m_children.size() / granularity), (int)tp->threadCount());
				uint32_t numChildrenPerThread = (uint32_t)pNode->m_children.size() / numThreads;

				uint32_t startIdx, endIdx;
				for (uint32_t k = 0; k < numThreads; k++)
				{
					startIdx = k * numChildrenPerThread; //start index for parallel run
					endIdx = k == numThreads - 1 ? (uint32_t)pNode->m_children.size() - 1 : (k + 1) * numChildrenPerThread - 1; //end index

					auto future = tp->add(&VESceneManager::updateSceneNodes3, this, pNode->m_children, worldMatrix,
						startIdx, endIdx, imageIndex); //add to threadpool
					{
						static std::mutex mutex;
						std::lock_guard<std::mutex> lock(mutex);
						m_updateFutures.push(
							std::move(future)); //add to list of futures, so main thread can wait for completion
					}
				}
			}
			else
			{
				updateSceneNodes3(pNode->m_children, worldMatrix, 0, (uint32_t)pNode->m_children.size() - 1,
					imageIndex); //do sequential update
			}
		}
	}

	/**
	*
	* \brief Update a list of nodes and all their children
	*
	* This function can be called in parallel on subparts of a children list
	*
	* \param[in] children Reference to a list of children
	* \param[in] worldMatrix Parent world matrix for the children
	* \param[in] startIdx Start index pointing to the child to start at in the children list
	* \param[in] endIdx End index pointing to the child to end with in the children list
	* \param[in] imageIndex Index of the swapchain image that is currently used.
	*
	*/
	void VESceneManager::updateSceneNodes3(std::vector<VESceneNode *> &children, glm::mat4 worldMatrix, uint32_t startIdx, uint32_t endIdx, uint32_t imageIndex)
	{
		for (uint32_t i = startIdx; i <= endIdx; i++)
		{
			updateSceneNodes2(children[i], worldMatrix, imageIndex);
		}
	}

	/**
	*
	* \brief Add a new scene node into the scene
	*
	* If the parent is the nullptr then make the root scene node its parent.
	* This is the protected shadow function.
	*
	* \param[in] pNode Pointer to the new scene node
	* \param[in] parent Pointer to the new parent of this node
	*
	*/
	void VESceneManager::addSceneNodeAndChildren2(VESceneNode *pNode, VESceneNode *parent)
	{
		if (pNode->getNodeType() == VESceneNode::VE_NODE_TYPE_SCENEOBJECT)
		{
			VESceneObject *pObject = (VESceneObject *)pNode;
			if (pObject->m_memoryHandle.owner == nullptr)
			{
				vh::vhMemBlockListAdd(m_memoryBlockMap[pObject->getObjectType()], pObject,
					&pObject->m_memoryHandle); //reserve a UBO
			}
		}

		if (parent != nullptr)
		{ //attach to the parent
			parent->addChild(pNode);
		}
		m_sceneNodes[pNode->getName()] = pNode; //store in scene node list

		for (auto pChild : pNode->getChildrenList())
		{ //do the same for all children
			addSceneNodeAndChildren2(pChild, pNode);
		}
	}

	/**
	*
	* \brief Find an entity using its name
	*
	* \param[in] name Name of the entity.
	* \returns a pointer to the entity
	*
	*/
	VESceneNode *VESceneManager::getSceneNode(std::string name)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_sceneNodes.count(name) == 0)
			return nullptr;
		return m_sceneNodes[name];
	}

	/**
	*
	* \brief Delete a scene node and all its subentities
	*
	* \param[in] name Name of the scene node.
	*
	*/
	void VESceneManager::deleteSceneNodeAndChildren(std::string name)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_sceneNodes.count(name) == 0)
			return;
		VESceneNode *pNode = m_sceneNodes[name]; //pointer to the node

		if (pNode->hasParent())
			pNode->getParent()->removeChild(pNode); //if it has a parent, remove from the children list
		setVisibility2(pNode, false); //make it and all children invisible

		std::vector<std::string> namelist; //first create a list of all child names
		createSceneNodeList2(pNode, namelist);

		for (auto nodename : namelist)
		{
			VESceneNode *pdelNode = m_sceneNodes[nodename];
			m_deletedSceneNodes.push_back(pdelNode); //push it to the deleted scene nodes list
		}

		sceneGraphChanged2(); //make scene graph update
	}

	/**
	*
	* \brief Notify event listeners that a scene node has been deleted
	*
	* Event listeners aswering true will also be deleted.
	*
	* \param[in] pNode Pointer to scene node that has been deleted
	*
	*/
	void VESceneManager::notifyEventListeners(VESceneNode *pNode)
	{
		//notify frame listeners that this node has been deleted
		veEvent event(veEvent::VE_EVENT_SUBSYSTEM_GENERIC, veEvent::VE_EVENT_DELETE_NODE);
		event.ptr = pNode;

		std::vector<std::string> nameList;
		std::vector<VEEventListener *> *listeners =
			getEnginePointer()->m_eventListeners[veEvent::VE_EVENT_DELETE_NODE]; //list of event listeners interested in this event

		for (auto listener : *listeners)
		{ //go through them and call onSceneNodeDeleted()
			if (listener->onSceneNodeDeleted(
				event))
			{ //if the listener answers with true, it wants to be destroyed
				nameList.push_back(listener->getName()); //so save its name on a list
			}
		}
		for (auto name : nameList)
		{ //delete the listeners that want to die
			getEnginePointer()->deleteEventListener(name);
		}
	}

	/**
	*
	* \brief Delete a scene node and all its subentities
	*
	* \param[in] pNode Pointer to the scene node to be deleted
	*
	*/
	void VESceneManager::deleteSceneNodeAndChildren2(VESceneNode *pNode)
	{
		//if (pNode->hasParent()) pNode->getParent()->removeChild(pNode);

		//std::vector<std::string> namelist;	//first create a list of all child names
		//createSceneNodeList2(pNode, namelist);

		//go through the list and delete all children
		//for (uint32_t i = 0; i < namelist.size(); i++) {
		//	pNode = m_sceneNodes[namelist[i]];

		if (pNode->getNodeType() == VESceneNode::VE_NODE_TYPE_SCENEOBJECT)
		{
			VESceneObject *pObject = (VESceneObject *)pNode;

			if (pObject->getObjectType() == VESceneObject::VE_OBJECT_TYPE_CAMERA &&
				m_camera == (VECamera *)pObject) //is it the current camera?
				m_camera = nullptr;

			if (pObject->getObjectType() ==
				VESceneObject::VE_OBJECT_TYPE_ENTITY) //if its a object that is rendered
				getEnginePointer()->getRenderer()->removeEntityFromSubrenderers(
					(VEEntity *)pObject); //remove it from its subrenderer

			if (pObject->m_memoryHandle.pMemBlock !=
				nullptr)
			{ //remove it from the UBO list
				vh::vhMemBlockRemoveEntry(&pObject->m_memoryHandle);
			}
		}

		notifyEventListeners(pNode); //notify all event listeners that this node will soon be deleted

		m_sceneNodes.erase(pNode->getName()); //remove it from the scene node list
		delete pNode; //delete the scene node
		//}
	}

	/**
	*
	* \brief Create a list of all child entities of a given entity, then delete them
	*
	* Function will delete all children of the root scene node, bit not the root itself
	*
	*/
	void VESceneManager::deleteScene()
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		getEnginePointer()->clearEventListenerList(); //delete all event listeners, so we do not have to iterate through them

		std::vector<VESceneNode *> children = m_rootSceneNode->getChildrenCopy();
		for (auto pChild : children)
		{ //move children of root to the deleted nodes list
			m_rootSceneNode->removeChild(pChild);
			m_deletedSceneNodes.push_back(pChild);
		}
		sceneGraphChanged3();
	}

	/**
	*
	* \brief Create a list of all child entities of a given entity
	*
	* Public API function locks the mutex, then calls its shadow function
	*
	* \param[in] pObject Pointer to the root of the tree.
	* \param[out] namelist List of names of children of the entity.
	*
	*/
	void VESceneManager::createSceneNodeList(VESceneNode *pObject, std::vector<std::string> &namelist)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		createSceneNodeList2(pObject, namelist);
	}

	/**
	*
	* \brief Create a list of all child entities of a given entity
	*
	* \param[in] pObject Pointer to the root of the tree.
	* \param[out] namelist List of names of children of the entity.
	*
	*/
	void VESceneManager::createSceneNodeList2(VESceneNode *pObject, std::vector<std::string> &namelist)
	{
		namelist.push_back(pObject->getName());

		for (uint32_t i = 0; i < pObject->getChildrenList().size(); i++)
		{
			createSceneNodeList2(pObject->getChildrenList()[i], namelist); //recursive call
		}
	}

	/**
	* \brief Create a new mesh with a certain name
	*
	* \param[in] name The name of mesh
	* \param[in] vertices Vertices of the mesh
	* \param[in] indices Indices of the mesh
	* \returns a new mesh or an old one if it exists with this name
	*
	*/
	VEMesh *
		VESceneManager::createMesh(std::string name, std::vector<vh::vhVertex> &vertices, std::vector<uint32_t> &indices)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_meshes.count(name) > 0)
			return m_meshes[name]; //if mesh already exists, resturn it

		VEMesh *pMesh = new VEMesh(name, vertices, indices); //create the mesh
		m_meshes[name] = pMesh; //store in mesh map
		return pMesh;
	}

	/**
	* \brief Find a mesh by its name and return a pointer to it
	*
	* \param[in] name The name of mesh
	* \returns a mesh given its name
	*
	*/
	VEMesh *VESceneManager::getMesh(std::string name)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_meshes.count(name) == 0)
			return nullptr;
		return m_meshes[name];
	};

	/**
	*
	* \brief Delete a mesh given its name.
	*
	* This function does NOT check whether the mesh is still used by some entity!
	* It should be only called if this is certain, e.g. if the whole scene is deleted.
	*
	* \param[in] name Name of the mesh.
	*
	*/
	void VESceneManager::deleteMesh(std::string name)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_meshes.count(name) > 0)
		{
			VEMesh *pMesh = m_meshes[name]; //get pointer to the mesh
			m_meshes.erase(name); //remove it from the mesh list
			delete pMesh; //delete the mesh
		}
	}

	/**
	*
	* \brief Create a new texture with a given name
	*
	* \param[in] name Name of the texture.
	* \param[in] basedir Name of the directory the file is in
	* \param[in] texName Filename of the texture file
	* \returns a pointer to the new texture, or an old one if one with this name  was found
	*
	*/
	VETexture *VESceneManager::createTexture(std::string name, std::string basedir, std::string texName)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return createTexture2(name, basedir, texName);
	}

	/**
	*
	* \brief Create a new texture with a given name
	*
	* \param[in] name Name of the texture.
	* \param[in] basedir Name of the directory the file is in
	* \param[in] texName Filename of the texture file
	* \returns a pointer to the new texture, or an old one if one with this name  was found
	*
	*/
	VETexture *VESceneManager::createTexture2(std::string name, std::string basedir, std::string texName)
	{
		if (m_textures.count(name) > 0)
			return m_textures[name]; //if the texture already exists, return it

		VETexture *pTex = new VETexture(name, basedir, { texName }); //create the texture
		m_textures[name] = pTex; //store in texture list
		return pTex;
	}

	/**
	*
	* \brief Get a texture with a given name
	*
	* \param[in] name Name of the texture.
	* \returns a pointer to the texture
	*
	*/
	VETexture *VESceneManager::getTexture(std::string name)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_textures.count(name) == 0)
			return nullptr;
		return m_textures[name];
	}

	/**
	*
	* \brief Delete a texture given its name
	*
	* \param[in] name Name of the texture to be deleted.
	*
	*/
	void VESceneManager::deleteTexture(std::string name)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_textures.count(name) > 0)
		{ //if the texture exists
			VETexture *pTex = m_textures[name]; //get pointer to the texture
			m_textures.erase(name); //remove from the texture list
			delete pTex; //delete it
		}
	}

	/**
	*
	* \brief Create a new material with a given name
	*
	* \param[in] name Name of the material.
	* \returns a pointer to the new material, or an old one if one with this name  was found
	*
	*/
	VEMaterial *VESceneManager::createMaterial(std::string name)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return createMaterial2(name);
	}

	/**
	*
	* \brief Create a new material with a given name
	*
	* \param[in] name Name of the material.
	* \returns a pointer to the new material, or an old one if one with this name  was found
	*
	*/
	VEMaterial *VESceneManager::createMaterial2(std::string name)
	{
		if (m_materials.count(name) > 0)
			return m_materials[name]; //if the material alredy exists, return it

		VEMaterial *pMat = new VEMaterial(name); //create the material
		m_materials[name] = pMat; //store in material map
		return pMat; //return it
	}

	/**
	* \brief Find a material by its name and return a pointer to it
	* \param[in] name The name of material
	* \returns a material given its name
	*/
	VEMaterial *VESceneManager::getMaterial(std::string name)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_materials.count(name) == 0)
			return nullptr;
		return m_materials[name];
	};

	/**
	*
	* \brief Delete a material given its name
	*
	* \param[in] name Name of the material.
	*
	*/
	void VESceneManager::deleteMaterial(std::string name)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_materials.count(name) > 0)
		{ //if the material exists
			VEMaterial *pMat = m_materials[name]; //get a pointer to it
			m_materials.erase(name); //remove from material map
			delete pMat; //delete it
		}
	}

	/**
	* \brief Close down the scene manager and delete all its assets.
	*/
	void VESceneManager::closeSceneManager()
	{
		for (auto ent : m_sceneNodes)
			delete ent.second;
		delete m_rootSceneNode;
		for (auto mesh : m_meshes)
			delete mesh.second;
		for (auto mat : m_materials)
			delete mat.second;
		for (auto tex : m_textures)
			delete tex.second;

		for (auto list : m_memoryBlockMap)
		{
			vh::vhMemBlockListClear(list.second);
		}
	}

	/**
	* \brief Print a list of all entities to the console.
	*/
	void VESceneManager::printSceneNodes()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto pEnt : m_sceneNodes)
		{
			std::cout << pEnt.second->getName() << "\n";
		}
	}

	/**
	*
	* \brief Print a list of all entities in an entity tree to the console.
	*
	* \param[in] root Pointer to the root entity of the tree.
	*
	*/
	void VESceneManager::printTree(VESceneNode *root)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		std::cout << root->getName() << "\n";
		for (uint32_t i = 0; i < root->getChildrenList().size(); i++)
		{
			printTree(root->getChildrenList()[i]);
		}
	}

	//---------------------------------------------------------------------------------------------
	//deprecated

	/**
	*
	* \brief Create a cube map based sky box
	*
	* This function uses GLI to load either a ktx or dds file containing a cube map.
	* The cube is then rotated an scaled so that it can be used as sky box.
	*
	* \param[in] entityName Name of the new entity.
	* \param[in] basedir Name of the directory the texture file is in
	* \param[in] filename Name of the texture file.
	* \returns a pointer to the new entity
	*
	*/
	/*VESceneNode *	VESceneManager::createCubemap(std::string entityName, std::string basedir,
		std::string filename) {

		VEEntity::veEntityType entityType = VEEntity::VE_ENTITY_TYPE_CUBEMAP;
		VkImageCreateFlags createFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_CUBE;

	#ifdef __MACOS__
		entityType = VEEntity::VE_ENTITY_TYPE_CUBEMAP2;
		createFlags = 0;
		viewType = 0;
	#endif

		std::string filekey = basedir + "/" + filename;

		VEMesh * pMesh = m_meshes[STANDARD_MESH_INVCUBE];

		VEMaterial *pMat = m_materials[filekey];
		if (pMat == nullptr) {
			pMat = new VEMaterial(filekey);
			m_materials[filekey] = pMat;

			gli::texture_cube texCube(gli::load(filekey));
			if (texCube.empty()) {
				throw std::runtime_error("Error: Could not load cubemap file " + filekey + "!");
			}

			pMat->mapDiffuse = new VETexture(filekey, texCube, createFlags, viewType);
		}

		VESceneNode *pEntity = createEntity(entityName, entityType, pMesh, pMat, glm::mat4(1.0f), m_rootSceneNode);
		pEntity->setTransform(glm::scale(glm::vec3(10000.0f, 10000.0f, 10000.0f)));

		sceneGraphChanged2();
		return pEntity;
	}*/

	/**
	*
	* \brief Create a cube map based sky box
	*
	* This function loads 6 textures to use them in a cube map.
	* The cube is then rotated an scaled so that it can be used as sky box.
	*
	* \param[in] entityName Name of the new entity.
	* \param[in] basedir Name of the directory the texture file is in
	* \param[in] filenames List of 6 names of the texture files. Order must be ft bk up dn rt lf
	* \returns a pointer to the new entity
	*
	*/
	/*VESceneNode * VESceneManager::createCubemap(std::string entityName, std::string basedir,
		std::vector<std::string> filenames) {

		VEEntity::veEntityType entityType = VEEntity::VE_ENTITY_TYPE_CUBEMAP;
		VkImageCreateFlags createFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_CUBE;

	#ifdef __MACOS__
		entityType = VEEntity::VE_ENTITY_TYPE_CUBEMAP2;
		createFlags = VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
		viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	#endif

		std::string filekey = basedir + "/";
		std::string addstring = "";
		for (auto filename : filenames) {
			filekey += addstring + filename;
			addstring = "+";
		}

		VEMesh * pMesh = m_meshes[STANDARD_MESH_INVCUBE];

		VEMaterial *pMat = m_materials[filekey];
		if (pMat == nullptr) {
			pMat = new VEMaterial(filekey);
			m_materials[filekey] = pMat;

			pMat->mapDiffuse = new VETexture(entityName, basedir, filenames, createFlags, viewType);
		}

		VEEntity *pEntity = createEntity(entityName, entityType, pMesh, pMat, glm::mat4(1.0f), m_rootSceneNode);
		pEntity->setTransform(glm::scale(glm::vec3(500.0f, 500.0f, 500.0f)));
		pEntity->m_castsShadow = false;

		sceneGraphChanged2();
		return pEntity;
	}*/

} // namespace ve
