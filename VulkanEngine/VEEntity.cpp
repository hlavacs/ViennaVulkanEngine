/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"



namespace ve {

	//---------------------------------------------------------------------
	//Mesh

	/**
	*
	* \brief VEMesh constructor from an Assimp aiMesh.
	*
	* Create a VEMesh from an Assmip aiMesh input.
	*
	* \param[in] name The name of the mesh.
	* \param[in] paiMesh Pointer to the Assimp aiMesh that is the source of this mesh.
	*
	*/

	VEMesh::VEMesh(std::string name, const aiMesh *paiMesh) : VENamedClass(name) {
		std::vector<vh::vhVertex>	vertices;	//vertex array
		std::vector<uint32_t>		indices;	//index array

		//copy the mesh vertex data
		m_vertexCount = paiMesh->mNumVertices;
		float maxsq = 0;
		for (uint32_t i = 0; i < paiMesh->mNumVertices; i++) {
			vh::vhVertex vertex;
			vertex.pos.x = paiMesh->mVertices[i].x;								//copy 3D position in local space
			vertex.pos.y = paiMesh->mVertices[i].y; 
			vertex.pos.z = paiMesh->mVertices[i].z;
			
			maxsq = std::max( std::max(vertex.pos.x*vertex.pos.x, vertex.pos.y*vertex.pos.y), vertex.pos.z*vertex.pos.z);

			if (paiMesh->HasNormals()) {										//copy normals
				vertex.normal.x = paiMesh->mNormals[i].x;
				vertex.normal.y = paiMesh->mNormals[i].y;
				vertex.normal.z = paiMesh->mNormals[i].z;
			}
			
			if (paiMesh->HasTangentsAndBitangents() && paiMesh->mTangents) {	//copy tangents
				vertex.tangent.x = paiMesh->mTangents[i].x;
				vertex.tangent.y = paiMesh->mTangents[i].y;
				vertex.tangent.z = paiMesh->mTangents[i].z;
			}

			if (paiMesh->HasTextureCoords(0)) {									//copy texture coordinates
				vertex.texCoord.x = paiMesh->mTextureCoords[0][i].x;
				vertex.texCoord.y = paiMesh->mTextureCoords[0][i].y;
			}

			vertices.push_back(vertex);
		}
		m_boundingSphereRadius = sqrt(maxsq);

		//got through the aiMesh faces, and copy the indices
		m_indexCount = 0;
		for (uint32_t i = 0; i < paiMesh->mNumFaces; i++) {
			for (uint32_t j = 0; j < paiMesh->mFaces[i].mNumIndices; j++) {
				indices.push_back( paiMesh->mFaces[i].mIndices[j] );
				m_indexCount++;
			}
		}

		//create the vertex buffer
		vh::vhBufCreateVertexBuffer(getRendererPointer()->getDevice(), getRendererPointer()->getVmaAllocator(),
			getRendererPointer()->getGraphicsQueue(), getRendererPointer()->getCommandPool(),
			vertices, &m_vertexBuffer, &m_vertexBufferAllocation);

		//create the index buffer
		vh::vhBufCreateIndexBuffer(getRendererPointer()->getDevice(), getRendererPointer()->getVmaAllocator(),
			getRendererPointer()->getGraphicsQueue(), getRendererPointer()->getCommandPool(),
			indices, &m_indexBuffer, &m_indexBufferAllocation);
	}

	/**
	* \brief Destroy the vertex and index buffers
	*/
	VEMesh::~VEMesh() {
		vmaDestroyBuffer(getRendererPointer()->getVmaAllocator(), m_indexBuffer, m_indexBufferAllocation);
		vmaDestroyBuffer(getRendererPointer()->getVmaAllocator(), m_vertexBuffer, m_vertexBufferAllocation);
	}


	//---------------------------------------------------------------------
	//Material

