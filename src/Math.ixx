export module VEEngine.Math;
import std;
#if defined(VVE_ENGINE_IMPLEMENTATION_IS_V5)
import VEEngine.V5.Math;
#else
import VEEngine.V4.Math;
#endif

/**
	* @file
	* @brief Public math contract backed by the selected engine implementation.
	*/
export namespace vve::math {

	using Scalar = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::Scalar;	///< Facade scalar type.
	using Vec2	= VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::Vec2;	///< Facade 2D vector.
	using Vec3	= VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::Vec3;	///< Facade 3D vector.
	using Vec4	= VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::Vec4;	///< Facade 4D vector.
	using Quat	= VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::Quat;	///< Facade quaternion.
	using Mat4	= VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::Mat4;	///< Facade 4x4 matrix.

	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::add;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::clamp;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::cross;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::dot;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::identityMat4;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::identityQuat;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::inverse;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::length;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::lengthSquared;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::lookAt;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::max;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::min;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::multiply;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::normalize;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::one;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::oneVec3;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::perspective;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::perspectiveVulkan;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::scale;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::subtract;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::translate;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::zero;
	using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::zeroVec3;

} // namespace vve::math

export namespace vve {

	using Scalar = math::Scalar;													///< Facade scalar type.
	using Vec2	= math::Vec2;														///< Facade 2D vector.
	using Vec3	= math::Vec3;														///< Facade 3D vector.
	using Vec4	= math::Vec4;														///< Facade 4D vector.
	using Quat	= math::Quat;														///< Facade quaternion.
	using Mat4	= math::Mat4;														///< Facade 4x4 matrix.

	using math::identityMat4;
	using math::identityQuat;
	using math::one;
	using math::oneVec3;
	using math::zero;
	using math::zeroVec3;

} // namespace vve
