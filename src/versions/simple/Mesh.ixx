export module VEEngine.Simple.Mesh;
import std;

/**
	* @file
	* @brief CPU mesh data for the simple forward renderer.
	*
	* Functional objects:
	* - Vertex stores the host-side attributes consumed by the Slang vertex input.
	* - Mesh stores indexed CPU geometry in plain STL containers before any Vulkan upload exists.
	* - makeCube creates hard-coded indexed sample geometry for renderer upload tests.
	*/
export namespace vve::simple {

	/// @brief Host-side vertex matching the Slang locations 0, 1, and 2.
	struct Vertex {
		std::array<float, 3> position{}; ///< Location 0 object-space position, matching Slang float3.
		std::array<float, 3> color{};    ///< Location 1 vertex color, matching Slang float3.
		std::array<float, 2> texCoord{}; ///< Location 2 texture coordinate, matching Slang float2.
	};

	/// @brief Indexed CPU mesh stored before renderer resource creation.
	struct Mesh {
		std::vector<Vertex> vertices{};      ///< Vertex attributes in shader input order.
		std::vector<std::uint32_t> indices{}; ///< Triangle indices addressing vertices.
	};

	/**
		* @brief Creates a colored unit cube as CPU-only indexed sample geometry.
		*
		* @return Mesh with one quad per face, uniform vertex color, and two triangles per face.
		*/
	Mesh makeCube() {
		constexpr std::array<float, 3> cubeColor{0.55F, 0.55F, 0.55F}; ///< Uniform color avoids vertex interpolation gradients.
		return Mesh{
			.vertices{
				{{{-0.5F, -0.5F, 0.5F}},  cubeColor, {{0.0F, 0.0F}}}, ///< Front face lower left.
				{{{0.5F, -0.5F, 0.5F}},   cubeColor, {{1.0F, 0.0F}}}, ///< Front face lower right.
				{{{0.5F, 0.5F, 0.5F}},    cubeColor, {{1.0F, 1.0F}}}, ///< Front face upper right.
				{{{-0.5F, 0.5F, 0.5F}},   cubeColor, {{0.0F, 1.0F}}}, ///< Front face upper left.
				{{{0.5F, -0.5F, -0.5F}},  cubeColor, {{0.0F, 0.0F}}}, ///< Back face lower left.
				{{{-0.5F, -0.5F, -0.5F}}, cubeColor, {{1.0F, 0.0F}}}, ///< Back face lower right.
				{{{-0.5F, 0.5F, -0.5F}},  cubeColor, {{1.0F, 1.0F}}}, ///< Back face upper right.
				{{{0.5F, 0.5F, -0.5F}},   cubeColor, {{0.0F, 1.0F}}}, ///< Back face upper left.
				{{{-0.5F, -0.5F, -0.5F}}, cubeColor, {{0.0F, 0.0F}}}, ///< Left face lower left.
				{{{-0.5F, -0.5F, 0.5F}},  cubeColor, {{1.0F, 0.0F}}}, ///< Left face lower right.
				{{{-0.5F, 0.5F, 0.5F}},   cubeColor, {{1.0F, 1.0F}}}, ///< Left face upper right.
				{{{-0.5F, 0.5F, -0.5F}},  cubeColor, {{0.0F, 1.0F}}}, ///< Left face upper left.
				{{{0.5F, -0.5F, 0.5F}},   cubeColor, {{0.0F, 0.0F}}}, ///< Right face lower left.
				{{{0.5F, -0.5F, -0.5F}},  cubeColor, {{1.0F, 0.0F}}}, ///< Right face lower right.
				{{{0.5F, 0.5F, -0.5F}},   cubeColor, {{1.0F, 1.0F}}}, ///< Right face upper right.
				{{{0.5F, 0.5F, 0.5F}},    cubeColor, {{0.0F, 1.0F}}}, ///< Right face upper left.
				{{{-0.5F, 0.5F, 0.5F}},   cubeColor, {{0.0F, 0.0F}}}, ///< Top face lower left.
				{{{0.5F, 0.5F, 0.5F}},    cubeColor, {{1.0F, 0.0F}}}, ///< Top face lower right.
				{{{0.5F, 0.5F, -0.5F}},   cubeColor, {{1.0F, 1.0F}}}, ///< Top face upper right.
				{{{-0.5F, 0.5F, -0.5F}},  cubeColor, {{0.0F, 1.0F}}}, ///< Top face upper left.
				{{{-0.5F, -0.5F, -0.5F}}, cubeColor, {{0.0F, 0.0F}}}, ///< Bottom face lower left.
				{{{0.5F, -0.5F, -0.5F}},  cubeColor, {{1.0F, 0.0F}}}, ///< Bottom face lower right.
				{{{0.5F, -0.5F, 0.5F}},   cubeColor, {{1.0F, 1.0F}}}, ///< Bottom face upper right.
				{{{-0.5F, -0.5F, 0.5F}},  cubeColor, {{0.0F, 1.0F}}}, ///< Bottom face upper left.
			},
			.indices{
				0U, 1U, 2U, 2U, 3U, 0U,       ///< Front face triangles.
				4U, 5U, 6U, 6U, 7U, 4U,       ///< Back face triangles.
				8U, 9U, 10U, 10U, 11U, 8U,    ///< Left face triangles.
				12U, 13U, 14U, 14U, 15U, 12U, ///< Right face triangles.
				16U, 17U, 18U, 18U, 19U, 16U, ///< Top face triangles.
				20U, 21U, 22U, 22U, 23U, 20U, ///< Bottom face triangles.
			},
		};
	}

} // namespace vve::simple
