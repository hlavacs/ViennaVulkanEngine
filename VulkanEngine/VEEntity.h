/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VEENTITY_H
#define VEENTITY_H

namespace ve
{
	//----------------------------------------------------------------------------------------------
	//Scene node

	/**
		*
		* \brief Represents any object that can be put into a scene.
		*
		* VESceneNode represents any object that can be used by the scene manager to be put into the scene.
		* This includes objects, cameras, lights, sky boxes or terrains.
		* Scene nodes have a local to parent transform. This is a 4x4 matrix translating position and orientation
		* from the local (object) space to the parent space.
		* Scene nodes can have a parent. If so the local transform is relative to the parent transform. This
		* relation is stored in the parent and children pointers.
		* Since there is a parent-child relationship, scene nodes build up trees of nodes.
		* If the scene node does not have a parent,
		* it will not be updated during the update run, and it will not be drawn.
		*
		*/

	class VESceneNode : public VENamedClass
	{
		friend VESceneManager;

	public:
		///Object type, can be just a scene node, or an object that additionally needs a UBO buffer
		enum veNodeType
		{
			VE_NODE_TYPE_SCENENODE, ///<Instance of the base class, acts as scene node, cannot be drawn
			VE_NODE_TYPE_SCENEOBJECT ///<Instance of the base class, acts as scene node, cannot be drawn
		};

	protected:
		glm::mat4 m_transform = glm::mat4(
			1.0); ///<Transform from local to parent space, the engine uses Y-UP, Left-handed
		VkBuffer m_transformBuffer = VK_NULL_HANDLE; ///<Vulkan transform buffer handle
		VmaAllocation m_transformBufferAllocation = nullptr; ///<VMA allocation info

		std::vector<VESceneNode *> m_children; ///<List of entity children
		VESceneNode *m_parent = nullptr; ///<Pointer to entity parent
		std::mutex m_mutex; ///<Mutex for locking access to this node

		//constructor
		VESceneNode(std::string name, glm::mat4 transf = glm::mat4(1.0f));

		///Destructor of the scene node class.
		virtual ~VESceneNode() {};

		//--------------------------------------------------------------------------------------
		glm::mat4 getWorldTransform2(); //Compute the world matrix
		glm::mat4 getWorldRotation2(); //Compute the world rotation matrix
		glm::mat4 getRotation2(); //Return local rotation

		//--------------------------------------------------------------------------------------
		//UBO updates

		///Meant for subclasses to add data to the UBO, so this function does nothing in base class
		virtual void updateUBO(glm::mat4 worldMatrix, uint32_t imageIndex) {};

	public:
		//-------------------------------------------------------------------------------------
		//Type

		///\returns the scene node type
		virtual veNodeType getNodeType()
		{
			return VE_NODE_TYPE_SCENENODE;
		};

		///\returns the parent of this scene node
		VESceneNode *getParent()
		{
			return m_parent;
		};

		///\returns whether this scene node has a parent
		bool hasParent()
		{
			return m_parent != nullptr;
		};

		///\returns a reference to the children list of this scene node
		std::vector<VESceneNode *> &getChildrenList()
		{
			return m_children;
		};

		///\returns a copy children list of this scene node
		std::vector<VESceneNode *> getChildrenCopy()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_children;
		};

		//-------------------------------------------------------------------------------------
		//transforms - must be synchronized with mutex

		void setTransform(glm::mat4 trans); //Overwrite the transform and copy it to the UBO
		glm::mat4 getTransform(); //Return local transform
		glm::mat4 getRotation(); //Return local rotation
		void setPosition(glm::vec3 pos); //Set the position of the entity
		glm::vec3 getPosition(); //Return the current position in parent space
		glm::vec3 getXAxis(); //Return local x-axis in parent space
		glm::vec3 getYAxis(); //Return local y-axis in parent space
		glm::vec3 getZAxis(); //Return local z-axis in parent space
		void multiplyTransform(glm::mat4 trans); //Multiply the transform, e.g. translate, scale, rotate
		glm::mat4 getWorldTransform(); //Compute the world matrix
		glm::mat4 getWorldRotation(); //Compute the world matrix, only rotations
		void lookAt(glm::vec3 eye, glm::vec3 point, glm::vec3 up); //LookAt function for left handed system

