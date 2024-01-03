/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VEInclude.h"

namespace ve
{
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

	VEMesh::VEMesh(std::string name, const aiMesh *paiMesh)
		: VENamedClass(name)
	{
		std::vector<vh::vhVertex> vertices; //vertex array
		std::vector<uint32_t> indices; //index array

		//copy the mesh vertex data
		m_vertexCount = paiMesh->mNumVertices;
		m_boundingSphereRadius = 0.0f;
		m_boundingSphereCenter = glm::vec3(0.0f, 0.0f, 0.0f);
		for (uint32_t i = 0; i < paiMesh->mNumVertices; i++)
		{
			vh::vhVertex vertex;
			vertex.pos.x = paiMesh->mVertices[i].x; //copy 3D position in local space
			vertex.pos.y = paiMesh->mVertices[i].y;
			vertex.pos.z = paiMesh->mVertices[i].z;

			m_boundingSphereRadius = std::max(
				std::max(std::max(vertex.pos.x * vertex.pos.x, vertex.pos.y * vertex.pos.y),
					vertex.pos.z * vertex.pos.z),
				m_boundingSphereRadius);

			if (paiMesh->HasNormals())
			{ //copy normals
				vertex.normal.x = paiMesh->mNormals[i].x;
				vertex.normal.y = paiMesh->mNormals[i].y;
				vertex.normal.z = paiMesh->mNormals[i].z;
			}

			if (paiMesh->HasTangentsAndBitangents() && paiMesh->mTangents)
			{ //copy tangents
				vertex.tangent.x = paiMesh->mTangents[i].x;
				vertex.tangent.y = paiMesh->mTangents[i].y;
				vertex.tangent.z = paiMesh->mTangents[i].z;
			}

			if (paiMesh->HasTextureCoords(0))
			{ //copy texture coordinates
				vertex.texCoord.x = paiMesh->mTextureCoords[0][i].x;
				vertex.texCoord.y = paiMesh->mTextureCoords[0][i].y;
			}

			vertices.push_back(vertex);
		}
		m_boundingSphereRadius = sqrt(m_boundingSphereRadius);

		//got through the aiMesh faces, and copy the indices
		m_indexCount = 0;
		for (uint32_t i = 0; i < paiMesh->mNumFaces; i++)
		{
			for (uint32_t j = 0; j < paiMesh->mFaces[i].mNumIndices; j++)
			{
				indices.push_back(paiMesh->mFaces[i].mIndices[j]);
				m_indexCount++;
			}
		}

		//create the vertex buffer
		VECHECKRESULT(vh::vhBufCreateVertexBuffer(getEnginePointer()->getRenderer()->getDevice(),
			getEnginePointer()->getRenderer()->getVmaAllocator(),
			getEnginePointer()->getRenderer()->getGraphicsQueue(),
			getEnginePointer()->getRenderer()->getCommandPool(),
			vertices, &m_vertexBuffer, &m_vertexBufferAllocation));

		//create the index buffer
		VECHECKRESULT(vh::vhBufCreateIndexBuffer(getEnginePointer()->getRenderer()->getDevice(),
			getEnginePointer()->getRenderer()->getVmaAllocator(),
			getEnginePointer()->getRenderer()->getGraphicsQueue(),
			getEnginePointer()->getRenderer()->getCommandPool(),
			indices, &m_indexBuffer, &m_indexBufferAllocation));
	}

	/**
		*
		* \brief VEMesh constructor from a vertex and an index list
		*
		* \param[in] name The name of the mesh.
		* \param[in] vertices A list of vertices to be used
		* \param[in] indices A list of indices to be used
		*
		*/