	/**
	* \brief Destroy the material textures
	*/
	VEMaterial::~VEMaterial() {
		if (mapDiffuse!=nullptr) delete mapDiffuse;
		if (mapBump != nullptr) delete mapBump;
		if (mapNormal != nullptr) delete mapNormal;
		if (mapHeight != nullptr) delete mapHeight;
	};


	//---------------------------------------------------------------------
	//Texture

	/**
	*
	* \brief VETexture constructor from a list of textures.
	*
	* Create a VETexture from a list of textures. The textures must lie in the same directory and are stored in a texture array.
	* This can be used also as a cube map.
	*
	* \param[in] name The name of the mesh.
	* \param[in] basedir Name of the directory the files are in.
	* \param[in] texNames List of filenames of the textures.
	* \param[in] flags Vulkan flags for creating the textures.
	* \param[in] viewType Vulkan view tape for the image view.
	*
	*/
	VETexture::VETexture(	std::string name, std::string &basedir, std::vector<std::string> texNames,
							VkImageCreateFlags flags, VkImageViewType viewType ) : VENamedClass( name ) {
		if (texNames.size() == 0) return;

		vh::vhBufCreateTextureImage(getRendererPointer()->getDevice(), getRendererPointer()->getVmaAllocator(),
			getRendererPointer()->getGraphicsQueue(), getRendererPointer()->getCommandPool(),
			basedir, texNames, flags, &m_image, &m_deviceAllocation);

		vh::vhBufCreateImageView(getRendererPointer()->getDevice(), m_image,
			VK_FORMAT_R8G8B8A8_UNORM, viewType, (uint32_t)texNames.size(), VK_IMAGE_ASPECT_COLOR_BIT, &m_imageView);

		vh::vhBufCreateTextureSampler(getRendererPointer()->getDevice(), &m_sampler);
	}

	/**
	*
	* \brief VETexture constructor from a GLI cube map file.
	*
	* Create a VETexture from a GLI cubemap file. This has been loaded using GLI from a ktx or dds file.
	*
	* \param[in] name The name of the mesh.
	* \param[in] texCube The GLI cubemap structure
	*
	*/
	VETexture::VETexture(std::string name, gli::texture_cube &texCube) : VENamedClass( name ) {

		VkFormat vkformat;

		vh::vhBufCreateTexturecubeImage(getRendererPointer()->getDevice(), getRendererPointer()->getVmaAllocator(),
			getRendererPointer()->getGraphicsQueue(), getRendererPointer()->getCommandPool(),
			texCube, &m_image, &m_deviceAllocation, &vkformat);

		vh::vhBufCreateImageView(getRendererPointer()->getDevice(), m_image,
			vkformat, VK_IMAGE_VIEW_TYPE_CUBE, 6,
			VK_IMAGE_ASPECT_COLOR_BIT, &m_imageView);

		vh::vhBufCreateTextureSampler(getRendererPointer()->getDevice(), &m_sampler);
	}

	/**
	* \brief VETexture destructor - destroy the sampler, image view and image
	*/
	VETexture::~VETexture() {
		if (m_sampler != VK_NULL_HANDLE) vkDestroySampler(getRendererPointer()->getDevice(), m_sampler, nullptr);
		if (m_imageView != VK_NULL_HANDLE) vkDestroyImageView(getRendererPointer()->getDevice(), m_imageView, nullptr);
		if (m_image != VK_NULL_HANDLE) vmaDestroyImage(getRendererPointer()->getVmaAllocator(), m_image, m_deviceAllocation);
	}


	//---------------------------------------------------------------------
	//Entity


	/**
	*
	* \brief VEEntity constructor.
	*
	* Create a VEEntity from a mesh, material, using a transform and a parent.
	*
	* \param[in] name The name of the mesh.
	* \param[in] type Mesh type
	* \param[in] pMesh Pointer to the mesh.
	* \param[in] pMat Pointer to the material.
	* \param[in] transf The local to parent transform.
	* \param[in] parent Pointer to the entity's parent.
	*
	*/
	VEEntity::VEEntity(std::string name, veEntityType type, VEMesh *pMesh, VEMaterial *pMat, glm::mat4 transf, VEEntity *parent) :
						VENamedClass(name), m_entityType( type) {
		m_pEntityParent = parent;
		if (parent != nullptr) {
			parent->addChild(this);
		}
		setTransform(transf);

		if (pMesh != nullptr && pMat != nullptr) {
			m_pMesh = pMesh;
			m_pMaterial = pMat;
			m_drawEntity = true;
			m_castsShadow = true;
		}
	}


