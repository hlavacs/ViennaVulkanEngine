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
		std::map<std::string, VEMesh *>		m_meshes = {};		///< storage of all entities currently in the engine
		std::map<std::string, VEMaterial*>	m_materials = {};	///< storage of all entities currently in the engine
		std::map<std::string, VEEntity*>	m_entities = {};	///< storage of all entities currently in the engine

		std::string			  m_cameraName = "";				///<entity name of the current camera
		std::set<std::string> m_lightNames = {};				///<names of the lights to use

		virtual void initSceneManager();
		virtual void closeSceneManager();

	public:
		///Constructor
		VESceneManager();
		///Destructor
		~VESceneManager() {};

		//------------------------------------------------------------------------
		const aiScene *	loadAssets(	std::string basedir, std::string filename, uint32_t aiFlags, 
									std::vector<VEMesh*> &meshes, std::vector<VEMaterial*> &materials);
		VEEntity *		loadModel(std::string entityName, std::string basedir, std::string filename, uint32_t aiFlags=0, VEEntity *parent=nullptr);
		void			copyAiNodes(const aiScene* pScene, std::vector<VEMesh*> &meshes, std::vector<VEMaterial*> &materials, aiNode* node, VEEntity *parent);
		void			createMeshes(const aiScene* pScene,std::string filekey, std::vector<VEMesh*> &meshes);
		void			createMaterials(const aiScene* pScene,  std::string basedir, std::string filekey, std::vector<VEMaterial*> &materials);
		VEEntity *		createEntity(std::string entityName, VEMesh *pMesh, VEMaterial *pMat, aiMatrix4x4 transf, VEEntity *parent=nullptr );
		VEEntity *		createEntity(std::string entityName, VEMesh *pMesh, VEMaterial *pMat, glm::mat4 transf, VEEntity *parent = nullptr);
		VEEntity *		createEntity(std::string entityName, VEEntity::veEntityType type, VEMesh *pMesh, VEMaterial *pMat, glm::mat4 transf, VEEntity *parent = nullptr);
		VEEntity *		createCubemap(std::string entityName, std::string basedir, std::string filename );
		VEEntity *		createCubemap(std::string entityName, std::string basedir, std::vector<std::string> filenames);

		//------------------------------------------------------------------------
		///Add an entity to the scene
		void			addEntity(VEEntity *entity) { m_entities[entity->getName()] = entity; };
		VEEntity *		getEntity(std::string entityName);
		void			deleteEntityAndSubentities(std::string name);
		void			createEntityList(VEEntity *pEntity, std::vector<std::string> &namelist);

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

		///\returns the name of the current camera
		std::string		getCameraName() { return m_cameraName; };
		/**
		* \brief Set the name of the current camera
		* \param[in] name The name of the camera
		*/
		void			setCameraName(std::string name) { m_cameraName = name; };
		///\returns a pointer to the current camera
		VECamera *      getCamera() { return static_cast<VECamera*>(m_entities[m_cameraName]);  };
		///\returns a list with names of the current lights shining on the scene
		std::set<std::string> & getLightnames() { return m_lightNames;  };

		//------------------------------------------------------------------------
		void			printEntities();
		void			printTree(VEEntity *root);
	};

}

