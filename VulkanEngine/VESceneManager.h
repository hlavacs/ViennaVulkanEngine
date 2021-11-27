/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VESCENEMANAGER_H
#define VESCENEMANAGER_H

#ifndef getSceneManagerPointer
#define getSceneManagerPointer() g_pVESceneManagerSingleton
#endif

#ifndef getRoot
#define getRoot() getSceneManagerPointer()->getRootSceneNode()
#endif

namespace ve
{
	class VEEngine;

	class VERenderer;

	class VERendererForward;

	class VERendererDeferred;

	class VESceneManager;

	extern VESceneManager *g_pVESceneManagerSingleton; ///<Pointer to the only class instance

	class VESubrenderFW_Shadow;

	class VESubrenderDF_Shadow;

	class VESubrenderDF_Composer;

	/**
		*
		* \brief The scene Manager manages the objects that have been loaded and put into the world.
		*
		* The scene manager can load objects from file and place them into the world. It also maintains
		* cameras and lights.
		*
		*/
	class VESceneManager
	{
		friend VEEngine;
		friend VERenderer;
		friend VERendererForward;
		friend VERendererDeferred;
		friend VESubrenderFW_Shadow;
		friend VESubrenderDF_Shadow;
		friend VESubrenderDF_Composer;

	protected:
		std::map<std::string, VEMesh *> m_meshes = {}; ///<Storage of all meshes currently in the engine
		std::map<std::string, VETexture *> m_textures = {}; ///<Storage of all textures
		std::map<std::string, VEMaterial *> m_materials = {}; ///<Storage of all materials currently in the engine
		std::map<std::string, VESceneNode *> m_sceneNodes = {}; ///<Storage of all scene nodes currently in the engine
		std::vector<VESceneNode *> m_deletedSceneNodes = {}; ///<List of deleted scene nodes and their children
		VESceneNode *m_rootSceneNode; ///<The root node of the scene graph
		std::map<VESceneObject::veObjectType, std::vector<vh::vhMemoryBlock *>> m_memoryBlockMap; ///<memory for the UBOs of the entities
		std::queue<std::future<void>> m_updateFutures; ///<we might do UBO updates in parallel, these are the futures to wait for

		VECamera *m_camera = nullptr; ///<Ptr to the current camera
		std::vector<VELight *> m_lights = {}; ///<ptrs to the lights to use - filled automatically
		std::mutex m_mutex; ///<Mutex for multithreading, locks the scene manager
		bool m_autoRecord = true; ///<if true, then scene graph changes automatically leasd to a cmd buffer rerecording

		virtual void initSceneManager();

		virtual void closeSceneManager();

		void createMeshes(const aiScene *pScene, std::string filekey, std::vector<VEMesh *> &meshes);

		void createMaterials(const aiScene *pScene, std::string basedir, std::string filekey, std::vector<VEMaterial *> &materials);

		void copyAiNodes(const aiScene *pScene, std::vector<VEMesh *> &meshes, std::vector<VEMaterial *> &materials, aiNode *node, VESceneNode *parent);

		//private shadow functions for the public API, so API does not lock itself
		VESceneNode *createSceneNode2(std::string name, VESceneNode *parent, glm::mat4 transf = glm::mat4(1.0f));

		VEEntity *createEntity2(std::string entityName, VEEntity::veEntityType type, VEMesh *pMesh, VEMaterial *pMat, VESceneNode *parent, glm::mat4 transf = glm::mat4(1.0f));

		void addSceneNodeAndChildren2(VESceneNode *pNode, VESceneNode *parent);

		void deleteSceneNodeAndChildren2(VESceneNode *pNode);

		void createSceneNodeList2(VESceneNode *pObject, std::vector<std::string> &namelist);

		VEEntity *
			createSkyplane2(std::string entityName, std::string basedir, std::string texName, VESceneNode *parent);

		VETexture *createTexture2(std::string name, std::string basedir, std::string texName);

		VEMaterial *createMaterial2(std::string name);

		void sceneGraphChanged2(); //tell renderer to rerecord the cmd buffers - internal
		void sceneGraphChanged3(); //tell renderer to rerecord the cmd buffers
		void updateSceneNodes(uint32_t imageIndex);

		void updateSceneNodes2(VESceneNode *pNode, glm::mat4 worldMatrix, uint32_t imageIndex);