	/**
	*
	* \brief VEEntity constructor.
	*
	* Create an empyt VEEntity.
	*
	* \param[in] name The name of the mesh.
	*
	*/
	VEEntity::VEEntity(std::string name) : VENamedClass(name), m_entityType(VE_ENTITY_TYPE_OBJECT) {
	};


	/**
	*
	* \brief VEEntity destructor.
	*
	* Destroy the entity's UBOs.
	*
	*/
	VEEntity::~VEEntity() {
		for (uint32_t i = 0; i < m_uniformBuffers.size(); i++) {
			vmaDestroyBuffer(getRendererPointer()->getVmaAllocator(), m_uniformBuffers[i], m_uniformBuffersAllocation[i]);
		}
	}

	/**
	* \returns the entity's local to parent transform.
	*/
	glm::mat4x4 VEEntity::getTransform() {
		return m_transform;
	}

	/**
	* \brief Sets the entity's local to parent transform.
	*/
	void VEEntity::setTransform(glm::mat4x4 trans) {
		m_transform = trans;
		update();
	}

	/**
	* \brief Sets the entity's position.
	*/
	void VEEntity::setPosition(glm::vec3 pos) { 
		m_transform[3] = glm::vec4(pos.x, pos.y, pos.z, 1.0f);
		update();
	};

	/**
	* \returns the entity's position.
	*/
	glm::vec3 VEEntity::getPosition() { 
		return glm::vec3(m_transform[3].x, m_transform[3].y, m_transform[3].z);
	};

	/**
	* \returns the entity's local x-axis in parent space 
	*/
	glm::vec3 VEEntity::getXAxis() {
		glm::vec4 x = m_transform[0];
		return glm::vec3(x.x, x.y, x.z);
	}

	/**
	* \returns the entity's local y-axis in parent space
	*/
	glm::vec3 VEEntity::getYAxis() {
		glm::vec4 y = m_transform[1];
		return glm::vec3(y.x, y.y, y.z);
	}

	/**
	* \returns the entity's local z-axis in parent space
	*/
	glm::vec3 VEEntity::getZAxis() {
		glm::vec4 z = m_transform[2];
		return glm::vec3(z.x, z.y, z.z);
	}


	/**
	*
	* \brief Multiplies the entity's transform with another 4x4 transform.
	*
	* The transform can be a translation, sclaing, rotation etc.
	*
	* \param[in] trans The 4x4 transform that is multiplied from the left onto the entity's old transform.
	*
	*/
	void VEEntity::multiplyTransform(glm::mat4x4 trans) { 
		setTransform(trans*m_transform); 
	};

	/**
	*
	* \brief An entity's world matrix is the local to parent transform multiplied by the parent's world matrix.
	*
	* \returns the entity's world (aka model) matrix.
	*
	*/
	glm::mat4x4 VEEntity::getWorldTransform() {
		if (m_pEntityParent != nullptr) return m_pEntityParent->getWorldTransform() * m_transform;
		return m_transform;
	};


