module;

#define GLM_ENABLE_EXPERIMENTAL
#include <cmath>
#include <glm/common.hpp>
#include <glm/ext/matrix_double4x4.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_double.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/vector_double2.hpp>
#include <glm/ext/vector_double3.hpp>
#include <glm/ext/vector_double4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/geometric.hpp>

export module VEEngine.Simple.Math;

/// @file
/// @brief simple math implementation over GLM and the selected scalar policy.
export namespace vve::simple::math {

#if defined(VVE_MATH_USE_DOUBLE)
	using Scalar = double;
	using Vec2 = glm::dvec2;
	using Vec3 = glm::dvec3;
	using Vec4 = glm::dvec4;
	using Quat = glm::dquat;
	using Mat4 = glm::dmat4;
#else
	using Scalar = float;
	using Vec2 = glm::vec2;
	using Vec3 = glm::vec3;
	using Vec4 = glm::vec4;
	using Quat = glm::quat;
	using Mat4 = glm::mat4;
#endif

	[[nodiscard]] inline constexpr Scalar zero() noexcept { return static_cast<Scalar>(0); }
	[[nodiscard]] inline constexpr Scalar one() noexcept { return static_cast<Scalar>(1); }
	[[nodiscard]] inline Mat4 identityMat4() noexcept { return Mat4(one()); }
	[[nodiscard]] inline Vec3 oneVec3() noexcept { return Vec3(one(), one(), one()); }
	[[nodiscard]] inline Vec3 zeroVec3() noexcept { return Vec3(zero(), zero(), zero()); }
	[[nodiscard]] inline Quat identityQuat() noexcept { return Quat(one(), zero(), zero(), zero()); }

	[[nodiscard]] inline auto multiply(const Quat &lhs, const Quat &rhs) noexcept					-> Quat{
		return Quat(lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
						lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
						lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
						lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w);
	}

