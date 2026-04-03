module;

#define GLM_ENABLE_EXPERIMENTAL
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

export module VEEngine:Math;
import std;

export namespace vve::math {

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

   [[nodiscard]] inline Vec3 zeroVec3() noexcept { return Vec3(zero(), zero(), zero()); }

   [[nodiscard]] inline Quat identityQuat() noexcept { return Quat(one(), zero(), zero(), zero()); }

   [[nodiscard]] inline Quat multiply(const Quat &lhs, const Quat &rhs) noexcept {
      return Quat(lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
                  lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
                  lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
                  lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w);
   }

   [[nodiscard]] inline Mat4 translate(const Mat4 &matrix, const Vec3 &offset) {
      return glm::translate(matrix, offset);
   }

   [[nodiscard]] inline Mat4 scale(const Mat4 &matrix, const Vec3 &factors) { return glm::scale(matrix, factors); }

   [[nodiscard]] inline Mat4 perspective(Scalar field_of_view_radians, Scalar aspect_ratio, Scalar near_plane,
                                         Scalar far_plane) {
      return glm::perspective(field_of_view_radians, aspect_ratio, near_plane, far_plane);
   }

   [[nodiscard]] inline Mat4 inverse(const Mat4 &matrix) { return glm::inverse(matrix); }

} // namespace vve::math