	/**
	*
	* \brief lookAt function
	*
	* \param[in] eye New position of the entity
	* \param[in] point Entity looks at this point (= new local z axis)
	* \param[in] up Up vector pointing up
	*
	*/
	void VEEntity::lookAt(glm::vec3 eye, glm::vec3 point, glm::vec3 up) {
		m_transform[3] = glm::vec4(eye.x, eye.y, eye.z, 1.0f);
		glm::vec3 z = glm::normalize(point - eye);
		up = glm::normalize(up);
		float corr = glm::dot(z, up);	//if z, up are lined up (corr=1 or corr=-1), decorrelate them
		if (1.0f - fabs(corr) < 0.00001f) {
			float sc = z.x + z.y + z.z;
			up = glm::normalize(glm::vec3(sc, sc, sc));
		}

		m_transform[2] = glm::vec4(z.x, z.y, z.z, 0.0f);
		glm::vec3 x = glm::cross( glm::normalize( up ), z );
		m_transform[0] = glm::vec4(x.x, x.y, x.z, 0.0f);
		glm::vec3 y = glm::cross(z, x);
		m_transform[1] = glm::vec4(y.x, y.y, y.z, 0.0f);
	}


	/**
	*
	* \brief Adds a child entity to the list of children.
	*
	* \param[in] pEntity Pointer to the new child.
	*
	*/
	void VEEntity::addChild(VEEntity * pEntity) {
		m_pEntityChildren.push_back(pEntity);
	}

	/**
	*
	* \brief remove a child from the children list - child is NOT destroyed
	*
	* \param[in] pEntity Pointer to the child to be removed.
	*
	*/
	void VEEntity::removeChild(VEEntity *pEntity) {
		for (uint32_t i = 0; i < m_pEntityChildren.size(); i++) {
			if (pEntity == m_pEntityChildren[i]) {
				VEEntity *last = m_pEntityChildren[m_pEntityChildren.size() - 1];
				m_pEntityChildren[i] = last;
				m_pEntityChildren.pop_back();		//child is not detroyed
				return;
			}
		}
	}

	/**
	*
	* \brief Update the entity's UBO buffer with the current world matrix
	*
	* If there is a parent, get the parent's world matrix. If not, set the parent matrix to identity.
	* Then call update(parent) to do the job.
	*
	*/
	void VEEntity::update() {
		glm::mat4x4 parentWorldMatrix = glm::mat4x4(1.0);
		if (m_pEntityParent != nullptr) {
			parentWorldMatrix = m_pEntityParent->getWorldTransform();
		}
		update(parentWorldMatrix);
	}

	/**
	*
	* \brief Update the entity's UBO buffer with the current world matrix
	* 
	* Calculate the new world matrix (and inv transpose matix to transform normal vectors).
	* Then copy the struct content into the UBO.
	*
	* \param[in] parentWorldMatrix The parent's world matrix or an identity matrix.
	*
	*/
	void VEEntity::update( glm::mat4x4 parentWorldMatrix ) {
		VESubrender::veUBOPerObject ubo = {};
		ubo.model = parentWorldMatrix * getTransform();
		ubo.modelInvTrans = glm::transpose(glm::inverse(ubo.model));
		if (m_pMaterial != nullptr) {
			ubo.color = m_pMaterial->color;
		}

		for (uint32_t i = 0; i < m_uniformBuffersAllocation.size(); i++) {
			void* data = nullptr;
			vmaMapMemory(getRendererPointer()->getVmaAllocator(), m_uniformBuffersAllocation[i], &data);
			memcpy(data, &ubo, sizeof(ubo));
			vmaUnmapMemory(getRendererPointer()->getVmaAllocator(), m_uniformBuffersAllocation[i]);
		}

		updateChildren(ubo.model);
	}

	/**
	* \brief Update the UBOs of all children of this entity
	*/
	void VEEntity::updateChildren( glm::mat4x4 worldMatrix ) {
		for (auto pEntity : m_pEntityChildren) {
			pEntity->update(worldMatrix);
		}
	}

	/**
	* \brief Get a bounding sphere for this entity
	*
	* Return the bounding sphere of the mesh that this entity represents (if there is one).
	* 
	* \param[out] center Pointer to the sphere center to return
	* \param[out] radius Pointer to the radius to return
	*
	*/
	void VEEntity::getBoundingSphere(glm::vec3 *center, float *radius) {
		*center = glm::vec3(0.0f, 0.0f, 0.0f);
		*radius = 1.0f;
		if (m_pMesh != nullptr) {
			*center = m_pMesh->m_boundingSphereCenter;
			*radius = m_pMesh->m_boundingSphereRadius;
		}
	}