		void updateSceneNodes3(std::vector<VESceneNode *> &children, glm::mat4 worldMatrix, uint32_t startIdx, uint32_t endIdx, uint32_t imageIndex);

		void setVisibility2(VESceneNode *pNode, bool flag); //set a whole subtree visible or not
		void notifyEventListeners(VESceneNode *pNode);

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

		const aiScene *loadAssets(std::string basedir, std::string filename, uint32_t aiFlags, std::vector<VEMesh *> &meshes, std::vector<VEMaterial *> &materials);

		VESceneNode *loadModel(std::string entityName, std::string basedir, std::string filename, uint32_t aiFlags = 0, VESceneNode *parent = nullptr);

		//-------------------------------------------------------------------------------------
		//Create scene nodes and entities
		//API that needs to by synchronized

		VESceneNode *createSceneNode(std::string name, VESceneNode *parent, glm::mat4 transf = glm::mat4(1.0f));

		VEEntity *createEntity(std::string entityName, VEMesh *pMesh, VEMaterial *pMat, VESceneNode *parent, glm::mat4 transf = glm::mat4(1.0f));

		VEEntity *createEntity(std::string entityName, VEEntity::veEntityType type, VEMesh *pMesh, VEMaterial *pMat, VESceneNode *parent, glm::mat4 transf = glm::mat4(1.0f));

		VECamera *createCamera(std::string cameraName, VECamera::veCameraType type, VESceneNode *parent, glm::mat4 transf = glm::mat4(1.0f));

		VELight *createLight(std::string lightName, VELight::veLightType type, VESceneNode *parent, glm::mat4 transf = glm::mat4(1.0f));

		//-------------------------------------------------------------------------------------
		//Create cubemaps and skyboxes

		VEEntity *createSkyplane(std::string entityName, std::string basedir, std::string texName, VESceneNode *parent);

		VESceneNode *createSkybox(std::string entityName, std::string basedir, std::vector<std::string> texNames, VESceneNode *parent);

		//-------------------------------------------------------------------------------------
		//Manage scene nodes and entities

		///\returns the root scene node
		VESceneNode *getRootSceneNode()
		{
			return m_rootSceneNode;
		};

		///\brief If true then scene graph changes automatically trigger a cmd buffer rerecording
		void setAutoRecord(bool flag)
		{
			m_autoRecord = flag;
		};

		void sceneGraphChanged(); //tell renderer to rerecord the cmd buffers
		void setVisibility(VESceneNode *pNode, bool flag); //set a whole subtree visible or not

		//----------------------------------------------------------------
		//API that needs to by synchronized
		VESceneNode *getSceneNode(std::string entityName);

		void deleteSceneNodeAndChildren(std::string name);

		void deleteScene();

		void createSceneNodeList(VESceneNode *pObject, std::vector<std::string> &namelist);

		//-------------------------------------------------------------------------------------
		//Manage meshes, materials, cameras, lights
		VEMesh *createMesh(std::string name, std::vector<vh::vhVertex> &vertices, std::vector<uint32_t> &indices);

		VEMesh *getMesh(std::string name);

		void deleteMesh(std::string name);

		VETexture *createTexture(std::string name, std::string basedir, std::string texName);

		VETexture *getTexture(std::string name);

		void deleteTexture(std::string name);

		VEMaterial *createMaterial(std::string name);

		VEMaterial *getMaterial(std::string name);

		void deleteMaterial(std::string name);

		///\returns a pointer to the current camera
		VECamera *getCamera()
		{
			return m_camera;
		};

		/**
			* \brief Set the the current camera
			* \param[in] cam Pointer to the camera
			*/
		void setCamera(VECamera *cam)
		{
			m_camera = cam;
		};

		///\returns a list with names of the current lights shining on the scene
		std::vector<VELight *> &getLights()
		{
			return m_lights;
		};

		//-------------------------------------------------------------------------------------
		//Print information about the tree of objects

		void printSceneNodes();

		void printTree(VESceneNode *root);

		//-------------------------------------------------------------------------------------
		//deprecated
		//VESceneNode *	createCubemap(std::string entityName, std::string basedir, std::string filename);
		//VESceneNode *	createCubemap(std::string entityName, std::string basedir, std::vector<std::string> filenames);
	};

} // namespace ve

#endif
