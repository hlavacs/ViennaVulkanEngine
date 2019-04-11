/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#pragma once

namespace ve {

	//----------------------------------------------------------------------------------------------
	//Scene node


	/**
	*
	* \brief Represents any object that can be put into a scene.
	*
	* VESceneNode represents any object that can be used by the scene manager to be put into the scene.
	* This includes objects, cameras, lights, sky boxes or terrains.
	* Scene nodes can have a parent. If so the local transform is relative to the parent transform. This
	* relation is stored in the parent and children pointers.
	*/

	class VESceneNode : public VENamedClass {

	public:
		///Object type, can be node, entity for drawing, camera or light
		enum veNodeType {
			VE_OBJECT_TYPE_SCENENODE,	///<Instance of the base class, acts as scene node, cannot be drawn
			VE_OBJECT_TYPE_ENTITY,		///<Normal object to be drawn
			VE_OBJECT_TYPE_CAMERA,		///<A projective camera, cannot be drawn
			VE_OBJECT_TYPE_LIGHT		///<A light, cannot be drawn
		};

	protected:
		glm::mat4		m_transform = glm::mat4(1.0);		///<Transform from local to parent space, the engine uses Y-UP, Left-handed

	public:
		VESceneNode *				m_parent = nullptr;		///<Pointer to entity parent
		std::vector<VESceneNode *>	m_children;				///<List of entity children

		//-------------------------------------------------------------------------------------
		//Class and type

		VESceneNode(std::string name, glm::mat4 transf = glm::mat4(1.0f), VESceneNode *parent = nullptr);
		virtual ~VESceneNode();
		///\returns the scene node type
		virtual veNodeType	getNodeType() { return VE_OBJECT_TYPE_SCENENODE; };

		//-------------------------------------------------------------------------------------
		//transforms

		void		setTransform(glm::mat4 trans);		//Overwrite the transform and copy it to the UBO
		glm::mat4	getTransform();						//Return local transform
		void		setPosition(glm::vec3 pos);			//Set the position of the entity
		glm::vec3	getPosition();						//Return the current position in parent space
		glm::vec3	getXAxis();							//Return local x-axis in parent space
		glm::vec3	getYAxis();							//Return local y-axis in parent space
		glm::vec3	getZAxis();							//Return local z-axis in parent space
		void		multiplyTransform(glm::mat4 trans); //Multiply the transform, e.g. translate, scale, rotate 
		glm::mat4	getWorldTransform();				//Compute the world matrix
		void		lookAt(glm::vec3 eye, glm::vec3 point, glm::vec3 up);

		//--------------------------------------------------------------------------------------
		//UBO updates

		virtual void update( uint32_t imageIndex );									//Copy the world matrix to the UBO
		virtual void update(glm::mat4 parentWorldMatrix, uint32_t imageIndex );		//Copy the world matrix using the parent's world matrix
		virtual void updateChildren(glm::mat4 worldMatrix, uint32_t imageIndex);	//Update all children

		///Meant for subclasses to add data to the UBO, so this function does nothing in base class
		virtual void updateUBO(glm::mat4 worldMatrix, uint32_t imageIndex) {};		//update the UBO of this node using its current world matrix

		//--------------------------------------------------------------------------------------
		//manage tree

		virtual void addChild(VESceneNode *);		//Add a new child
		virtual void removeChild(VESceneNode *);	//Remove a child, dont destroy it

		//-------------------------------------------------------------------------------------
		//Bounding volumes

		virtual void getBoundingSphere( glm::vec3 *center, float *radius );		//return center and radius for a bounding sphere
		virtual void getOBB(std::vector<glm::vec4> &pointsW, float t1, float t2, glm::vec3 &center, float &width, float &height, float &depth);	//return min and max along the axes
	};


	//--------------------------------------------------------------------------------------------------
	//Scene object


	/**
	*
	* \brief Represents any object that has its own UBO.
	*
	* A scene object has its own UBO describing its transform and current state.
	* 
	*/

	class VESceneObject : public VESceneNode {

	protected:
		void updateUBO( void *pUBO, uint32_t sizeUBO, uint32_t imageIndex );						//Helper function to call VMA functions

	public:

		std::vector<VkBuffer>			m_uniformBuffers;				///<One UBO for each framebuffer frame
		std::vector<VmaAllocation>		m_uniformBuffersAllocation;		///<VMA information for the UBOs
		std::vector<VkDescriptorSet>	m_descriptorSetsUBO;			///<Descriptor sets for UBO

		VESceneObject(std::string name, glm::mat4 transf = glm::mat4(1.0f), VESceneNode *parent = nullptr, uint32_t sizeUBO = 0);
		virtual ~VESceneObject();
	};


	//--------------------------------------------------------------------------------------------------
	//Entity