	//-------------------------------------------------------------------------------------------------
	//camera

	/**
	*
	* \brief VECamera constructor. Set nearPlane, farPlane to a default value.
	*
	* \param[in] name Name of the camera.
	*
	*/
	VECamera::VECamera(std::string name) : VEEntity(name) {
		m_entityType = VE_ENTITY_TYPE_CAMERA_PROJECTIVE; 
	};

	/**
	*
	* \brief VECamera constructor. 
	*
	* \param[in] name Name of the camera.
	* \param[in] nearPlane Distance of near plane to the camera origin
	* \param[in] farPlane Distance of far plane to the camera origin
	*
	*/
	VECamera::VECamera(std::string name, float nearPlane, float farPlane) :
		VEEntity(name), m_nearPlane(nearPlane), m_farPlane(farPlane) {
		m_entityType = VE_ENTITY_TYPE_CAMERA_PROJECTIVE;
	}

	/**
	* \brief Get a bounding sphere for this camera
	*
	* Return the bounding sphere of camera frustum.
	*
	* \param[out] center Pointer to the sphere center to return
	* \param[out] radius Pointer to the radius to return
	*
	*/
	void VECamera::getBoundingSphere(glm::vec3 *center, float *radius) {
		std::vector<glm::vec4> points;

		getFrustumPoints(points);					//get frustum points in world space

		glm::vec4 mean(0.0f, 0.0f, 0.0f, 1.0f);
		for (auto point : points) {
			mean += point;
		}
		mean /= (float)points.size();

		float maxsq = 0.0f;
		for (auto point : points) {
			float sq = glm::dot(mean - point, mean - point);
			maxsq = sq > maxsq ? sq : maxsq;
		}

		*center = glm::vec3( mean.x, mean.y, mean.z );
		*radius = sqrt(maxsq);
	}


	/**
	*
	* \brief Create an ortho shadow camera from the camera's frustum bounding sphere
	*
	* \param[in] pLight Pointer to a light that defines the light direction.
	* \returns a new VECameraOrtho that can be used to create shadow maps for directional light
	*
	*/
	VECamera * VECamera::createShadowCameraOrtho(VELight *pLight) {

		glm::vec3 center;
		float radius;
		getBoundingSphere(&center, &radius);
		float diam = 2.0f * radius;
		VECameraOrtho *pCamOrtho = new VECameraOrtho("Ortho", 0.0, diam, diam, diam);

		glm::vec3 z = normalize(getZAxis());
		pCamOrtho->lookAt(center - radius * z, center, glm::vec3(0.0f, 1.0f, 0.0f ));

		return pCamOrtho;
	}

	/**
	*
	* \brief Create an ortho shadow camera from the camera's frustum bounding sphere
	*
	* \param[in] pLight Pointer to a light that defines the light direction.
	* \returns a new VECameraProjective that can be used to create shadow maps for spot light
	* 
	*/
	VECamera * VECamera::createShadowCameraProjective(VELight *pLight) {

		glm::vec3 center;
		float radius;
		getBoundingSphere(&center, &radius);
		float diam = 2.0f * radius;

		glm::vec3 pos = pLight->getPosition();
		glm::vec3 z = normalize(getZAxis());
		glm::vec3 end = center + radius * z;
		float pz = glm::dot(pos, z);
		float ez = glm::dot(end, z);
		float dist = ez - pz;
		if (dist <= 0.0) dist = 1.0;

		VECameraProjective *pCamProj = new VECameraProjective("Proj", 0.01f, dist, 1.0f, 90.0f/360.0f);
		pCamProj->lookAt(pos, pos + dist*z, glm::vec3(0.0f, 1.0f, 0.0f));

		return pCamProj;
	}



	//-------------------------------------------------------------------------------------------------
	//camera projective

