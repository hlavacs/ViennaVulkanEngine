/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#pragma once

namespace ve {

	/**
	*
	* \brief Store texture data
	*
	* Store an image and a sampler as shader resource.
	*/

	struct VETexture : VENamedClass {
		VkImage			m_image = VK_NULL_HANDLE;				///<image handle
		VkImageView		m_imageView = VK_NULL_HANDLE;			///<image view
		VmaAllocation	m_deviceAllocation = nullptr;			///<VMA allocation info
		VkSampler		m_sampler = VK_NULL_HANDLE;				///<image sampler

		VETexture(std::string name, gli::texture_cube &texCube);
		VETexture(std::string name, std::string &basedir, std::vector<std::string> texNames, VkImageCreateFlags flags=0, VkImageViewType viewtype = VK_IMAGE_VIEW_TYPE_2D);
		~VETexture();
	};

	/**
	*
	* \brief Store material data
	*
	* VEMaterial stores material data, including several textures, and color data.
	*/

	class VEMaterial : public VENamedClass {
	public:
		aiShadingMode shading = aiShadingMode_Phong;	///<Default shading model

		VETexture *mapDiffuse = nullptr;				///<Diiffuse texture
		VETexture *mapBump = nullptr;					///<Bump map
		VETexture *mapNormal = nullptr;					///<Normal map
		VETexture *mapHeight = nullptr;					///<Height map
		glm::vec4 color = glm::vec4( 0.5f, 0.5f, 0.5f, 1.0f);	///<General color of the entity

		///Constructor
		VEMaterial(std::string name) : VENamedClass(name), mapDiffuse(nullptr), mapBump(nullptr), mapNormal(nullptr), mapHeight(nullptr), color( glm::vec4(0.5f, 0.5f, 0.5f, 1.0f) ) {};
		~VEMaterial();
	};

	/**
	*
	* \brief Store a mesh in a Vulkan vertex and index buffer
	*
	* VEMesh stores a mesh in a Vulkan vertex and index buffer. For both buffers, also the VMA
	* allocation information is stored.
	*
	*/

	class VEMesh : public VENamedClass {
	public:
		uint32_t		m_vertexCount = 0;					///<Number of vertices in the vertex buffer
		uint32_t		m_indexCount = 0;					///<Number of indices in the index buffer
		VkBuffer		m_vertexBuffer = VK_NULL_HANDLE;	///<Vulkan vertex buffer handle
		VmaAllocation	m_vertexBufferAllocation = nullptr;	///<VMA allocation info
		VkBuffer		m_indexBuffer = VK_NULL_HANDLE;		///<Vulkan index buffer handle
		VmaAllocation	m_indexBufferAllocation = nullptr;	///<VMA allocation info
		glm::vec3		m_boundingSphereCenter=glm::vec3(0.0f, 0.0f, 0.0f );	///<center of bounding sphere in local space
		float			m_boundingSphereRadius = 1.0;		///<Radius of bounding sphere in local space

		VEMesh(std::string name, const aiMesh *paiMesh);
		~VEMesh();
	};


	class VESubrender;

	/**
	*
	* \brief Represents any object that can be put into a scene.
	*
	* VEEntity represents any object that can be used by the scene manager to be put into the scene.
	* This includes objects, cameras, lights, sky boxes or terrains. It also contains a Vulkan UBO for storing
	* position and orientation. Entities that should be drawn have exactly one mesh and one material.
	* Entities can have a parent. If so the local transform is relative to the parent transform. This
	* is stored in the parent and children pointers.
	* Entities are associated to exactly one VESubrender instance. This instance is responsible for managing
	* all drawing related tasks, like creating UBOs and selecting the right PSO.
	*/

	class VEEntity : public VENamedClass {
	protected:
		glm::mat4x4	m_transform = glm::mat4(1.0);	///<Transform from local to parent space, the engine uses Y-UP, Left-handed

	public:
		///The entity type determines what kind of entity this is
		enum veEntityType {	
			VE_ENTITY_TYPE_OBJECT,				///<Normal object to be drawn
			VE_ENTITY_TYPE_CUBEMAP,				///<A cubemap for sky boxes
			VE_ENTITY_TYPE_TERRAIN_HEIGHTMAP,	///<A heightmap for terrain modelling
			VE_ENTITY_TYPE_CAMERA_PROJECTIVE,	///<A projective camera, cannot be drawn
			VE_ENTITY_TYPE_CAMERA_ORTHO,		///<An orthographic camera, cannot be drawn
			VE_ENTITY_TYPE_LIGHT
		};
		
		uint32_t					m_entityType = VE_ENTITY_TYPE_OBJECT;	///<Entity type
		VEMesh *					m_pMesh = nullptr;						///<Pointer to entity mesh
		VEMaterial *				m_pMaterial = nullptr;					///<Pointer to entity material
		VEEntity *					m_pEntityParent = nullptr;				///<Pointer to entity parent
		std::vector<VEEntity *>		m_pEntityChildren;						///<List of entity children
		VESubrender *				m_pSubrenderer = nullptr;				///<subrenderer this entity is registered with / replace with a set
		bool						m_drawEntity = false;					///<should it be drawn at all?
		bool						m_castsShadow = false;					///<draw in the shadow pass?

		std::vector<VkBuffer>			m_uniformBuffers;			///<One UBO for each framebuffer frame
		std::vector<VmaAllocation>		m_uniformBuffersAllocation; ///<VMA information for the UBOs
		std::vector<VkDescriptorSet>	m_descriptorSets;			///<Per subrenderer descriptor sets