	class VESubrender;

	/**
	*
	* \brief Represents any object that can be drawn.
	*
	* VEEntity represents any object that can be drawn. It also contains a Vulkan UBO for storing
	* position and orientation. Entities have exactly one mesh and one material.
	* Entities are associated with exactly one VESubrender instance. This instance is responsible for managing
	* all drawing related tasks, like creating UBOs and selecting the right PSO.
	*
	*/

	class VEEntity : public VESceneObject {

	public:
		///The entity type determines what kind of entity this is
		enum veEntityType {	
			VE_ENTITY_TYPE_NORMAL,				///<Normal object to be drawn
			VE_ENTITY_TYPE_CUBEMAP,				///<A cubemap for sky boxes
			VE_ENTITY_TYPE_CUBEMAP2,			///<A cubemap for sky boxes, but simulated
			VE_ENTITY_TYPE_SKYPLANE,			///<A plane for sky boxes
			VE_ENTITY_TYPE_TERRAIN_HEIGHTMAP	///<A heightmap for terrain modelling
		};

		///Data that is updated for each object
		struct veUBOPerObject_t {
			glm::mat4 model;			///<Object model matrix
			glm::mat4 modelInvTrans;	///<Inverse transpose
			glm::vec4 color;			///<Uniform color if needed by shader
			glm::vec4 texParam;			///<Texture scaling and animation
		};

	protected:
		veEntityType				m_entityType = VE_ENTITY_TYPE_NORMAL;			///<Entity type
		glm::vec4					m_texParam = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);	///<Texture animation

	public:
		VEMesh *					m_pMesh = nullptr;					///<Pointer to entity mesh
		VEMaterial *				m_pMaterial = nullptr;				///<Pointer to entity material

		VESubrender *				m_pSubrenderer = nullptr;			///<subrenderer this entity is registered with / replace with a set
		bool						m_drawEntity = false;				///<should it be drawn at all?
		bool						m_castsShadow = true;				///<draw in the shadow pass?

		std::vector<VkDescriptorSet>	m_descriptorSetsResources;		///<Per subrenderer descriptor sets for other resources


		//-------------------------------------------------------------------------------------
		//Class and type

		VEEntity(	std::string name, veEntityType type,
					VEMesh *pMesh, VEMaterial *pMat,
					glm::mat4 transf, VESceneNode *parent);
		virtual ~VEEntity();

		///\returns the scene node type
		virtual veNodeType	getNodeType() { return VE_OBJECT_TYPE_ENTITY; };

		///\returns the entity type
		virtual veEntityType getEntityType() { return m_entityType; };

		//-------------------------------------------------------------------------------------
		//UBO

		virtual void updateUBO( glm::mat4 worldMatrix, uint32_t imageIndex );	//update the UBO of this node using its current world matrix
		void		 setTexParam(glm::vec4 param);

		//-------------------------------------------------------------------------------------
		//Bounding volume