	VEMesh::VEMesh(std::string name, std::vector<vh::vhVertex> &vertices, std::vector<uint32_t> &indices)
		: VENamedClass(name)
	{
		//copy the mesh vertex data
		m_vertexCount = (uint32_t)vertices.size();
		m_boundingSphereRadius = 0.0f;
		m_boundingSphereCenter = glm::vec3(0.0f, 0.0f, 0.0f);
		for (uint32_t i = 0; i < vertices.size(); i++)
		{ //find max over all vertices
			m_boundingSphereRadius = std::max(
				std::max(std::max(vertices[i].pos.x * vertices[i].pos.x, vertices[i].pos.y * vertices[i].pos.y),
					vertices[i].pos.z * vertices[i].pos.z),
				m_boundingSphereRadius);
		}
		m_boundingSphereRadius = sqrt(m_boundingSphereRadius);

		m_indexCount = (uint32_t)indices.size();

		//create the vertex buffer
		VECHECKRESULT(vh::vhBufCreateVertexBuffer(getEnginePointer()->getRenderer()->getDevice(),
			getEnginePointer()->getRenderer()->getVmaAllocator(),
			getEnginePointer()->getRenderer()->getGraphicsQueue(),
			getEnginePointer()->getRenderer()->getCommandPool(),
			vertices, &m_vertexBuffer, &m_vertexBufferAllocation));

		//create the index buffer
		VECHECKRESULT(vh::vhBufCreateIndexBuffer(getEnginePointer()->getRenderer()->getDevice(),
			getEnginePointer()->getRenderer()->getVmaAllocator(),
			getEnginePointer()->getRenderer()->getGraphicsQueue(),
			getEnginePointer()->getRenderer()->getCommandPool(),
			indices, &m_indexBuffer, &m_indexBufferAllocation));
	}

	/**
		* \brief Destroy the vertex and index buffers
		*/
	VEMesh::~VEMesh()
	{
		vmaDestroyBuffer(getEnginePointer()->getRenderer()->getVmaAllocator(), m_indexBuffer, m_indexBufferAllocation);
		vmaDestroyBuffer(getEnginePointer()->getRenderer()->getVmaAllocator(), m_vertexBuffer, m_vertexBufferAllocation);
	}

	//---------------------------------------------------------------------
	//Material

	/**
		* \brief Destroy the material textures
		*/
	VEMaterial::~VEMaterial() {
		/*if (mapDiffuse != nullptr) delete mapDiffuse;
			if (mapBump != nullptr) delete mapBump;
			if (mapNormal != nullptr) delete mapNormal;
			if (mapHeight != nullptr) delete mapHeight;*/
	};

	//---------------------------------------------------------------------
	//Texture

	/**
		*
		* \brief VETexture constructor from a list of textures.
		*
		* Create a VETexture from a list of textures. The textures must lie in the same directory and are stored in a texture array.
		* This can be used also as a 2D texture or a cube map.
		*
		* \param[in] name The name of the mesh.
		* \param[in] basedir Name of the directory the files are in.
		* \param[in] texNames List of filenames of the textures.
		* \param[in] flags Vulkan flags for creating the textures.
		* \param[in] viewType Vulkan view tape for the image view.
		*
		*/
	VETexture::VETexture(std::string name,
		std::string &basedir,
		std::vector<std::string> texNames,
		VkImageCreateFlags flags,
		VkImageViewType viewType)
		: VENamedClass(name)
	{
		if (texNames.size() == 0)
			return;

		VECHECKRESULT(vh::vhBufCreateTextureImage(getEnginePointer()->getRenderer()->getDevice(),
			getEnginePointer()->getRenderer()->getVmaAllocator(),
			getEnginePointer()->getRenderer()->getGraphicsQueue(),
			getEnginePointer()->getRenderer()->getCommandPool(),
			basedir, texNames, flags, &m_image, &m_deviceAllocation, &m_extent));

		m_format = VK_FORMAT_R8G8B8A8_UNORM;
		VECHECKRESULT(vh::vhBufCreateImageView(getEnginePointer()->getRenderer()->getDevice(), m_image,
			m_format, viewType,
			(uint32_t)texNames.size(), VK_IMAGE_ASPECT_COLOR_BIT,
			&m_imageInfo.imageView));

		VECHECKRESULT(
			vh::vhBufCreateTextureSampler(getEnginePointer()->getRenderer()->getDevice(), &m_imageInfo.sampler));
	}