	/**
	*
	* \brief VECameraProjective constructor. Set nearPlane, farPlane, aspect ratio and fov to a default value.
	*
	* \param[in] name Name of the camera.
	*
	*/
	VECameraProjective::VECameraProjective(std::string name ) : VECamera(name) {
		m_entityType = VE_ENTITY_TYPE_CAMERA_PROJECTIVE;
	};

	/**
	*
	* \brief VECameraProjective constructor.
	*
	* \param[in] name Name of the camera.
	* \param[in] nearPlane Distance of near plane to the camera origin
	* \param[in] farPlane Distance of far plane to the camera origin
	* \param[in] aspectRatio Ratio between width and height. Can change due to window size change.
	* \param[in] fov Vertical field of view angle.
	*
	*/
	VECameraProjective::VECameraProjective(std::string name, float nearPlane, float farPlane, float aspectRatio, float fov) :
			VECamera(name, nearPlane, farPlane), m_aspectRatio(aspectRatio), m_fov(fov)   {
		m_entityType = VE_ENTITY_TYPE_CAMERA_PROJECTIVE;
	};

	/**
	* \brief Get a projection matrix for this camera.
	* \param[in] width Width of the current game window.
	* \param[in] height Height of the current game window.
	* \returns the camera projection matrix.
	*/
	glm::mat4 VECameraProjective::getProjectionMatrix(float width, float height) {
		m_aspectRatio = width / height;
		glm::mat4 pm = glm::perspectiveFov( glm::radians(m_fov), (float) width, (float)height, m_nearPlane, m_farPlane);

		pm[2][2] *= -1.0f;		//camera looks down its positive z-axis, OpenGL function does it reverse
		pm[2][3] *= -1.0f;
		return pm;
	}

	/**
	* \brief Get a projection matrix for this camera.
	* \returns the camera projection matrix.
	*/
	glm::mat4 VECameraProjective::getProjectionMatrix() {
		return getProjectionMatrix( m_aspectRatio, 1.0f );
	}

	/**
	* \brief Get a list of 8 points making up the camera frustum
	*
	* The points are returned in world space.
	*
	* \param[out] points List of 8 points that make up the frustum in world space
	*
	*/
	void VECameraProjective::getFrustumPoints(std::vector<glm::vec4> &points) {
		float halfh = (float)tan( (m_fov/2.0f) * 2.0f * M_PI / 360.0f );
		float halfw = halfh * m_aspectRatio;

		glm::mat4 worldMatrix = getWorldTransform();

		points.push_back( worldMatrix * glm::vec4(-m_nearPlane * halfw, -m_nearPlane * halfh, m_nearPlane, 1.0f ) );
		points.push_back( worldMatrix * glm::vec4( m_nearPlane * halfw, -m_nearPlane * halfh, m_nearPlane, 1.0f));
		points.push_back( worldMatrix * glm::vec4(-m_nearPlane * halfw,  m_nearPlane * halfh, m_nearPlane, 1.0f));
		points.push_back( worldMatrix * glm::vec4( m_nearPlane * halfw,  m_nearPlane * halfh, m_nearPlane, 1.0f));

		points.push_back( worldMatrix * glm::vec4(-m_farPlane * halfw, -m_farPlane * halfh, m_farPlane, 1.0f));
		points.push_back( worldMatrix * glm::vec4( m_farPlane * halfw, -m_farPlane * halfh, m_farPlane, 1.0f));
		points.push_back( worldMatrix * glm::vec4(-m_farPlane * halfw,  m_farPlane * halfh, m_farPlane, 1.0f));
		points.push_back( worldMatrix * glm::vec4( m_farPlane * halfw,  m_farPlane * halfh, m_farPlane, 1.0f));
	}


	//-------------------------------------------------------------------------------------------------
	//camera ortho

	/**
	*
	* \brief VECameraOrtho constructor. Set nearPlane, farPlane, aspect ratio and fov to a default value.
	*
	* \param[in] name Name of the camera.
	*
	*/
	VECameraOrtho::VECameraOrtho(std::string name) : VECamera(name) {
		m_entityType = VE_ENTITY_TYPE_CAMERA_ORTHO;
	};

