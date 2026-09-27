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

	/// @brief Configured math scalar type used by the simple engine.
	using Scalar = math::Scalar;
	/// @brief Two-dimensional vector from the shared math vocabulary.
	using Vec2 = math::Vec2;
	/// @brief Three-dimensional vector from the shared math vocabulary.
	using Vec3 = math::Vec3;
	/// @brief Four-dimensional vector from the shared math vocabulary.
	using Vec4 = math::Vec4;
	/// @brief Quaternion from the shared math vocabulary.
	using Quat = math::Quat;
	/// @brief Four-by-four matrix from the shared math vocabulary.
	using Mat4 = math::Mat4;

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