		//--------------------------------------------------------------------------------------
		//manage tree, will make cmd buffers to be rerecorded since the tree is changed
		//must be synchronized

		virtual void addChild(VESceneNode *); //Add a new child
		virtual void removeChild(VESceneNode *); //Remove a child, dont destroy it

		//-------------------------------------------------------------------------------------
		//Bounding volumes

		virtual void
			getBoundingSphere(glm::vec3 *center, float *radius); //return center and radius for a bounding sphere
		virtual void
			getOBB(std::vector<glm::vec4> &pointsW, float t1, float t2, glm::vec3 &center, float &width, float &height,
				float &depth); //return min and max along the axes
	};

	//--------------------------------------------------------------------------------------------------
	//Scene object

	/**
		*
		* \brief Represents any object that has its own UBO.
		*
		* A scene object has its own UBO describing its transform and current state.
		* Scene objects thus need a vector of uniform buffers, buffer allcoations (VMA) and descriptor sets.
		* This is one for each swap chain image. In a mailbox swapchain, we usually have three images.
		* Two of them can be in flight, meaning that they are currently rendered into. Thus we need
		* at least two UBOs per object.
		*
		*/

	class VESceneObject : public VESceneNode
	{
		friend VESceneManager;

	public:
		///Object type, can be node, entity for drawing, camera or light
		enum veObjectType
		{
			VE_OBJECT_TYPE_ENTITY, ///<Normal object to be drawn
			VE_OBJECT_TYPE_CAMERA, ///<A projective camera, cannot be drawn
			VE_OBJECT_TYPE_LIGHT ///<A light, cannot be drawn
		};

	protected:
		virtual void
			updateUBO(void *pUBO, uint32_t sizeUBO, uint32_t imageIndex); //Helper function to call VMA functions

		VESceneObject(std::string name, glm::mat4 transf = glm::mat4(1.0f), uint32_t sizeUBO = 0);

		virtual ~VESceneObject();

	public:
		vh::vhMemoryHandle m_memoryHandle = {}; ///<Handle to the UBO memory

		///\returns the scene node type
		virtual veNodeType getNodeType()
		{
			return VE_NODE_TYPE_SCENEOBJECT;
		};

		///\returns the scene object type
		virtual veObjectType getObjectType() = 0;

		///\returns the size of the UBO that this object uses
		virtual uint32_t getSizeUBO() = 0;
	};

	//--------------------------------------------------------------------------------------------------
	//Entity

	class VESubrender;

	/**
		*
		* \brief Represents any object that can be drawn.
		*
		* VEEntity represents any object that can be drawn. Entities have exactly one mesh and one material.
		* Entities are associated with exactly one VESubrender instance. This instance is responsible for managing
		* all drawing related tasks, like creating UBOs and selecting the right PSO.
		*
		*/

	class VEEntity : public VESceneObject
	{
		friend VESceneManager;

	public:
		///The entity type determines what kind of entity this is
		enum veEntityType
		{
			VE_ENTITY_TYPE_NORMAL, ///<Normal object to be drawn
			VE_ENTITY_TYPE_SKYPLANE, ///<A plane for sky boxes
			VE_ENTITY_TYPE_TERRAIN_HEIGHTMAP ///<A heightmap for terrain modelling
		};

		///Data that is updated for each object
		struct veUBOPerEntity_t
		{
			glm::mat4 model; ///<Object model matrix
			glm::mat4 modelTrans; ///<Transpose (for raytracing)
			glm::mat4 modelInvTrans; ///<Inverse transpose
			glm::vec4 color; ///<Uniform color if needed by shader
			glm::vec4 param; ///<Texture scaling and animation: 0,1..scale 2,3...offset
			glm::ivec4 iparam; ///<iparam[0] is the resource idx, iparam[1] shows if normal map exists
			glm::vec4 a; ///<paddding to ensure that struct has size 256
		};

	protected:
		veEntityType m_entityType = VE_ENTITY_TYPE_NORMAL; ///<Entity type
		glm::vec4 m_param = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f); ///<Free parameter, e.g. for texture animation
		uint32_t m_resourceIdx = 0; ///<Idx into subrenderer list of resources

		VEEntity(std::string name, veEntityType type, VEMesh *pMesh, VEMaterial *pMat, glm::mat4 transf);

		virtual ~VEEntity();

		virtual void updateUBO(glm::mat4 worldMatrix,
			uint32_t imageIndex); //update the UBO of this node using its current world matrix
		void updateAccelerationStructure();

	public:
		VEMesh *m_pMesh = nullptr; ///<Pointer to entity mesh
		VEMaterial *m_pMaterial = nullptr; ///<Pointer to entity material

		VESubrender *m_pSubrenderer = nullptr; ///<subrenderer this entity is registered with / replace with a set
		bool m_visible = false; ///<should it be drawn at all?
		bool m_castsShadow = true; ///<draw in the shadow pass?

		vh::vhAccelerationStructure m_AccelerationStructure;

		//-------------------------------------------------------------------------------------
		//Class and type

		///\returns the scene node type
		virtual veObjectType getObjectType()
		{
			return VE_OBJECT_TYPE_ENTITY;
		};

		///\returns the entity type
		virtual veEntityType getEntityType()
		{
			return m_entityType;
		};

		//-------------------------------------------------------------------------------------
		//UBO

		///\returns size of entity UBO
		virtual uint32_t getSizeUBO()
		{
			return sizeof(veUBOPerEntity_t);
		};

		void setParam(glm::vec4 param); //set the free parameter
		/**
			* \brief set the index into the subrenderer resource list
			* \param[in] idx The new index
			*/
		void setResourceIdx(uint32_t idx)
		{
			m_resourceIdx = idx;
		};

		///\returns the index into the list of resources for this entity, held by the subrenderer
		uint32_t getResourceIdx()
		{
			return m_resourceIdx;
		};
		//-------------------------------------------------------------------------------------
		//Bounding volume

		virtual void
			getBoundingSphere(glm::vec3 *center, float *radius); //return center and radius for a bounding sphere
	};

	//--------------------------------------------------------------------------------------------------
	//Cameras

	class VELight;

	/**
		*
		* \brief A camera that can be used to take photos of the scene.
		*
		* VECamera is derived from VESceneObject and can be put into the scene. It produces a projection matrix which
		* represents the camera frustum. The base class however should never be used. Use a derived class.
		*
		*/

	class VECamera : public VESceneObject
	{
	protected:
		virtual void updateLocalUBO(glm::mat4 worldMatrix); //update local UBO copy
		virtual void updateUBO(glm::mat4 worldMatrix,
			uint32_t imageIndex); //update the UBO of this node using its current world matrix

	public:
		///Camera type, can be projective or orthographic
		enum veCameraType
		{
			VE_CAMERA_TYPE_PROJECTIVE, ///<A projective camera
			VE_CAMERA_TYPE_ORTHO, ///<An orthographic camera
		};

		///Structure for sending camera information to a UBO
		struct veUBOPerCamera_t
		{
			glm::mat4 model; ///<Camera model (world) matrix, needed for camera world position
			glm::mat4 view; ///<Camera view matrix
			glm::mat4 proj; ///<Camera projection matrix
			glm::mat4 modelInverse; ///<Camera model (world) inverse matrix (for ray tracing)
			glm::mat4 viewInverse; ///<Camera view inverse matrix (for ray tracing)
			glm::mat4 projInverse; ///<Camera projection inverse matrix (for ray tracing)
			glm::vec4 param; ///<param[0]: near plane param[1]: far plane distances - 2 and 3 are shadow depth fractions
			glm::vec4 a, b, c, d, e, f, g; ///<paddding to ensure that struct has size multiple of 256
		};

		struct veUBOPerCamera_t m_ubo; ///<The UBO that is copied to the GPU
		float m_nearPlane = 0.1f; ///<The distance of the near plane to the camera origin
		float m_farPlane = 200.0f; ///<The distance of the far plane to the camera origin
		float m_nearPlaneFraction = 0.0f; ///<If this is a shadow cam: fraction of frustum covered by this cam, start
		float m_farPlaneFraction = 1.0f; ///<If this is a shadow cam: fraction of frustum covered by this cam, end

		//-------------------------------------------------------------------------------------
		//Type

		VECamera(std::string name, glm::mat4 transf = glm::mat4(1.0f));

		VECamera(std::string name,
			float nearPlane,
			float farPlane,
			float nearPlaneFraction = 0.0f,
			float farPlaneFraction = 1.0f,
			glm::mat4 transf = glm::mat4(1.0f));

		virtual ~VECamera() {};

		///\returns the scene node type
		virtual veObjectType getObjectType()
		{
			return VE_OBJECT_TYPE_CAMERA;
		};

		///\returns the camera type - pure virtual for the camera base class
		virtual veCameraType getCameraType() = 0;

		///Set the camera extent, this is a pure virtual function for the base class
		virtual void setExtent(VkExtent2D extent) = 0;

		//-------------------------------------------------------------------------------------
		//UBO

		///\returns size of camera UBO
		virtual uint32_t getSizeUBO()
		{
			return sizeof(veUBOPerCamera_t);
		};

		///\returns the projection matrix - pure virtual for the camera base class
		virtual glm::mat4 getProjectionMatrix() = 0;

		///\return the projection matrix - pure virtual for the camera base class
		virtual glm::mat4 getProjectionMatrix(float width, float height) = 0;

		//-------------------------------------------------------------------------------------
		//Bounding volumes

		virtual void
			getBoundingSphere(glm::vec3 *center, float *radius); //return center and radius for a bounding sphere
			///\returns list of frustum points in world space - pure virtual for the camera base class
		virtual void getFrustumPoints(std::vector<glm::vec4> &points, float z0 = 0.0f, float z1 = 1.0f) = 0;
	};

	class VEPointLight;

	class VESpotLight;

	/**
		*
		* \brief A projective camera that can be used to take photos of the scene.
		*
		* VECameraProjective is derived from VECamera and can be put into the scene.
		* It produces a projection matrix which representing the camera frustum.
		* This class assumes a projective mapping with a camera eye point.
		*
		*/
	class VECameraProjective : public VECamera
	{
		friend VESceneManager;
		friend VEPointLight;
		friend VESpotLight;

	protected:
		VECameraProjective(std::string name, glm::mat4 transf = glm::mat4(1.0f));

		VECameraProjective(std::string name,
			float nearPlane,
			float farPlane,
			float aspectRatio,
			float fov,
			float nearPlaneFraction = 0.0f,
			float farPlaneFraction = 1.0f,
			glm::mat4 transf = glm::mat4(1.0f));

		///Destructor projective camera
		virtual ~VECameraProjective() {};

	public:
		float m_aspectRatio = 16.0f / 9.0f; ///<Ratio between width and height of camera (and window).
		float m_fov = 45.0f; ///<Vertical field of view in degrees

		//-------------------------------------------------------------------------------------
		//Type

		///\returns the camera type
		virtual veCameraType getCameraType()
		{
			return VE_CAMERA_TYPE_PROJECTIVE;
		};

		///Set the new camera extent to the current view port
		virtual void setExtent(VkExtent2D e)
		{
			m_aspectRatio = (float)e.width / (float)e.height;
		};

		//-------------------------------------------------------------------------------------
		//UBO

		virtual glm::mat4 getProjectionMatrix(); //Return the projection matrix
		virtual glm::mat4 getProjectionMatrix(float width, float height); //Return the projection matrix

		//-------------------------------------------------------------------------------------
		//Bounding volume

		virtual void getFrustumPoints(std::vector<glm::vec4> &points, float t1 = 0.0f,
			float t2 = 1.0f); //return list of frustum points in world space
	};

	class VEDirectionalLight;

	/**
		*
		* \brief Am ortho camera that can be used to take photos of the scene.
		*
		* VECameraOrtho is derived from VECamera and can be put into the scene. It produces a projection matrix which
		* representing the camera frustum. This class assumes a orthographic mapping.
		* The frustum is a box and x,z are not influenced by the depth value.
		*
		*/
	class VECameraOrtho : public VECamera
	{
		friend VESceneManager;
		friend VEDirectionalLight;

	protected:
		VECameraOrtho(std::string name, glm::mat4 transf = glm::mat4(1.0f));

		VECameraOrtho(std::string name,
			float nearPlane,
			float farPlane,
			float width,
			float height,
			float nearPlaneFraction = 0.0f,
			float farPlaneFraction = 1.0f,
			glm::mat4 transf = glm::mat4(1.0f));

		virtual ~VECameraOrtho() {};

	public:
		float m_width = 1.0f / 20.0f; ///<Camera width
		float m_height = 1.0f / 20.0f; ///<Camera height

		//-------------------------------------------------------------------------------------
		//Type

		///\returns the camera type
		virtual veCameraType getCameraType()
		{
			return VE_CAMERA_TYPE_ORTHO;
		};

		///Set the new camera extent to the current view port
		virtual void setExtent(VkExtent2D e)
		{
			m_width = (float)e.width;
			m_height = (float)e.height;
		};

		//-------------------------------------------------------------------------------------
		//UBO

		virtual glm::mat4 getProjectionMatrix(); //Return the projection matrix
		virtual glm::mat4 getProjectionMatrix(float width, float height); //Return the projection matrix

		//-------------------------------------------------------------------------------------
		//Bounding volume

		virtual void getFrustumPoints(std::vector<glm::vec4> &points, float t1 = 0.0f,
			float t2 = 1.0f); //return list of frustum points in world space
	};

	//--------------------------------------------------------------------------------------------------
	//Lights

	/**
		*
		* \brief A VELight has a color and can be used to light a scene.
		*
		* VELight is derived from VESceneObject and can be put into the scene.
		* It shines light on objects and is sent to the fragment shader to produce the final coloring of a pixel.
		*
		*/

	class VELight : public VESceneObject
	{
		friend VESceneManager;

	public:
		///A light can have one of these types
		enum veLightType
		{
			VE_LIGHT_TYPE_DIRECTIONAL = 0, ///<Directional light
			VE_LIGHT_TYPE_POINT = 1, ///<Point light
			VE_LIGHT_TYPE_SPOT = 2, ///<Spot light
			VE_LIGHT_TYPE_AMBIENT = 3 ///<Ambient light, lighting is constant in the scene
		};

		///Light data to be copied into a UBO
		struct veUBOPerLight_t
		{
			glm::ivec4 type; ///<Light type information
			glm::mat4 model; ///<Position and orientation of the light
			glm::vec4 col_ambient; ///<Ambient color
			glm::vec4 col_diffuse; ///<Diffuse color
			glm::vec4 col_specular; ///<Specular color
			glm::vec4 param; ///<Light parameters
			glm::vec4 a, b, c, d, e, f, g; ///<paddding to ensure that struct has size multiple of 256
			VECamera::veUBOPerCamera_t shadowCameras[6]; ///<Up to 6 different shadows, each having its own camera and shadow map
		};

	protected:
		VELight(std::string name, glm::mat4 transf = glm::mat4(1.0f));

		virtual ~VELight();

		virtual void updateLocalUBO(glm::mat4 worldMatrix); //update local UBO
		virtual void updateUBO(glm::mat4 worldMatrix,
			uint32_t imageIndex); //update the UBO of this node using its current world matrix

	public:
		struct veUBOPerLight_t m_ubo; ///<The UBO that is copied to the GPU
		std::vector<VECamera *> m_shadowCameras; ///<Up to 6 shadow cameras for this light
		bool m_switchedOn = true; ///<If false, then the colors are all zero

		glm::vec4 m_col_ambient = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); ///<Ambient color
		glm::vec4 m_col_diffuse = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); ///<Diffuse color
		glm::vec4 m_col_specular = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); ///<Specular color
		glm::vec4 m_param = glm::vec4(100.0f, 1.0f, 1.0f, 1.0f); ///<Light parameters: 0...reach

		//-------------------------------------------------------------------------------------
		//Type

		///Update all shadow cameras of this light - pure virtual for the light base class
		virtual void updateShadowCameras(VECamera *pCamera, uint32_t imageIndex) = 0;

		///\returns the scene node type
		virtual veObjectType getObjectType()
		{
			return VE_OBJECT_TYPE_LIGHT;
		};

		///\returns the light type - pure virtual for the light base class
		virtual veLightType getLightType() = 0;

		//-------------------------------------------------------------------------------------
		//UBO

		///\returns size of light UBO
		virtual uint32_t getSizeUBO()
		{
			return sizeof(veUBOPerLight_t);
		};
	};

	/**
		*
		* \brief Directional light class.
		*
		* This class represents a directional light. It is derived from the VELight class.
		* A directional light is used to model su or moon light. It comes from a certain fixed direction,
		* which is represented by its z-axis. The position of this light is not used.
		* A directional light uses four ortho cameras as shadow cams. Each of them covers a segment
		* of the light camera.
		*
		*/

	class VEDirectionalLight : public VELight
	{
		friend VESceneManager;

	protected:
		VEDirectionalLight(std::string name, glm::mat4 transf = glm::mat4(1.0f));

		///Destructor of the directional light
		virtual ~VEDirectionalLight() {};

	public:
		virtual void updateShadowCameras(VECamera *pCamera, uint32_t imageIndex);

		///\returns the light type
		virtual veLightType getLightType()
		{
			return VE_LIGHT_TYPE_DIRECTIONAL;
		};
	};

	/**
		*
		* \brief Point light class
		*
		* This class represents a point light. It is derived from the VELight class.
		* A point light emanates from its position, which is the only parameter used.
		* Its orientation does not influence the scene.
		* A point light uses 6 projective shadow cameras, filming in 6 different directions.
		*
		*/

	class VEPointLight : public VELight
	{
		friend VESceneManager;

	protected:
		VEPointLight(std::string name, glm::mat4 transf = glm::mat4(1.0f));

		///Destructor of the point light
		virtual ~VEPointLight() {};

	public:
		virtual void updateShadowCameras(VECamera *pCamera, uint32_t imageIndex);

		///\returns the light type
		virtual veLightType getLightType()
		{
			return VE_LIGHT_TYPE_POINT;
		};
	};

	/**
		*
		* \brief Spot light class.
		*
		* This class represents a spot light. It is derived from the VELight class.
		* A spot light uses one projective shadow camera.
		*
		*/

	class VESpotLight : public VELight
	{
		friend VESceneManager;

	protected:
		VESpotLight(std::string name, glm::mat4 transf = glm::mat4(1.0f));

		///Destructor of the spot light
		virtual ~VESpotLight() {};

	public:
		virtual void updateShadowCameras(VECamera *pCamera, uint32_t imageIndex);

		///\returns the light type
		virtual veLightType getLightType()
		{
			return VE_LIGHT_TYPE_SPOT;
		};
	};

	/**
		*
		* \brief Ambient light class.
		*
		* This class represents an ambient light. It is derived from the VELight class.
		* An ambient light is constant throughout the scene
		*
		*/
	class VEAmbientLight : public VELight
	{
		friend VESceneManager;

	protected:
		///Constructor of Ambient Light class
		VEAmbientLight(std::string name)
			: VELight(name) {};

		///Destructor of the spot light
		virtual ~VEAmbientLight() {};

	public:
		virtual void updateShadowCameras(VECamera *pCamera, uint32_t imageIndex) {};

		///\returns the light type
		virtual veLightType getLightType()
		{
			return VE_LIGHT_TYPE_AMBIENT;
		};
	};

} // namespace ve

#endif
