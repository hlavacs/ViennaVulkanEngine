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