		VEEntity( std::string name );
		VEEntity(std::string name, veEntityType type, VEMesh *pMesh, VEMaterial *pMat, glm::mat4 transf, VEEntity *parent);
		virtual ~VEEntity();

		//--------------------------------------------------
		void		setTransform(glm::mat4x4 trans);	//Overwrite the transform and copy it to the UBO
		glm::mat4x4 getTransform();						//Return local transform
		void		setPosition(glm::vec3 pos);			//Set the position of the entity
		glm::vec3	getPosition();						//Return the current position in parent space
		glm::vec3	getXAxis();							//Return local x-axis in parent space
		glm::vec3	getYAxis();							//Return local y-axis in parent space
		glm::vec3	getZAxis();							//Return local z-axis in parent space
		void		multiplyTransform(glm::mat4x4 trans); //Multiply the transform, e.g. translate, scale, rotate 
		glm::mat4x4 getWorldTransform();				//Compute the world matrix
		void		lookAt(glm::vec3 eye, glm::vec3 point, glm::vec3 up);

		//--------------------------------------------------
		void		update();							//Copy the world matrix to the UBO
		void		update(glm::mat4x4 parentWorldMatrix);	//Copy the world matrix using the parent's world matrix
		void		addChild(VEEntity *);				//Add a new child
		void		removeChild(VEEntity *);			//Remove a child, dont destroy it
		void		updateChildren(glm::mat4x4 worldMatrix);	//Update all children
		virtual void getBoundingSphere( glm::vec3 *center, float *radius );		//return center and radius for a bounding sphere
	};

	class VELight;

	/**
	*
	* \brief A camera that can be used to take photos of the scene.
	*
	* VECamera is derived from VEEntity and can be put into the scene. It produces a projection matrix which 
	* represents the camera frustum. The base class however should never be used. Use a derived class.
	*
	*/
	class VECamera : public VEEntity {
	public:
		float m_nearPlane = 0.2f;	///<The distannce of the near plane to the camera origin
		float m_farPlane = 200.0f;	///<The distance of the far plane to the camera origin

		VECamera(std::string name);
		VECamera(std::string name, float nearPlane, float farPlane);
		virtual ~VECamera() {};
		virtual glm::mat4 getProjectionMatrix()=0;								///<Return the projection matrix
		virtual glm::mat4 getProjectionMatrix( float width, float height )=0;	///<Return the projection matrix
		virtual void getBoundingSphere(glm::vec3 *center, float *radius);		//return center and radius for a bounding sphere
		virtual void getFrustumPoints( std::vector<glm::vec4> &points )=0;		///<Return list of frustum points in local space
		virtual VECamera *createShadowCamera(VELight *light);					//Depending on light type, create shadow camera
		virtual VECamera *createShadowCameraOrtho(VELight *light);				//Create an ortho shadow cam for directional light
		virtual VECamera *createShadowCameraProjective(VELight *light);			//Create a projective shadow cam for spot light
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
		float m_fov = 40.0f;					///<Vertical field of view

		VECameraProjective(std::string name);
		VECameraProjective(std::string name, float nearPlane, float farPlane, float aspectRatio, float fov);
		virtual ~VECameraProjective() {};
		glm::mat4 getProjectionMatrix();										//Return the projection matrix
		virtual glm::mat4 getProjectionMatrix(float width, float height);		//Return the projection matrix
		virtual void getFrustumPoints(std::vector<glm::vec4> &points);			//return list of frustum points in local space
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

		VECameraOrtho(std::string name);
		VECameraOrtho(std::string name, float nearPlane, float farPlane, float width, float height );
		virtual ~VECameraOrtho() {};
		glm::mat4 getProjectionMatrix();										///<Return the projection matrix
		virtual glm::mat4 getProjectionMatrix(float width, float height);		///<Return the projection matrix
		virtual void getFrustumPoints(std::vector<glm::vec4> &points);			//return list of frustum points in local space
	};


	/**
	*
	* \brief A VELight has a color and can be used to light a scene.
	*
	* VELight is derived from VEEntity and can be put into the scene. It shines light on objects and is sent to the
	* fragment shader to produce the final coloring of a pixel.
	*
	*/

	class VELight : public VEEntity {

	public:

		///Light data
		struct veLight {
			glm::vec4	type;			///<Light type information
			glm::vec4	param;			///<Light parameters
			glm::vec4	col_ambient;	///<Ambient color
			glm::vec4	col_diffuse;	///<Diffuse color
			glm::vec4	col_specular;	///<Specular color
			glm::mat4	transform;		///<Position and orientation of the light
		};

		///A light can have one of these types
		enum veLightType {
			VE_LIGHT_TYPE_DIRECTIONAL=0,	///<Directional light
			VE_LIGHT_TYPE_POINT=1,			///<Point light
			VE_LIGHT_TYPE_SPOT=2			///<Spot light
		};

	protected:
		veLightType m_lightType;	///<Light type of this light

	public:
		glm::vec4 param = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);				///<1-2: attenuation, 3: Ns
		glm::vec4 col_ambient  = 0.5f * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);	///<Ambient color
		glm::vec4 col_diffuse  = 0.8f * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);	///<Diffuse color
		glm::vec4 col_specular = 0.9f * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);	///<Specular color

		VELight(std::string name, veLightType type );
		veLightType getLightType();		//return the light type

		void fillVhLightStructure( veLight *pLight);
	};


}


