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

export module VEEngine.V4.Math;

/// @file
/// @brief v4 math implementation over GLM and the selected scalar policy.
export namespace vve::v4::math {

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

   [[nodiscard]] inline Quat multiply(const Quat &lhs, const Quat &rhs) noexcept {
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
   [[nodiscard]] inline constexpr Scalar clamp(Scalar value, Scalar low, Scalar high) noexcept {
      return max(low, min(value, high));
   }
   [[nodiscard]] inline Vec3 clamp(const Vec3 &value, const Vec3 &low, const Vec3 &high) {
      return glm::clamp(value, low, high);
   }
   [[nodiscard]] inline Mat4 translate(const Mat4 &matrix, const Vec3 &offset) {
      return glm::translate(matrix, offset);
   }
   [[nodiscard]] inline Mat4 scale(const Mat4 &matrix, const Vec3 &factors) { return glm::scale(matrix, factors); }
   [[nodiscard]] inline Mat4 lookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up) {
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
   [[nodiscard]] inline Mat4 inverse(const Mat4 &matrix) { return glm::inverse(matrix); }

} // namespace vve::v4::math