		virtual void getBoundingSphere( glm::vec3 *center, float *radius );		//return center and radius for a bounding sphere
	};


	//--------------------------------------------------------------------------------------------------
	//Cameras


	class VELight;

	/**
	*
	* \brief A camera that can be used to take photos of the scene.
	*
	* VECamera is derived from VEEntity and can be put into the scene. It produces a projection matrix which 
	* represents the camera frustum. The base class however should never be used. Use a derived class.
	*
	*/
	class VECamera : public VESceneObject {

	public:
		///Camera type, can be projective or orthographic
		enum veCameraType {
			VE_CAMERA_TYPE_PROJECTIVE,	///<A projective camera
			VE_CAMERA_TYPE_ORTHO,		///<An orthographic camera
		};

		///Structure for sending camera information to a UBO
		struct veUBOPerCamera_t {
			glm::mat4 model;		///<Camera model (world) matrix, needed for camera world position
			glm::mat4 view;			///<Camera view matrix
			glm::mat4 proj;			///<Camera projection matrix
			glm::vec4 param;		///<param[0]: near plane param[1]: far plane distances - 2 and 3 are shadow depth fractions
		};


		///Structure for sending information about a shadow to a UBO
		//struct veShadowData_t {
		//	glm::mat4 shadowView;		///<View matrix of the shadow cam
		//	glm::mat4 shadowProj;		///<Projection matrix of the shadow cam
		//	glm::vec4 limits;			///<limits[0] nad [1]: fraction of (far-near) distance where frustum begins/ends
		//};

		float m_nearPlane = 1.0f;			///<The distance of the near plane to the camera origin
		float m_farPlane = 200.0f;			///<The distance of the far plane to the camera origin
		float m_nearPlaneFraction = 0.0f;	///<If this is a shadow cam: fraction of frustum covered by this cam, start
		float m_farPlaneFraction = 1.0f;	///<If this is a shadow cam: fraction of frustum covered by this cam, end

		//-------------------------------------------------------------------------------------
		//Class and type

		VECamera(	std::string name,
					glm::mat4 transf = glm::mat4(1.0f), 
					VESceneNode *parent=nullptr );

		VECamera(	std::string name, 
					float nearPlane, float farPlane, 
					float nearPlaneFraction = 0.0f, float farPlaneFraction = 1.0f,
					glm::mat4 transf = glm::mat4(1.0f),
					VESceneNode *parent = nullptr );

		virtual ~VECamera() {};

		///\returns the scene node type
		virtual veNodeType	getNodeType() { return VE_OBJECT_TYPE_CAMERA; };

		///\returns the camera type - pure virtual for the camera base class
		virtual veCameraType getCameraType()=0;

		//-------------------------------------------------------------------------------------
		//UBO
		//void fillCameraStructure(veCameraData_t *pCamera);
		//void fillShadowStructure(veShadowData_t *pCamera);
		//virtual void updateUBO(glm::mat4 parentWorldMatrix) {};

		virtual void updateUBO(glm::mat4 worldMatrix, uint32_t imageIndex);		//update the UBO of this node using its current world matrix

		///\returns the projection matrix - pure virtual for the camera base class
		virtual glm::mat4 getProjectionMatrix()=0;

		///\return the projection matrix - pure virtual for the camera base class
		virtual glm::mat4 getProjectionMatrix( float width, float height )=0;

		//-------------------------------------------------------------------------------------
		//Bounding volumes

		virtual void getBoundingSphere(glm::vec3 *center, float *radius);		//return center and radius for a bounding sphere
		///\returns list of frustum points in world space - pure virtual for the camera base class
		virtual void getFrustumPoints(std::vector<glm::vec4> &points, float z0 = 0.0f, float z1 = 1.0f)=0;

		//-------------------------------------------------------------------------------------
		//shadow cams

		virtual VECamera *createShadowCamera(VELight *light, float z0 = 0.0f, float z1 = 1.0f);	//Depending on light type, create shadow camera
		virtual VECamera *createShadowCameraOrtho(VELight *light, float z0 = 0.0f, float z1 = 1.0f);				//Create an ortho shadow cam for directional light
		virtual VECamera *createShadowCameraProjective(VELight *light, float z0 = 0.0f, float z1 = 1.0f);			//Create a projective shadow cam for spot light
	};



	/**
	*
	* \brief A projective camera that can be used to take photos of the scene.
	*
	* VECameraProjective is derived from VECamera and can be put into the scene. It produces a projection matrix which
	* representing the camera frustum. This class assumes a projective mapping with a camera eye point.
	*
	*/
	class VECameraProjective : public VECamera {
	public:
		float m_aspectRatio = 16.0f / 9.0f;		///<Ratio between width and height of camera (and window).
		float m_fov = 45.0f;					///<Vertical field of view

		//-------------------------------------------------------------------------------------
		//Class and type

		VECameraProjective(std::string name, glm::mat4 transf = glm::mat4(1.0f), VESceneNode *parent = nullptr);
		VECameraProjective(	std::string name, 
							float nearPlane, float farPlane, 
							float aspectRatio, float fov,
							float nearPlaneFraction = 0.0f, float farPlaneFraction = 1.0f,
							glm::mat4 transf = glm::mat4(1.0f), VESceneNode *parent = nullptr );

		virtual ~VECameraProjective() {};

		///\returns the camera type
		virtual veCameraType getCameraType() { return VE_CAMERA_TYPE_PROJECTIVE; };

		//-------------------------------------------------------------------------------------
		//UBO

		virtual glm::mat4 getProjectionMatrix();								//Return the projection matrix
		virtual glm::mat4 getProjectionMatrix(float width, float height);		//Return the projection matrix

		//-------------------------------------------------------------------------------------
		//Bounding volume

		virtual void getFrustumPoints(std::vector<glm::vec4> &points, float t1 = 0.0f, float t2 = 1.0f);	//return list of frustum points in world space
	};


	/**
	*
	* \brief Am ortho camera that can be used to take photos of the scene.
	*
	* VECameraOrtho is derived from VECamera and can be put into the scene. It produces a projection matrix which
	* representing the camera frustum. This class assumes a orthographic mapping. 
	* The frustum is a box and x,z are not influenced by the depth value.
	*
	*/
	class VECameraOrtho : public VECamera {
	public:
		float m_width = 1.0f/20.0f;					///<Camera width
		float m_height = 1.0f/20.0f;				///<Camera height

		//-------------------------------------------------------------------------------------
		//Class and type

		VECameraOrtho(	std::string name, glm::mat4 transf = glm::mat4(1.0f), VESceneNode *parent = nullptr );
		VECameraOrtho(	std::string name, 
						float nearPlane, float farPlane, 
						float width, float height, 
						float nearPlaneFraction = 0.0f, float farPlaneFraction = 1.0f,
						glm::mat4 transf = glm::mat4(1.0f), VESceneNode *parent = nullptr);
		virtual ~VECameraOrtho() {};

		///\returns the camera type
		virtual veCameraType getCameraType() { return VE_CAMERA_TYPE_ORTHO; };

		//-------------------------------------------------------------------------------------
		//UBO

		virtual glm::mat4 getProjectionMatrix();								//Return the projection matrix
		virtual glm::mat4 getProjectionMatrix(float width, float height);		//Return the projection matrix

		//-------------------------------------------------------------------------------------
		//Bounding volume

		virtual void getFrustumPoints(std::vector<glm::vec4> &points, float t1 = 0.0f, float t2 = 1.0f);	//return list of frustum points in world space
	};


	//--------------------------------------------------------------------------------------------------
	//Lights

	/**
	*
	* \brief A VELight has a color and can be used to light a scene.
	*
	* VELight is derived from VEEntity and can be put into the scene. It shines light on objects and is sent to the
	* fragment shader to produce the final coloring of a pixel.
	*
	*/

	class VELight : public VESceneObject {

	public:
		///A light can have one of these types
		enum veLightType {
			VE_LIGHT_TYPE_DIRECTIONAL=0,	///<Directional light
			VE_LIGHT_TYPE_POINT=1,			///<Point light
			VE_LIGHT_TYPE_SPOT=2			///<Spot light
		};

		///Structure for sending information about a shadow to a UBO
		//struct veShadowData_t {
		//	VECamera::veCameraData_t	shadowCameras[6];
		//	glm::vec4					limits;				///<limits[0] and [1]: fraction of (far-near) distance where frustum begins/ends
		//};

		///Light data to be copied into a UBO
		struct veUBOPerLight_t {
			glm::ivec4	type;								///<Light type information
			glm::mat4	model;								///<Position and orientation of the light
			glm::vec4	col_ambient;						///<Ambient color
			glm::vec4	col_diffuse;						///<Diffuse color
			glm::vec4	col_specular;						///<Specular color
			glm::vec4	param;								///<Light parameters
			VECamera::veUBOPerCamera_t shadowCameras[6];	///<Up to 6 different shadows, each having its own camera and shadow map
		};

	public:
		std::vector<VECamera*>	m_shadowCameras;			///<Up to 6 shadow cameras for this light

		glm::vec4 m_col_ambient  = 0.5f * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);	///<Ambient color
		glm::vec4 m_col_diffuse  = 0.8f * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);	///<Diffuse color
		glm::vec4 m_col_specular = 0.9f * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);	///<Specular color
		glm::vec4 m_param		 = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);			///<1-2: attenuation, 3: Ns

		//-------------------------------------------------------------------------------------
		//Class and type

		VELight(std::string name, glm::mat4 transf = glm::mat4(1.0f), VESceneNode *parent = nullptr);
		virtual ~VELight();

		///\returns the scene node type
		virtual veNodeType	getNodeType() { return VE_OBJECT_TYPE_LIGHT; };

		///\returns the light type - pure virtual for the light base class
		virtual veLightType	getLightType()=0;

		//-------------------------------------------------------------------------------------
		//UBO

		//void fillLightStructure( veLightData_t *pLight);
		virtual void updateUBO(glm::mat4 worldMatrix, uint32_t imageIndex);		//update the UBO of this node using its current world matrix
	};



	class VEDirectionalLight : public VELight {
	public:

		//-------------------------------------------------------------------------------------
		//Class and type

		VEDirectionalLight(	std::string name, glm::mat4 transf = glm::mat4(1.0f), VESceneNode *parent = nullptr);
		virtual ~VEDirectionalLight() {};

		///\returns the light type
		virtual veLightType getLightType() { return VE_LIGHT_TYPE_DIRECTIONAL; };
	};


	class VEPointLight : public VELight {
	public:

		//-------------------------------------------------------------------------------------
		//Class and type

		VEPointLight(std::string name, glm::mat4 transf = glm::mat4(1.0f), VESceneNode *parent = nullptr);
		virtual ~VEPointLight() {};

		///\returns the light type
		virtual veLightType getLightType() { return VE_LIGHT_TYPE_POINT; };
	};


	class VESpotLight : public VELight {
	public:

		//-------------------------------------------------------------------------------------
		//Class and type

		VESpotLight(std::string name, glm::mat4 transf = glm::mat4(1.0f), VESceneNode *parent = nullptr);
		virtual ~VESpotLight() {};

		///\returns the light type
		virtual veLightType getLightType() { return VE_LIGHT_TYPE_SPOT; };
	};


}


