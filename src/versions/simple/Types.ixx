export module VEEngine.Simple.Types;
import std;
export import VEEngine.Types;
export import VEEngine.ECSContainer;

/// @file
/// @brief Vocabulary of the simple engine.
///
/// vve::simple is nested in vve, so the facade names exported by VEEngine.Types (Error, Vector, TypedHandle,
/// SceneHandle, Transform, Camera, ...) are found by ordinary lookup. Only the math vocabulary lives in
/// vve::math and is pulled into vve::simple here.
export namespace vve::simple {

	using Scalar = math::Scalar;	///< Configured math scalar type.
	using Vec2 = math::Vec2;		///< 2D vector.
	using Vec3 = math::Vec3;		///< 3D vector.
	using Vec4 = math::Vec4;		///< 4D vector.
	using Quat = math::Quat;		///< Quaternion.
	using Mat4 = math::Mat4;		///< 4x4 matrix.

	/// @brief Vertex payload shared by CPU render meshes and Vulkan vertex-input descriptions.
	struct RenderVertex {
		Vec3 position{math::zeroVec3()};											///< Object-space position.
		Vec3 normal{Vec3(math::zero(), math::one(), math::zero())};		///< Object-space normal.
		Vec2 uv{math::zero(), math::zero()};									///< First texture coordinate.
		Vec4 tangent{math::zero(), math::zero(), math::zero(), math::zero()}; ///< Object-space tangent with handedness in w, or zero when absent.
	};

	/// @brief Texture roles shared by imported asset materials and CPU render materials.
	enum class MaterialTextureSemantic : std::uint8_t {
		base_color,			///< Surface base-color map.
		normal,				///< Tangent-space normal map.
		metalness,			///< Metallic response map.
		roughness,			///< Surface roughness map.
		emissive,			///< Self-illumination map.
		ambient_occlusion	///< Ambient-occlusion map.
	};

	/// @brief One imported material texture with its renderer-facing semantic and canonical source path.
	struct MaterialTextureSource {
		MaterialTextureSemantic semantic{};	///< Renderer-facing texture role.
		std::filesystem::path path{};			///< Canonical absolute image path.
	};

	using math::add;
	using math::clamp;
	using math::cross;
	using math::dot;
	using math::identityMat4;
	using math::identityQuat;
	using math::inverse;
	using math::length;
	using math::lengthSquared;
	using math::lookAt;
	using math::max;
	using math::min;
	using math::multiply;
	using math::normalize;
	using math::one;
	using math::oneVec3;
	using math::orthoVulkan;
	using math::perspective;
	using math::perspectiveVulkan;
	using math::scale;
	using math::subtract;
	using math::translate;
	using math::zero;
	using math::zeroVec3;

} // namespace vve::simple