	/**
	*
	* \brief VECameraProjective constructor.
	*
	* \param[in] name Name of the camera.
	* \param[in] nearPlane Distance of near plane to the camera origin
	* \param[in] farPlane Distance of far plane to the camera origin
	* \param[in] width Width of the frustum
	* \param[in] height Height of the frustum
	*
	*/
	VECameraOrtho::VECameraOrtho(std::string name, float nearPlane, float farPlane, float width, float height) :
		VECamera(name, nearPlane, farPlane), m_width(width), m_height(height) {
		m_entityType = VE_ENTITY_TYPE_CAMERA_ORTHO;
	};


	/**
	* \brief Get a projection matrix for this camera.
	* \param[in] width Width of the current game window.
	* \param[in] height Height of the current game window.
	* \returns the camera projection matrix.
	*/
	glm::mat4 VECameraOrtho::getProjectionMatrix(float width, float height) {
		glm::mat4 pm = glm::ortho(-width * m_width / 2.0f, width * m_width / 2.0f, -height * m_height / 2.0f, height * m_height / 2.0f, m_nearPlane, m_farPlane);
		pm[2][2] *= -1;	//camera looks down its positive z-axis, OpenGL function does it reverse
		return pm;
	}

	/**
	* \brief Get a projection matrix for this camera.
	* \returns the camera projection matrix.
	*/
	glm::mat4 VECameraOrtho::getProjectionMatrix() {
		glm::mat4 pm = glm::ortho( -m_width/2.0f, m_width/2.0f, -m_height/2.0f, m_height/2.0f, m_nearPlane, m_farPlane);
		pm[2][2] *= -1;		//camera looks down its positive z-axis, OpenGL function does it reverse
		return pm;
	}

	/**
	* \brief Get a list of 8 points making up the camera frustum
	*
	* The points are returned in world space.
	*
	* \param[out] points List of 8 points that make up the frustum in world space
	*
	*/
	void VECameraOrtho::getFrustumPoints(std::vector<glm::vec4> &points) {
		float halfh = m_height / 2.0f;
		float halfw = m_width / 2.0f;

		glm::mat4 worldMatrix = getWorldTransform();

		points.push_back(worldMatrix * glm::vec4(-halfw, -halfh, m_nearPlane, 1.0f));
		points.push_back(worldMatrix * glm::vec4( halfw, -halfh, m_nearPlane, 1.0f));
		points.push_back(worldMatrix * glm::vec4(-halfw,  halfh, m_nearPlane, 1.0f));
		points.push_back(worldMatrix * glm::vec4( halfw,  halfh, m_nearPlane, 1.0f));

		points.push_back(worldMatrix * glm::vec4(-halfw, -halfh, m_farPlane, 1.0f));
		points.push_back(worldMatrix * glm::vec4( halfw, -halfh, m_farPlane, 1.0f));
		points.push_back(worldMatrix * glm::vec4(-halfw,  halfh, m_farPlane, 1.0f));
		points.push_back(worldMatrix * glm::vec4( halfw,  halfh, m_farPlane, 1.0f));
	}


	//-------------------------------------------------------------------------------------------------
	//light

	/**
	*
	* \brief Fill a UBO structure with the light's data
	*
	* \param[in] pLight Pointer to the light structure that will be copied into a UBO
	*
	*/
	void VELight::fillVhLightStructure(veLight *pLight) {
		pLight->type[0] = m_lightType;
		pLight->param = param;
		pLight->col_ambient = col_ambient;
		pLight->col_diffuse = col_diffuse;
		pLight->col_specular = col_specular;
		pLight->position = getWorldTransform();
	}

	/**
	* \brief Simple VELight constructor, default is directional light
	* \param[in] name Name of the camera
	* \param[in] type Light type
	*/
	VELight::VELight(std::string name, veLightType type) : VEEntity(name), m_lightType(type) {
		m_entityType = VE_ENTITY_TYPE_LIGHT;
	};

}