	/**
		*
		* \brief VETexture constructor from a GLI cube map file.
		*
		* Create a VETexture from a GLI cubemap file. This has been loaded using GLI from a ktx or dds file.
		*
		* \param[in] name The name of the mesh.
		* \param[in] texCube The GLI cubemap structure
		* \param[in] flags Create flags for the images (e.g. Cube compatible or array)
		* \param[in] viewType Type for the image views
		*
		*/
		/*VETexture::VETexture(std::string name, gli::texture_cube &texCube,
				VkImageCreateFlags flags, VkImageViewType viewType) : VENamedClass(name) {

				VECHECKRESULT(vh::vhBufCreateTexturecubeImage(getEnginePointer()->getRenderer()->getDevice(), getEnginePointer()->getRenderer()->getVmaAllocator(),
					getEnginePointer()->getRenderer()->getGraphicsQueue(), getEnginePointer()->getRenderer()->getCommandPool(),
									texCube, &m_image, &m_deviceAllocation, &m_format));

				m_extent.width = texCube.extent().x;
				m_extent.height = texCube.extent().y;

				VECHECKRESULT(vh::vhBufCreateImageView(getEnginePointer()->getRenderer()->getDevice(), m_image,
									m_format, VK_IMAGE_VIEW_TYPE_CUBE, 6, VK_IMAGE_ASPECT_COLOR_BIT, &m_imageInfo.imageView));

				VECHECKRESULT(vh::vhBufCreateTextureSampler(getEnginePointer()->getRenderer()->getDevice(), &m_imageInfo.sampler));
			}*/

			/**
				* \brief VETexture destructor - destroy the sampler, image view and image
				*/
	VETexture::~VETexture()
	{
		if (m_imageInfo.sampler != VK_NULL_HANDLE)
			vkDestroySampler(getEnginePointer()->getRenderer()->getDevice(), m_imageInfo.sampler, nullptr);
		if (m_imageInfo.imageView != VK_NULL_HANDLE)
			vkDestroyImageView(getEnginePointer()->getRenderer()->getDevice(), m_imageInfo.imageView, nullptr);
		if (m_image != VK_NULL_HANDLE)
			vmaDestroyImage(getEnginePointer()->getRenderer()->getVmaAllocator(), m_image, m_deviceAllocation);
	}

	//--------------------------------Begin-Cloth-Simulation-Stuff----------------------------------
	// by Felix Neumann

	/// <summary>
	/// A constructor of for the VEMesh class that allows to surpass the default buffer creation.
	/// </summary>
	/// <param name="name"></param>
	VEMesh::VEMesh(std::string name) : VENamedClass(name) {}

	/// <summary>
	/// Surpasses the default buffer creation of VEMesh so that it can create an additional custom
	/// staging buffer for updating the vertices.
	/// </summary>
	/// <param name="paiMesh"> assimp mesh. </param>
	VEClothMesh::VEClothMesh(const aiMesh* paiMesh) : VEMesh::VEMesh("A Cloth Mesh")
	{
		loadFromAiMesh(paiMesh);
		calculateBoundingSphere(m_initialVertices);
		createBuffers();
	}

	/// <summary>
	/// Destructor. Destroys the staging buffer and unmaps its memory allocation.
	/// </summary>
	VEClothMesh::~VEClothMesh()
	{
		vmaUnmapMemory(getEnginePointer()->getRenderer()->getVmaAllocator(),
			m_stagingBufferAllocation);

		vmaDestroyBuffer(getEnginePointer()->getRenderer()->getVmaAllocator(),
			m_stagingBuffer, m_stagingBufferAllocation);
	}

	/// <summary>
	/// Updates the vertices of the mesh and calculates flat shading normals based on the new
	/// positions.
	/// </summary>
	/// <param name="vertices"> The new vertices. </param>
	/// <param name="updateBoundingSphere"> Wether the bounding sphere should be recalculated.
	/// </param>
	void VEClothMesh::updateVertices(std::vector<glm::vec3>& glmvertices, bool updateBoundingSphere)
	{
		std::vector<vh::vhVertex> vertices;
		for (auto gv : glmvertices) {
			vh::vhVertex v;
			v.pos = gv;
			vertices.emplace_back(v);
		}

		updateNormals(vertices);																	// Calculate flat shading normals based on the new vertex															

		vertices = createVerticesWithBack(vertices);												// Create a vector twice its original size with flipped triangles appended
		// for correct rendering of both sides of the
		VECHECKRESULT(vh::updateClothStagingBuffer(vertices, m_bufferSize, m_ptrToStageBufMem));	// Copy the new vertex data into the staging buffer

		VECHECKRESULT(vh::updateClothVertexBuffer(													// Update the main vertex buffer with the data from
			getEnginePointer()->getRenderer()->getDevice(),											// the staging buffer
			getEnginePointer()->getRenderer()->getVmaAllocator(),
			getEnginePointer()->getRenderer()->getGraphicsQueue(),
			getEnginePointer()->getRenderer()->getCommandPool(),
			m_vertexBuffer, m_stagingBuffer, m_bufferSize));

		if (updateBoundingSphere)																	// Recalculate the boinding sphere
			calculateBoundingSphere(vertices);														// Costly, so false by default
	}

