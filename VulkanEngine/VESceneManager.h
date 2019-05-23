/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#pragma once


#ifndef getSceneManagerPointer
#define getSceneManagerPointer() g_pVESceneManagerSingleton
#endif

#ifndef getRoot
#define getRoot() getSceneManagerPointer()->getRootSceneNode()
#endif


namespace ve {

	class VEEngine;
	class VERenderer;
	class VERendererForward;

	class VESceneManager;
	extern VESceneManager* g_pVESceneManagerSingleton;	///<Pointer to the only class instance 

	class VESubrenderFW_Shadow;

	/**
	*
	* \brief The scene Manager manages the objects that have been loaded and put into the world.
	*
	* The scene manager can load objects from file and place them into the world. It also maintains
	* cameras and lights. 
	*
	*/
	class VESceneManager {
		friend VEEngine;
		friend VERenderer;
		friend VERendererForward;
		friend VESubrenderFW_Shadow;

	protected:
		std::map<std::string, VEMesh *>		m_meshes = {};		///<Storage of all meshes currently in the engine
		std::map<std::string, VEMaterial*>	m_materials = {};	///<Storage of all materials currently in the engine
		std::map<std::string, VESceneNode*>	m_sceneNodes = {};	///<Storage of all scene nodes currently in the engine
		
		VESceneNode						*	m_rootSceneNode;	///<The root node of the scene graph
		std::map<VESceneObject::veObjectType, std::vector<vh::vhMemoryBlock*>> m_memoryBlockMap;	///<memory for the UBOs of the entities

		VECamera *				m_camera = nullptr;			///<entity ptr of the current camera
		std::vector<VELight*>	m_lights = {};				///<ptrs to the lights to use
		std::mutex				m_mutex;					///<Mutex for multithreading, locks the scene manager

		virtual void initSceneManager();
		virtual void closeSceneManager();
		void createMeshes(const aiScene* pScene,std::string filekey, std::vector<VEMesh*> &meshes);
		void createMaterials(const aiScene* pScene,  std::string basedir, std::string filekey, std::vector<VEMaterial*> &materials);
		void copyAiNodes(	const aiScene* pScene,  std::vector<VEMesh*> &meshes, 
							std::vector<VEMaterial*> &materials, aiNode* node, VESceneNode *parent);

		//private shadow functions for the public API, so API does not lock itself
		VESceneNode *	createSceneNode2(std::string name, glm::mat4 transf = glm::mat4(1.0f), VESceneNode *parent = nullptr);
		VEEntity *		createEntity2(std::string entityName, VEEntity::veEntityType type, VEMesh *pMesh, VEMaterial *pMat, glm::mat4 transf, VESceneNode *parent = nullptr);
		void			addSceneNode2(VESceneNode *pNode, VESceneNode *parent = nullptr);
		void			createSceneNodeList2(VESceneNode *pObject, std::vector<std::string> &namelist);
		VEEntity *		createSkyplane2(std::string entityName, std::string basedir, std::string texName);
		void			sceneGraphChanged();			//tell renderer to rerecord the cmd buffers

		//locking and unlocking the scene manager for thread safety
		void lockSceneManager();	//lock the scene manager mutex		
		void unlockSceneManager();	//unlock the scene manager mutex

	public:
		///Constructor of class VESceneManager
		VESceneManager();
		///Destructor of class VESceneManager
		~VESceneManager() {};

		//-------------------------------------------------------------------------------------
		//Public API of the scene manager - will lock and unlock the scene manager to make it thread save
		//then access internal data structures or call internal shadow functions

		//-------------------------------------------------------------------------------------
		//Load assets

		const aiScene *	loadAssets(	std::string basedir, std::string filename, uint32_t aiFlags,
									std::vector<VEMesh*> &meshes, std::vector<VEMaterial*> &materials);
		VESceneNode *	loadModel(	std::string entityName, std::string basedir, std::string filename, 
									uint32_t aiFlags=0, VESceneNode *parent=nullptr);

		//-------------------------------------------------------------------------------------
		//Create scene nodes and entities

		VESceneNode *	createSceneNode( std::string name, glm::mat4 transf = glm::mat4(1.0f), VESceneNode *parent = nullptr);
		VEEntity *		createEntity(std::string entityName, VEMesh *pMesh, VEMaterial *pMat, glm::mat4 transf, VESceneNode *parent = nullptr);
		VEEntity *		createEntity(std::string entityName, VEEntity::veEntityType type, VEMesh *pMesh, VEMaterial *pMat, glm::mat4 transf, VESceneNode *parent = nullptr);

		//-------------------------------------------------------------------------------------
		//Create cubemaps and skyboxes

		VEEntity *		createSkyplane(std::string entityName, std::string basedir, std::string texName);
		VESceneNode *	createSkybox(std::string entityName, std::string basedir, std::vector<std::string> texNames);

		//-------------------------------------------------------------------------------------
		//Manage scene nodes and entities

		///\returns the root scene node
		VESceneNode *	getRootSceneNode() { return m_rootSceneNode; };
		//----------------------------------------------------------------
		//API that needs to by synchronized
		void			updateSceneNodes( uint32_t imageIndex );
		void			addSceneNode(VESceneNode *pNode, VESceneNode *parent=nullptr);
		VESceneNode *	getSceneNode(std::string entityName);
		void			removeSceneNode(std::string name);
		void			deleteSceneNodeAndChildren(std::string name);
		void			createSceneNodeList(VESceneNode *pObject, std::vector<std::string> &namelist);

		//-------------------------------------------------------------------------------------
		//Manage meshes, materials, cameras, lights

		VEMesh *		getMesh(std::string name);
		void			deleteMesh(std::string name);
		/**
		* \brief Find a material by its name and return a pointer to it
		* \param[in] name The name of material
		* \returns a material given its name
		*/
		VEMaterial *	getMaterial(std::string name);
		void			deleteMaterial(std::string name);

		///\returns a pointer to the current camera
		VECamera*		getCamera() { return m_camera; };
		/**
		* \brief Set the the current camera
		* \param[in] cam Pointer to the camera
		*/
		void			setCamera( VECamera *cam) { m_camera = cam; };
		///\returns a list with names of the current lights shining on the scene
		std::vector<VELight*> & getLights() { return m_lights;  };
		void			switchOnLight(VELight * light);		//Add a light to the m_lights list
		void			switchOffLight(VELight *light);		//Remove a light from the m_lights list

		//-------------------------------------------------------------------------------------
		//Print information about the tree of objects

		void			printSceneNodes();
		void			printTree(VESceneNode *root);

		//-------------------------------------------------------------------------------------
		//deprecated
		//VESceneNode *	createCubemap(std::string entityName, std::string basedir, std::string filename);
		//VESceneNode *	createCubemap(std::string entityName, std::string basedir, std::vector<std::string> filenames);

	};

}