	[[nodiscard]] inline Mat4 multiply(const Mat4 &lhs, const Mat4 &rhs) { return lhs * rhs; }
	[[nodiscard]] inline Vec4 multiply(const Mat4 &lhs, const Vec4 &rhs) { return lhs * rhs; }
	[[nodiscard]] inline Vec2 add(const Vec2 &lhs, const Vec2 &rhs) { return lhs + rhs; }
	[[nodiscard]] inline Vec3 add(const Vec3 &lhs, const Vec3 &rhs) { return lhs + rhs; }
	[[nodiscard]] inline Vec4 add(const Vec4 &lhs, const Vec4 &rhs) { return lhs + rhs; }
	[[nodiscard]] inline Vec2 subtract(const Vec2 &lhs, const Vec2 &rhs) { return lhs - rhs; }
	[[nodiscard]] inline Vec3 subtract(const Vec3 &lhs, const Vec3 &rhs) { return lhs - rhs; }
	[[nodiscard]] inline Vec4 subtract(const Vec4 &lhs, const Vec4 &rhs) { return lhs - rhs; }
	[[nodiscard]] inline Vec2 scale(const Vec2 &value, Scalar factor) { return value * factor; }
	[[nodiscard]] inline Vec3 scale(const Vec3 &value, Scalar factor) { return value * factor; }
	[[nodiscard]] inline Vec4 scale(const Vec4 &value, Scalar factor) { return value * factor; }
	[[nodiscard]] inline Scalar dot(const Vec2 &lhs, const Vec2 &rhs) { return glm::dot(lhs, rhs); }
	[[nodiscard]] inline Scalar dot(const Vec3 &lhs, const Vec3 &rhs) { return glm::dot(lhs, rhs); }
	[[nodiscard]] inline Scalar dot(const Vec4 &lhs, const Vec4 &rhs) { return glm::dot(lhs, rhs); }
	[[nodiscard]] inline Vec3 cross(const Vec3 &lhs, const Vec3 &rhs) { return glm::cross(lhs, rhs); }
	[[nodiscard]] inline Scalar lengthSquared(const Vec2 &value) { return dot(value, value); }
	[[nodiscard]] inline Scalar lengthSquared(const Vec3 &value) { return dot(value, value); }
	[[nodiscard]] inline Scalar lengthSquared(const Vec4 &value) { return dot(value, value); }
	[[nodiscard]] inline Scalar length(const Vec2 &value) { return glm::length(value); }
	[[nodiscard]] inline Scalar length(const Vec3 &value) { return glm::length(value); }
	[[nodiscard]] inline Scalar length(const Vec4 &value) { return glm::length(value); }
	[[nodiscard]] inline Vec2 normalize(const Vec2 &value) { return glm::normalize(value); }
	[[nodiscard]] inline Vec3 normalize(const Vec3 &value) { return glm::normalize(value); }
	[[nodiscard]] inline Vec4 normalize(const Vec4 &value) { return glm::normalize(value); }
	[[nodiscard]] inline constexpr Scalar min(Scalar lhs, Scalar rhs) noexcept { return lhs < rhs ? lhs : rhs; }
	[[nodiscard]] inline Vec2 min(const Vec2 &lhs, const Vec2 &rhs) { return glm::min(lhs, rhs); }
	[[nodiscard]] inline Vec3 min(const Vec3 &lhs, const Vec3 &rhs) { return glm::min(lhs, rhs); }
	[[nodiscard]] inline Vec4 min(const Vec4 &lhs, const Vec4 &rhs) { return glm::min(lhs, rhs); }
	[[nodiscard]] inline constexpr Scalar max(Scalar lhs, Scalar rhs) noexcept { return lhs > rhs ? lhs : rhs; }
	[[nodiscard]] inline Vec2 max(const Vec2 &lhs, const Vec2 &rhs) { return glm::max(lhs, rhs); }
	[[nodiscard]] inline Vec3 max(const Vec3 &lhs, const Vec3 &rhs) { return glm::max(lhs, rhs); }
	[[nodiscard]] inline Vec4 max(const Vec4 &lhs, const Vec4 &rhs) { return glm::max(lhs, rhs); }
	[[nodiscard]] inline constexpr auto clamp(Scalar value, Scalar low, Scalar high) noexcept	-> Scalar{
		return max(low, min(value, high));
	}
	[[nodiscard]] inline auto clamp(const Vec3 &value, const Vec3 &low, const Vec3 &high)		-> Vec3{
		return glm::clamp(value, low, high);
	}
	[[nodiscard]] inline auto translate(const Mat4 &matrix, const Vec3 &offset)					-> Mat4{
		return glm::translate(matrix, offset);
	}
	[[nodiscard]] inline Mat4 scale(const Mat4 &matrix, const Vec3 &factors) { return glm::scale(matrix, factors); }
	[[nodiscard]] inline auto lookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up)		-> Mat4{
		return glm::lookAt(eye, center, up);
	}
	[[nodiscard]] inline Mat4 perspective(Scalar field_of_view_radians, Scalar aspect_ratio, Scalar near_plane,
														Scalar far_plane) {
		return glm::perspective(field_of_view_radians, aspect_ratio, near_plane, far_plane);
	}
	[[nodiscard]] inline Mat4 perspectiveVulkan(Scalar field_of_view_radians, Scalar aspect_ratio, Scalar near_plane,
																Scalar far_plane) {
		auto result = Mat4(zero());
		const auto f = one() / static_cast<Scalar>(std::tan(field_of_view_radians / static_cast<Scalar>(2)));
		result[0][0] = f / aspect_ratio;
		result[1][1] = -f;
		result[2][2] = far_plane / (near_plane - far_plane);
		result[2][3] = -one();
		result[3][2] = (far_plane * near_plane) / (near_plane - far_plane);
		return result;
	}
	[[nodiscard]] inline Mat4 orthoVulkan(Scalar left, Scalar right, Scalar bottom, Scalar top, Scalar near_plane, Scalar far_plane) {
		auto result = Mat4(one());
		result[0][0] = static_cast<Scalar>(2) / (right - left);
		result[1][1] = static_cast<Scalar>(-2) / (top - bottom);
		result[2][2] = one() / (near_plane - far_plane);
		result[3][0] = -(right + left) / (right - left);
		result[3][1] = -(top + bottom) / (top - bottom);
		result[3][2] = near_plane / (near_plane - far_plane);
		return result;
	}

	[[nodiscard]] inline Mat4 inverse(const Mat4 &matrix) { return glm::inverse(matrix); }

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
	using math::perspective;
	using math::perspectiveVulkan;
	using math::orthoVulkan;
	using math::scale;
	using math::subtract;
	using math::translate;
	using math::zero;
	using math::zeroVec3;
}