	/// <summary>
	/// Get the initial vertices for example for the construction of cloth (or other soft
	/// bodies).
	/// </summary>
	/// <returns> The vector of vertices fetched from the assimp mesh. <returns>
	const std::vector<glm::vec3>& VEClothMesh::getInitialVertices() const {
		std::vector<glm::vec3> vertices;
		for (auto v : m_initialVertices) {
			vertices.emplace_back(v.pos);
		}
		return vertices;
	}

	/// <summary>
	/// Get the indices of the mesh to determine for example which vertices form a triangle.
	/// </summary>
	/// <returns> The vector of indices of the mesh. </returns>
	const std::vector<uint32_t>& VEClothMesh::getIndices() const {
		return m_indices;
	}

	/// <summary>
	/// Since the positions of the mesh can change, the bounding sphere can as well. The calculation
	/// can be costly.
	/// </summary>
	void VEClothMesh::calculateBoundingSphere(const std::vector<vh::vhVertex>& vertices)
	{
		glm::vec3 centre{};																			// Get the centre by adding up all vertex positions
		// and dividing the sum by the count
		for (const vh::vhVertex& vertex : vertices)
			centre += vertex.pos;

		centre = centre / (float)vertices.size();

		float sphereRadius{};																		// Get the radius by finding the max distance between
		// the centre and a vertex
		for (const vh::vhVertex& vertex : vertices)
		{
			float distance = glm::distance(centre, vertex.pos);

			if (distance > sphereRadius)
				sphereRadius = distance;
		}
	}

	/// <summary>
	/// Create vertices and indices based on the assimp mesh. 
	/// </summary>
	/// <param name="paiMesh"> Assimp mesh. </param>
	void VEClothMesh::loadFromAiMesh(const aiMesh* paiMesh)
	{
		for (uint32_t i = 0; i < paiMesh->mNumVertices; i++)										// Copy the vertices
		{
			vh::vhVertex vertex;
			vertex.pos.x = paiMesh->mVertices[i].x;
			vertex.pos.y = paiMesh->mVertices[i].y;
			vertex.pos.z = paiMesh->mVertices[i].z;

			if (paiMesh->HasTextureCoords(0))
			{
				vertex.texCoord.x = paiMesh->mTextureCoords[0][i].x;
				vertex.texCoord.y = paiMesh->mTextureCoords[0][i].y;
			}

			m_initialVertices.push_back(vertex);
		}

		for (uint32_t i = 0; i < paiMesh->mNumFaces; i++)											// Copy the indices
			for (uint32_t j = 0; j < paiMesh->mFaces[i].mNumIndices; j++)
				m_indices.push_back(paiMesh->mFaces[i].mIndices[j]);

		m_indexCount = static_cast<uint32_t>(m_indices.size()) * 2;									// *2 Because its rendered twice, front and back for correct shadows

		updateNormals(m_initialVertices);
	}

	/// <summary>
	/// Calculate flat shaded normals based on the new vertex positions.
	/// </summary>
	/// <param name="vertices"> Reference to the vertex vector. </param>
	void VEClothMesh::updateNormals(std::vector<vh::vhVertex>& vertices)
	{
		for (size_t i = 0; i < m_indices.size(); i += 3)
		{
			glm::vec3 p0 = vertices[m_indices[i]].pos;
			glm::vec3 p1 = vertices[m_indices[i + 1]].pos;
			glm::vec3 p2 = vertices[m_indices[i + 2]].pos;

			glm::vec3 normal = glm::normalize(glm::cross(p0 - p1, p0 - p2)) * -1.f;					// The normal is the normalized cross product of two edges (vectors)
			// which form the triangle
			vertices[m_indices[i]].normal = normal;
			vertices[m_indices[i + 1]].normal = normal;
			vertices[m_indices[i + 2]].normal = normal;
		}
	}

