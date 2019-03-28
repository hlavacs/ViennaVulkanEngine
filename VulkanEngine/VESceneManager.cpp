/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"

#define STANDARD_MESH_CUBE		"models/standard/cube.obj/cube"
#define STANDARD_MESH_INVCUBE	"models/standard/invcube.obj/cube"
#define STANDARD_MESH_PLANE		"models/standard/plane.obj/plane"
#define STANDARD_MESH_SPHERE	"models/standard/sphere.obj/sphere"


namespace ve {

	VESceneManager * g_pVESceneManagerSingleton = nullptr;	///<Singleton pointer to the only VESceneManager instance


	VESceneManager::VESceneManager() {
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
	void VESceneManager::initSceneManager() {
		std::vector<VEMesh*> meshes;
		std::vector<VEMaterial*> materials;

		loadAssets("models/standard", "cube.obj", 0, meshes, materials);
		loadAssets("models/standard", "invcube.obj", aiProcess_FlipWindingOrder, meshes, materials);
		loadAssets("models/standard", "plane.obj", 0, meshes, materials);
		loadAssets("models/standard", "sphere.obj", 0, meshes, materials);

		//camera parent is used for translation rotations
		VEEntity *cameraParent = new VEEntity("StandardCameraParent");
		cameraParent->setTransform( glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)) );
		addEntity(cameraParent);

		//camera can only do yaw (parent y-axis) and pitch (local x-axis) rotations
		VkExtent2D extent = getWindowPointer()->getExtent();
		VECamera *camera = new VECameraProjective("StandardCamera", 1.0f, 501.0f, extent.width/ (float)extent.height, 45.0f);
		camera->lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		camera->m_pEntityParent = cameraParent;
		addEntity(camera);
		setCamera( camera );

