/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VEMATERIAL_H
#define VEMATERIAL_H

namespace ve
{
	/**
		*
		* \brief Store texture data
		*
		* Store an image and a sampler as shader resource.
		*/

	struct VETexture : VENamedClass
	{
		VkImage m_image = VK_NULL_HANDLE; ///<image handle

		///\brief Vulkan handles for the image
		VkDescriptorImageInfo m_imageInfo = {
			VK_NULL_HANDLE, ///<sampler
			VK_NULL_HANDLE, ///<image view
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ///<layout
		};
		VmaAllocation m_deviceAllocation = nullptr; ///<VMA allocation info
		VkExtent2D m_extent = { 0, 0 }; ///<map extent
		VkFormat m_format; ///<texture format

		//VETexture(std::string name, gli::texture_cube &texCube, VkImageCreateFlags flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_CUBE);
		VETexture(std::string name, std::string &basedir, std::vector<std::string> texNames, VkImageCreateFlags flags = 0, VkImageViewType viewtype = VK_IMAGE_VIEW_TYPE_2D);

		///Empty constructor
		VETexture(std::string name)
			: VENamedClass(name) {};

		~VETexture();
	};

	/**
		*
		* \brief Store material data
		*
		* VEMaterial stores material data, including several textures, and color data.
		*/

	class VEMaterial : public VENamedClass
	{
	public:
		aiShadingMode shading = aiShadingMode_Phong; ///<Default shading model

		VETexture *mapDiffuse = nullptr; ///<Diiffuse texture
		VETexture *mapBump = nullptr; ///<Bump map
		VETexture *mapNormal = nullptr; ///<Normal map
		VETexture *mapHeight = nullptr; ///<Height map
		glm::vec4 color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f); ///<General color of the entity

		///Constructor
		VEMaterial(std::string name)
			: VENamedClass(name),
			mapDiffuse(nullptr),
			mapBump(nullptr),
			mapNormal(nullptr),
			mapHeight(nullptr),
			color(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) {};

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

	class VEMesh : public VENamedClass
	{
	protected:
		VEMesh(std::string name);		
	public:
		uint32_t m_vertexCount = 0; ///<Number of vertices in the vertex buffer
		uint32_t m_indexCount = 0; ///<Number of indices in the index buffer
		VkBuffer m_vertexBuffer = VK_NULL_HANDLE; ///<Vulkan vertex buffer handle
		VmaAllocation m_vertexBufferAllocation = nullptr; ///<VMA allocation info
		VkBuffer m_indexBuffer = VK_NULL_HANDLE; ///<Vulkan index buffer handle
		VmaAllocation m_indexBufferAllocation = nullptr; ///<VMA allocation info
		glm::vec3 m_boundingSphereCenter = glm::vec3(0.0f, 0.0f, 0.0f); ///<center of bounding sphere in local space
		float m_boundingSphereRadius = 1.0; ///<Radius of bounding sphere in local space

		VEMesh(std::string name, const aiMesh *paiMesh);

		VEMesh(std::string name, std::vector<vh::vhVertex> &vertices, std::vector<uint32_t> &indices);

		virtual ~VEMesh();
	};

	//--------------------------------Begin-Cloth-Simulation-Stuff----------------------------------
	// by Felix Neumann

	/// <summary>
	/// A extension of VEMesh that offers the possibility to update its vertices. This makes it
	/// possible to animate the positions of the mesh itself and not just the transformation of the
	/// whole model.
	/// </summary>
	class VEClothMesh : public VEMesh {
	private:
		VkBuffer m_stagingBuffer = VK_NULL_HANDLE;													// Additional buffer used for staging updated vertices
		VmaAllocation m_stagingBufferAllocation = nullptr;											// Memory allocation for the staging buffer
		VkDeviceSize m_bufferSize = 0;																// Size of the staging and vertex buffer
		std::vector<vh::vhVertex> m_initialVertices;												// The vector of vertices after construction
		std::vector<uint32_t> m_indices;															// Vector of indices. The indices and their copy within the index buffer do not change
		void* m_ptrToStageBufMem = nullptr;															// Pointer to the memory of the staging buffer
	public:
		/// <summary>
		/// Surpasses the default buffer creation of VEMesh so that it can create an additional
		/// custom staging buffer for updating the vertices.
		/// </summary>
		/// <param name="paiMesh"> assimp mesh. </param>
		VEClothMesh(const aiMesh* paiMesh);

		/// <summary>
		/// Destructor. Destroys the staging buffer and unmaps its memory allocation.
		/// </summary>
		virtual ~VEClothMesh();

		/// <summary>
		/// Updates the vertices of the mesh and calculates flat shading normals based on the new
		/// positions.
		/// </summary>
		/// <param name="vertices"> The new vertices. </param>
		/// <param name="updateBoundingSphere"> Wether the bounding sphere should be recalculated.
		/// </param>
		void updateVertices(std::vector<glm::vec3>& vertices, bool updateBoundingSphere = false);

		/// <summary>
		/// Get the initial vertices to modify them or for the construction of a cloth entitiy in
		/// the physics world.
		/// </summary>
		/// <returns> The vector of vertices fetched from the assimp mesh. <returns>
		const std::vector<glm::vec3>& getInitialVertices() const;

		/// <summary>
		/// Get the indices of the mesh to determine for example which vertices form a triangle.
		/// </summary>
		/// <returns> The vector of indices of the mesh. </returns>
		const std::vector<uint32_t>& getIndices() const;

		/// <summary>
		/// Since the positions of the mesh can change, the bounding sphere can as well. This is not
		/// done automatically, since it can be costly. Only call if needed.
		/// </summary>
		void calculateBoundingSphere(const std::vector<vh::vhVertex>& vertices);

	private:
		/// <summary>
		/// Create vertices and indices based on the assimp mesh. 
		/// </summary>
		/// <param name="paiMesh"> Assimp mesh. </param>
		void loadFromAiMesh(const aiMesh* paiMesh);

		/// <summary>
		/// Calculate flat shaded normals based on the new vertex positions.
		/// </summary>
		/// <param name="vertices"> Reference to the vertex vector. </param>
		void updateNormals(std::vector<vh::vhVertex>& vertices);

		/// <summary>
		/// Takes a vector of vertices and creates a copy that has a duplicate of itself appended to
		/// itself. The second part has inverted normals for correct shading and drawing of shadows
		/// in combination with the index vertex with its back appended.
		/// </summary>
		/// <param name="vertices"> The vector of all vertices of the mesh. </param>
		/// <returns> A vector where the second half is a copy of the first with inverted normals
		/// </returns>
		static std::vector<vh::vhVertex> createVerticesWithBack(
			const std::vector<vh::vhVertex>& vertices);

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
		static std::vector<uint32_t> createIndicesWithBack(std::vector<uint32_t>& indices,
			uint32_t highestIndex);

		/// <summary>
		/// Creates an index buffer and a vertex and vertex staging buffer.
		/// </summary>
		void createBuffers();
	};

	//---------------------------------End-Cloth-Simulation-Stuff-----------------------------------

} // namespace ve

#endif