	/// <summary>
	/// Takes a vector of vertices and creates a copy that has a duplicate of itself appended to
	/// itself. The second part has inverted normals for correct shading and drawing of shadows.
	/// </summary>
	/// <param name="vertices"> The vector of all vertices of the mesh. </param>
	/// <returns> A vector where the second half is a copy of the first with inverted normals
	/// </returns>
	std::vector<vh::vhVertex> VEClothMesh::createVerticesWithBack(
		const std::vector<vh::vhVertex>& vertices)
	{
		std::vector<vh::vhVertex> verticesWithBack(vertices);										// Create a copy

		verticesWithBack.reserve(vertices.size() * 2);												// Append all entries of the copy to itself
		std::copy(vertices.begin(), vertices.end(), std::back_inserter(verticesWithBack));

		for (size_t i = vertices.size(); i < vertices.size() * 2; ++i)								// Flip the normals of the second half for correct shading
			verticesWithBack[i].normal *= -2;														// The increased length of the normal signals the shader program not to draw the shadow map
		// onto the cloth a second time (little bit hacky but other solutions would have required
		return verticesWithBack;																	// significant modifications of the engines source code)
	}

	/// <summary>
	/// Takes a vector of indices and creates a copy that has a modified copy of itself appended
	/// to itself. For the second part the lowest index is the highest index of the first part
	/// + 1. The stepsize between the indices is then the same. The first and last index of each
	/// triangle are switched so that the result are flipped triangles for correct shading and
	/// drawing of shadows in combination with the vertex vector with its back appended.
	/// </summary>
	/// <param name="indices"> The vector of all indices of the mesh. </param>
	/// <param name="highestIndex"> The highest index of all indices of the mesh. </param>
	/// <returns> A vector where the second half is a modified version of the first with flipped
	/// triangles. </returns>
	std::vector<uint32_t> VEClothMesh::createIndicesWithBack(std::vector<uint32_t>& indices,
		uint32_t highestIndex)
	{
		std::vector<uint32_t> indicesWithBack(indices);												// create a copy

		for (size_t i = 1; i < indices.size(); i += 3)												// append all entries but with the first and last entry of each
		{																							// triangle switched.
			indicesWithBack.push_back(highestIndex + indices[i + 1]);
			indicesWithBack.push_back(highestIndex + indices[i]);
			indicesWithBack.push_back(highestIndex + indices[i - 1]);
		}

		return indicesWithBack;
	}

	/// <summary>
	/// Creates an index buffer and a vertex and vertex staging buffer.
	/// </summary>
	void VEClothMesh::createBuffers()
	{
		auto vertices = createVerticesWithBack(m_initialVertices);									// Take the initial vertex and index vectors and create copies with a modified
		auto indices = createIndicesWithBack(m_indices, m_initialVertices.size());					// version of itself appended so that shadows are drawn correctly.

		VECHECKRESULT(vh::vhBufCreateClothVertexBuffers(											// Creates a vertex buffer analogous to the one of a normal VEMesh
			getEnginePointer()->getRenderer()->getDevice(),											// and a staging buffer for updating the vertices.
			getEnginePointer()->getRenderer()->getVmaAllocator(),
			getEnginePointer()->getRenderer()->getGraphicsQueue(),
			getEnginePointer()->getRenderer()->getCommandPool(),
			vertices, &m_vertexBuffer, &m_vertexBufferAllocation, &m_stagingBuffer,
			&m_stagingBufferAllocation, &m_ptrToStageBufMem, &m_bufferSize));

		VECHECKRESULT(vh::vhBufCreateIndexBuffer(													// The index buffer is created analogously to the one of a normal VEMesh
			getEnginePointer()->getRenderer()->getDevice(),
			getEnginePointer()->getRenderer()->getVmaAllocator(),
			getEnginePointer()->getRenderer()->getGraphicsQueue(),
			getEnginePointer()->getRenderer()->getCommandPool(),
			indices, &m_indexBuffer, &m_indexBufferAllocation));
	}

	//---------------------------------End-Cloth-Simulation-Stuff-----------------------------------


} // namespace ve