		//use one light source
		VELight *light = new VELight("StandardLight", VELight::VE_LIGHT_TYPE_DIRECTIONAL );
		light->lookAt(glm::vec3(0.0f, 20.0f, -20.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		addEntity(light);
		m_lights.push_back(light);
	};

	//-----------------------------------------------------------------------------------------------------------------------

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
	const aiScene* VESceneManager::loadAssets(	std::string basedir, std::string filename, uint32_t aiFlags, 
												std::vector<VEMesh*> &meshes, std::vector<VEMaterial*> &materials) {
		Assimp::Importer importer;

		std::string filekey = basedir + "/" + filename;

		const aiScene* pScene = importer.ReadFile( filekey, 
			//aiProcess_RemoveRedundantMaterials |
			//aiProcess_PreTransformVertices |
			aiProcess_GenNormals | 
			aiProcess_CalcTangentSpace |
			aiProcess_Triangulate |
			//aiProcess_JoinIdenticalVertices | 
			//aiProcess_FixInfacingNormals |
			aiFlags);

		if (pScene == nullptr) {
			throw std::runtime_error("Error: Could not load asset file " + filekey + "!");
			return nullptr;
		}
		createMeshes(pScene, filekey, meshes);
		createMaterials(pScene, basedir, filekey, materials);

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
	VEEntity * VESceneManager::loadModel(	std::string entityName, std::string basedir, std::string filename, 
											uint32_t aiFlags, VEEntity *parent) {

		Assimp::Importer importer;

		std::string filekey = basedir + "/" + filename;

		const aiScene* pScene = importer.ReadFile(filekey,
			//aiProcess_RemoveRedundantMaterials |
			//aiProcess_PreTransformVertices |
			aiProcess_GenNormals |
			aiProcess_CalcTangentSpace |
			aiProcess_Triangulate |
			//aiProcess_JoinIdenticalVertices |
			//aiProcess_FixInfacingNormals |
			aiFlags);

		if (pScene == nullptr) {
			throw std::runtime_error("Error: Could not load asset file " + filekey + "!");
			return nullptr;
		}
		std::vector<VEMesh*> meshes;
		createMeshes(pScene, filekey, meshes);

		std::vector<VEMaterial*> materials;
		createMaterials(pScene, basedir, filekey, materials);

		VEEntity *pEntity = m_entities[entityName];
		if (pEntity != nullptr) return pEntity;
			
		pEntity = createEntity( entityName, nullptr, nullptr, aiMatrix4x4(), parent );

		copyAiNodes( pScene, meshes, materials, pScene->mRootNode, pEntity );
		pEntity->update();	//update ubos of all children

		return pEntity;
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
	void VESceneManager::copyAiNodes(	const aiScene* pScene, 
										std::vector<VEMesh*> &meshes, 
										std::vector<VEMaterial*> &materials, 
										aiNode* node, 
										VEEntity *parent ) {

		VEEntity *pEntity = createEntity(	parent->getName() + "/" + node->mName.C_Str(),	
											nullptr, nullptr, aiMatrix4x4(), parent);

		for (uint32_t i = 0; i < node->mNumMeshes; i++) {	//go through the meshes of the Assimp node

			VEMesh * pMesh = nullptr;
			VEMaterial * pMaterial = nullptr;

			uint32_t paiMeshIdx = node->mMeshes[i];			//get mesh index in global mesh list
			pMesh = meshes[paiMeshIdx];						//use index to get pointer to VEMesh
			aiMesh * paiMesh = pScene->mMeshes[paiMeshIdx];	//also get handle to the Assimp mesh

			uint32_t paiMatIdx = paiMesh->mMaterialIndex;	//get the material index for this mesh
			pMaterial = materials[paiMatIdx];				//use the index to get the right VEMaterial

			VEEntity *pEnt = createEntity(  pEntity->getName() + "/Entity_" + std::to_string(i), //create the new entity
											pMesh, pMaterial, node->mTransformation, pEntity);
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++) {		//recursivly go down the node tree
			copyAiNodes(pScene, meshes, materials, node->mChildren[i], pEntity);
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
	void VESceneManager::createMeshes(const aiScene* pScene, std::string filekey, std::vector<VEMesh*> &meshes) {

		VEMesh *pMesh = nullptr;

		for (uint32_t i = 0; i < pScene->mNumMeshes; i++) {
			const aiMesh *paiMesh = pScene->mMeshes[i];
			std::string name = filekey + "/" + paiMesh->mName.C_Str();

			VEMesh *pMesh = m_meshes[name];
			if (pMesh == nullptr) {
				pMesh = new VEMesh(name, paiMesh);
				m_meshes[name] = pMesh;
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
	void VESceneManager::createMaterials(	const aiScene* pScene, std::string basedir, std::string filekey, 
											std::vector<VEMaterial*> &materials) {

		for (uint32_t i = 0; i < pScene->mNumMaterials; i++) {
			aiMaterial *paiMat = pScene->mMaterials[i];
			aiString matname("");
			paiMat->Get(AI_MATKEY_NAME, matname);

			std::string name = filekey + "/" + matname.C_Str();
			VEMaterial *pMat = m_materials[name];
			if (pMat == nullptr) {
				pMat = new VEMaterial(name);
				m_materials[name] = pMat;
				paiMat->Get(AI_MATKEY_SHADING_MODEL, pMat->shading);

				aiColor3D color(0.f, 0.f, 0.f);
				if (paiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
					pMat->color.r = color.r; pMat->color.g = color.g; pMat->color.b = color.b; pMat->color.a = 1.0f;
				}

				/*for (uint32_t i = 0; i < paiMat->mNumProperties; i++) {
					aiMaterialProperty *paimatprop = paiMat->mProperties[i];
					aiString key = paimatprop->mKey;
					std::string skey(key.C_Str());
					uint32_t j = 0;
				}*/

				for (uint32_t i = 0; i < paiMat->GetTextureCount(aiTextureType_AMBIENT); i++) {
					aiString str;
					paiMat->GetTexture(aiTextureType_AMBIENT, i, &str);
				}
				for (uint32_t i = 0; i < paiMat->GetTextureCount(aiTextureType_DIFFUSE); i++) {
					aiString str;
					paiMat->GetTexture(aiTextureType_DIFFUSE, i, &str);

					std::string name(str.C_Str());
					pMat->mapDiffuse = new VETexture(filekey + "/" + name, basedir, { name });
				}
				for (uint32_t i = 0; i < paiMat->GetTextureCount(aiTextureType_SPECULAR); i++) {
					aiString str;
					paiMat->GetTexture(aiTextureType_SPECULAR, i, &str);
				}
				for (uint32_t i = 0; i < paiMat->GetTextureCount(aiTextureType_LIGHTMAP); i++) {
					aiString str;
					paiMat->GetTexture(aiTextureType_LIGHTMAP, i, &str);
				}
				for (uint32_t i = 0; i < paiMat->GetTextureCount(aiTextureType_NORMALS); i++) {
					aiString str;
					paiMat->GetTexture(aiTextureType_NORMALS, i, &str);

					std::string name(str.C_Str());
					if (pMat->mapNormal == nullptr) pMat->mapNormal = new VETexture(filekey + "/" + name, basedir, { name });
				}
				for (uint32_t i = 0; i < paiMat->GetTextureCount(aiTextureType_DISPLACEMENT); i++) {
					aiString str;
					paiMat->GetTexture(aiTextureType_DISPLACEMENT, i, &str);

					std::string name(str.C_Str());
					if (pMat->mapBump == nullptr) pMat->mapBump = new VETexture(filekey + "/" + name, basedir, { name });
				}
				for (uint32_t i = 0; i < paiMat->GetTextureCount(aiTextureType_HEIGHT); i++) {
					aiString str;
					paiMat->GetTexture(aiTextureType_HEIGHT, i, &str);

					std::string name(str.C_Str());
					if (pMat->mapHeight == nullptr) pMat->mapHeight = new VETexture(filekey + "/" + name, basedir, { name });
				}
			}
			materials.push_back(pMat);
		}
	}

	/**
	* \brief Create an entity
	*
	* \param[in] entityName The name of the new entity. 
	* \param[in] pMesh Pointer the mesh for this entity.
	* \param[in] pMat Pointer to the material for this entity.
	* \param[in] transf Local to parent transform, given as Assimp matrix.
	* \param[in] parent Pointer to entity to be used as parent.
	* \returns a pointer to the new entity
	*
	*/
	VEEntity * VESceneManager::createEntity(std::string entityName, VEMesh *pMesh, VEMaterial *pMat, 
											aiMatrix4x4 transf, VEEntity *parent) {
		glm::mat4 *pMatrix = (glm::mat4*) &transf;
		return createEntity( entityName, pMesh, pMat, *pMatrix, parent);
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
	VEEntity * VESceneManager::createEntity(std::string entityName, VEMesh *pMesh, VEMaterial *pMat,
											glm::mat4 transf, VEEntity *parent) {
		return createEntity(entityName, VEEntity::VE_ENTITY_TYPE_OBJECT, pMesh, pMat, transf, parent);
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
	VEEntity * VESceneManager::createEntity(std::string entityName, VEEntity::veEntityType type, VEMesh *pMesh,
											VEMaterial *pMat, glm::mat4 transf, VEEntity *parent) {
		VEEntity *pEntity = new VEEntity(entityName, type, pMesh, pMat, transf, parent);
		addEntity(pEntity);				// store entity in the entity array

		if (pMesh != nullptr && pMat != nullptr) {
			getRendererPointer()->addEntityToSubrenderer(pEntity);
			pEntity->update();
		}
		return pEntity;
	}

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
	VEEntity *	VESceneManager::createCubemap(	std::string entityName, std::string basedir, 
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
				return nullptr;
			}

			pMat->mapDiffuse = new VETexture( filekey, texCube, createFlags, viewType );
		}

		VEEntity *pEntity = createEntity(entityName, entityType, pMesh, pMat, glm::mat4(1.0f), nullptr );
		pEntity->setTransform(glm::scale(glm::vec3(10000.0f, 10000.0f, 10000.0f)));

		return pEntity;
	}

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
	VEEntity *	VESceneManager::createCubemap(	std::string entityName, std::string basedir, 
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

			pMat->mapDiffuse = new VETexture(entityName, basedir, filenames, createFlags, viewType );
		}

		VEEntity *pEntity = createEntity(entityName, entityType, pMesh, pMat, glm::mat4(1.0f), nullptr);
		pEntity->setTransform(glm::scale(glm::vec3(500.0f, 500.0f, 500.0f)));
		pEntity->m_castsShadow = false;

		return pEntity;
	}

	VEEntity *	VESceneManager::createSkyplane(std::string entityName, std::string basedir, std::string texName) {

		std::string filekey = basedir + "/" + texName;
		VEMesh * pMesh = m_meshes[STANDARD_MESH_PLANE];

		VEMaterial *pMat = m_materials[filekey];
		if (pMat == nullptr) {
			pMat = new VEMaterial(filekey);
			m_materials[filekey] = pMat;

			pMat->mapDiffuse = new VETexture(entityName, basedir, { texName });
		}

		VEEntity *pEntity = createEntity(entityName, VEEntity::VE_ENTITY_TYPE_SKYPLANE, pMesh, pMat, glm::mat4(1.0f), nullptr);
		pEntity->m_castsShadow = false;

		return pEntity;
	}

	//  ft bk up dn rt lf
	VEEntity * VESceneManager::createSkybox(std::string entityName, std::string basedir, std::vector<std::string> texNames) {
		std::string filekey = basedir + "/";
		std::string addstring = "";
		for (auto filename : texNames) {
			filekey += addstring + filename;
			addstring = "+";
		}

		VEEntity *parent = new VEEntity(entityName);

		float scale = 1000.0f;

		VEEntity *sp1 = getSceneManagerPointer()->createSkyplane(filekey + "/Skyplane1", basedir, texNames[0]);
		sp1->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(-scale, 1.0f, -scale)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), -(float)M_PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)));
		sp1->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, scale / 2.0f)));
		parent->addChild(sp1);

		sp1 = getSceneManagerPointer()->createSkyplane(filekey + "/Skyplane2", basedir, texNames[1]);
		sp1->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(scale, 1.0f, scale)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), (float)M_PI / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f)));
		sp1->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -scale / 2.0f)));
		parent->addChild(sp1);

		sp1 = getSceneManagerPointer()->createSkyplane(filekey + "/Skyplane3", basedir, texNames[2]);
		sp1->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(scale, 1.0f, scale)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), (float)M_PI / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), (float)M_PI, glm::vec3(1.0f, 0.0f, 0.0f)));
		sp1->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, scale / 2.0f, 0.0f)));
		parent->addChild(sp1);

		sp1 = getSceneManagerPointer()->createSkyplane(filekey + "/Skyplane4", basedir, texNames[4] );
		sp1->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(-scale, 1.0f, -scale)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), (float)M_PI / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), (float)M_PI / 2.0f, glm::vec3(0.0f, 0.0f, 01.0f)));
		sp1->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(scale / 2.0f, 0.0f, 0.0f)));
		parent->addChild(sp1);

		sp1 = getSceneManagerPointer()->createSkyplane(filekey + "/Skyplane5", basedir, texNames[5]);
		sp1->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(scale, 1.0f, scale)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), (float)M_PI / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
		sp1->multiplyTransform(glm::rotate(glm::mat4(1.0f), -(float)M_PI / 2.0f, glm::vec3(0.0f, 0.0f, 1.0f)));
		sp1->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-scale / 2.0f, 0.0f, 0.0f)));
		parent->addChild(sp1);

		return parent;
	}



	//----------------------------------------------------------------------------------------------------------------
	//scene management stuff

	/**
	*
	* \brief Find an entity using its name
	*
	* \param[in] name Name of the entity.
	* \returns a pointer to the entity
	*
	*/
	VEEntity * VESceneManager::getEntity(std::string name) {
		if (m_entities.count(name) > 0) return m_entities[name];
		return nullptr;
	}

	/**
	*
	* \brief Delete an entity and all its subentities
	*
	* \param[in] name Name of the entity.
	*
	*/
	void VESceneManager::deleteEntityAndSubentities(std::string name) {
		VEEntity * pEntity = m_entities[name];
		if (pEntity == nullptr) return;

		if (pEntity->m_entityType == VEEntity::VE_ENTITY_TYPE_CUBEMAP) {	//remove the event listener associated with this entity
			getEnginePointer()->deleteEventListener(name);
		}

		if (pEntity->m_pEntityParent != nullptr) pEntity->m_pEntityParent->removeChild(pEntity);

		std::vector<std::string> namelist;
		createEntityList(pEntity, namelist);

		//delete entities
		for (uint32_t i = 0; i < namelist.size(); i++) {
			VEEntity *pEntity = m_entities[namelist[i]];
			getRendererPointer()->removeEntityFromSubrenderers(pEntity);
			m_entities.erase(namelist[i]);
			delete pEntity;
		}
	}

	/**
	*
	* \brief Create a list of all child entities of a given entity
	*
	* \param[in] pEntity Pointer to the root of the tree.
	* \param[out] namelist List of names of children of the entity.
	*
	*/
	void VESceneManager::createEntityList(VEEntity *pEntity, std::vector<std::string> &namelist) {
		namelist.push_back(pEntity->getName());

		for( uint32_t i=0; i<pEntity->m_pEntityChildren.size(); i++ ) {
			createEntityList(pEntity->m_pEntityChildren[i], namelist );
		}
	}

	/**
	*
	* \brief Delete a mesh given its name
	*
	* \param[in] name Name of the mesh.
	*
	*/
	void VESceneManager::deleteMesh(std::string name) {
		VEMesh * pMesh = m_meshes[name];
		if (pMesh != nullptr) {
			m_meshes.erase(name);
			delete pMesh;
		}
	}

	/**
	*
	* \brief Delete a material given its name
	*
	* \param[in] name Name of the material.
	*
	*/
	void VESceneManager::deleteMaterial(std::string name) {
		VEMaterial * pMat = m_materials[name];
		if (pMat != nullptr) {
			m_materials.erase(name);
			delete pMat;
		}
	}


	/**
	*
	* \brief Add a light to the m_lights list, thus switching it on
	*
	* A light must be also on the stage as entity in the scene manager's entity list. 
	* but until it is member of this list, it will not be considered as a shining light.
	*
	* \param[in] light A pointer to the light to add to the shining lights
	*
	*/
	void  VESceneManager::switchOnLight(VELight * light) {
		m_lights.push_back(light); 
	};


	/**
	*
	* \brief Remove a light from the m_lights list, thus switching it off
	*
	* Removing this light does not remove it from the m_entities list.
	* Removing it from the m_lights list causes the light to be switched off.
	*
	* \param[in] light A pointer to the light to switch off
	*
	*/
	void  VESceneManager::switchOffLight(VELight *light) {
		for (uint32_t i = 0; i < m_lights.size(); i++) {
			if (light == m_lights[i]) {
				m_lights[i] = m_lights[m_lights.size() - 1];	//overwrite with last light
				m_lights.pop_back();							//remove last light
			}
		}
	}


	/**
	* \brief Close down the scene manager and delete all its assets.
	*/
	void VESceneManager::closeSceneManager() {
		for (auto ent : m_entities) 
			delete ent.second;
		for (auto mesh : m_meshes) delete mesh.second;
		for (auto mat : m_materials) delete mat.second;
	}

	/**
	* \brief Print a list of all entities to the console.
	*/
	void VESceneManager::printEntities() {
		for (auto pEnt : m_entities) {
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
	void VESceneManager::printTree( VEEntity *root ) {
		std::cout << root->getName() << "\n";
		for (uint32_t i = 0; i < root->m_pEntityChildren.size(); i++) {
			printTree( root->m_pEntityChildren[i] );
		}
	}

}

