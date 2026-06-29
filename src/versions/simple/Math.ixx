export module VEEngine.Simple.Math;
import VEEngine.Math;

/// @file
/// @brief simple math compatibility names bound to the facade-owned vocabulary.
export namespace vve::simple::math {

	using vve::math::Scalar;
	using vve::math::Vec2;
	using vve::math::Vec3;
	using vve::math::Vec4;
	using vve::math::Quat;
	using vve::math::Mat4;
	using vve::math::add;
	using vve::math::clamp;
	using vve::math::cross;
	using vve::math::dot;
	using vve::math::identityMat4;
	using vve::math::identityQuat;
	using vve::math::inverse;
	using vve::math::length;
	using vve::math::lengthSquared;
	using vve::math::lookAt;
	using vve::math::max;
	using vve::math::min;
	using vve::math::multiply;
	using vve::math::normalize;
	using vve::math::one;
	using vve::math::oneVec3;
	using vve::math::orthoVulkan;
	using vve::math::perspective;
	using vve::math::perspectiveVulkan;
	using vve::math::scale;
	using vve::math::subtract;
	using vve::math::translate;
	using vve::math::zero;
	using vve::math::zeroVec3;

} // namespace vve::simple::math

export namespace vve::simple {
	using Scalar = math::Scalar;
	using Vec2 = math::Vec2;
	using Vec3 = math::Vec3;
	using Vec4 = math::Vec4;
	using Quat = math::Quat;
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
}
