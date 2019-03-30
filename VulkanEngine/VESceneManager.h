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
		std::map<std::string, VEMesh *>		m_meshes = {};				///< storage of all entities currently in the engine
		std::map<std::string, VEMaterial*>	m_materials = {};			///< storage of all entities currently in the engine
		std::map<std::string, VEMovableObject*>	m_movableObjects = {};	///< storage of all entities currently in the engine

		VECamera *				m_camera = nullptr;			///<entity ptr of the current camera
		std::vector<VELight*>	m_lights = {};				///<ptrs to the lights to use

		virtual void initSceneManager();
		virtual void closeSceneManager();
		void copyAiNodes(const aiScene* pScene, std::vector<VEMesh*> &meshes, std::vector<VEMaterial*> &materials, aiNode* node, VEMovableObject *parent);

	public:
		///Constructor
		VESceneManager();
		///Destructor
		~VESceneManager() {};

		//------------------------------------------------------------------------
		const aiScene *		loadAssets(	std::string basedir, std::string filename, uint32_t aiFlags, 
									std::vector<VEMesh*> &meshes, std::vector<VEMaterial*> &materials);
		void				createMeshes(const aiScene* pScene,std::string filekey, std::vector<VEMesh*> &meshes);
		void				createMaterials(const aiScene* pScene,  std::string basedir, std::string filekey, std::vector<VEMaterial*> &materials);
		VEMovableObject *	loadModel(std::string entityName, std::string basedir, std::string filename, uint32_t aiFlags=0, VEMovableObject *parent=nullptr);

		//------------------------------------------------------------------------
		VEMovableObject *	createMovableObject( std::string name, glm::mat4 transf = glm::mat4(1.0f), VEMovableObject *parent = nullptr);
		VEEntity *			createEntity(std::string entityName, VEMesh *pMesh, VEMaterial *pMat, aiMatrix4x4 transf, VEMovableObject *parent=nullptr );
		VEEntity *			createEntity(std::string entityName, VEMesh *pMesh, VEMaterial *pMat, glm::mat4 transf, VEMovableObject *parent = nullptr);
		VEEntity *			createEntity(std::string entityName, VEEntity::veEntityType type, VEMesh *pMesh, VEMaterial *pMat, glm::mat4 transf, VEMovableObject *parent = nullptr);

		//------------------------------------------------------------------------
		VEMovableObject *	createCubemap(std::string entityName, std::string basedir, std::string filename );
		VEMovableObject *	createCubemap(std::string entityName, std::string basedir, std::vector<std::string> filenames );
		VEEntity *			createSkyplane(std::string entityName, std::string basedir, std::string texName);
		VEMovableObject *	createSkybox(std::string entityName, std::string basedir, std::vector<std::string> texNames);

		//------------------------------------------------------------------------
		///Add a movable object to the scene
		void				addMovableObject(VEMovableObject *entity) { m_movableObjects[entity->getName()] = entity; };
		VEMovableObject *	getMovableObject(std::string entityName);
		void				deleteMOAndChildren(std::string name);
		void				createMOList(VEMovableObject *pObject, std::vector<std::string> &namelist);

		//------------------------------------------------------------------------
		/**
		* \brief Find a mesh by its name and return a pointer to it
		* \param[in] name The name of mesh
		* \returns a mesh given its name
		*/
		VEMesh *		getMesh(std::string name) { return m_meshes[name]; };
		void			deleteMesh(std::string name);
		/**
		* \brief Find a material by its name and return a pointer to it
		* \param[in] name The name of material
		* \returns a material given its name
		*/
		VEMaterial *	getMaterial(std::string name) { return m_materials[name]; };
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
		void switchOnLight(VELight * light);	//Add a light to the m_lights list
		void switchOffLight(VELight *light);	//Remove a light from the m_lights list

		//------------------------------------------------------------------------
		void			printMovableObjects();
		void			printTree(VEMovableObject *root);
	};

}

