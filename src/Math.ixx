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

/**
 * @file
 * @brief Thin math facade over GLM and the selected engine scalar policy.
 *
 * This partition intentionally contains only math aliases and stateless helper
 * functions. Strong semantic engine value types live in VEEngine:Types.
 */
export namespace vve::math {

#if defined(VVE_MATH_USE_DOUBLE)
   using Scalar = double;   ///< Scalar type selected for engine math at compile time.
   using Vec2 = glm::dvec2; ///< Two-component vector using the selected scalar precision.
   using Vec3 = glm::dvec3; ///< Three-component vector using the selected scalar precision.
   using Vec4 = glm::dvec4; ///< Four-component vector using the selected scalar precision.
   using Quat = glm::dquat; ///< Quaternion using the selected scalar precision.
   using Mat4 = glm::dmat4; ///< 4x4 matrix using the selected scalar precision.
#else
   using Scalar = float;   ///< Scalar type selected for engine math at compile time.
   using Vec2 = glm::vec2; ///< Two-component vector using the selected scalar precision.
   using Vec3 = glm::vec3; ///< Three-component vector using the selected scalar precision.
   using Vec4 = glm::vec4; ///< Four-component vector using the selected scalar precision.
   using Quat = glm::quat; ///< Quaternion using the selected scalar precision.
   using Mat4 = glm::mat4; ///< 4x4 matrix using the selected scalar precision.
#endif

   /// @brief Returns the additive identity for the configured scalar type.
   [[nodiscard]] inline constexpr Scalar zero() noexcept { return static_cast<Scalar>(0); }

   /// @brief Returns the multiplicative identity for the configured scalar type.
   [[nodiscard]] inline constexpr Scalar one() noexcept { return static_cast<Scalar>(1); }

   /// @brief Returns a 4x4 identity matrix.
   [[nodiscard]] inline Mat4 identityMat4() noexcept { return Mat4(one()); }

   /// @brief Returns a vector with all components set to one.
   [[nodiscard]] inline Vec3 oneVec3() noexcept { return Vec3(one(), one(), one()); }

   /// @brief Returns a zero-initialized three-component vector.
   [[nodiscard]] inline Vec3 zeroVec3() noexcept { return Vec3(zero(), zero(), zero()); }

   /// @brief Returns the identity quaternion.
   [[nodiscard]] inline Quat identityQuat() noexcept { return Quat(one(), zero(), zero(), zero()); }

   /// @brief Multiplies two quaternions without exposing GLM at call sites.
   [[nodiscard]] inline Quat multiply(const Quat &lhs, const Quat &rhs) noexcept {
      // Keep this wrapper explicit so engine-facing code does not depend on
      // GLM operator conventions at call sites.
      return Quat(lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
                  lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
                  lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
                  lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w);
   }

   /// @brief Multiplies two 4x4 matrices without exposing GLM at call sites.
   [[nodiscard]] inline Mat4 multiply(const Mat4 &lhs, const Mat4 &rhs) { return lhs * rhs; }

   /// @brief Subtracts one 3D vector from another without exposing GLM operators at call sites.
   [[nodiscard]] inline Vec3 subtract(const Vec3 &lhs, const Vec3 &rhs) { return lhs - rhs; }

   /// @brief Returns `matrix` translated by `offset`.
   [[nodiscard]] inline Mat4 translate(const Mat4 &matrix, const Vec3 &offset) {
      return glm::translate(matrix, offset);
   }

   /// @brief Returns `matrix` scaled by `factors`.
   [[nodiscard]] inline Mat4 scale(const Mat4 &matrix, const Vec3 &factors) { return glm::scale(matrix, factors); }

   /// @brief Builds a right-handed world-to-view transform looking from `eye` toward `center`.
   [[nodiscard]] inline Mat4 lookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up) {
      return glm::lookAt(eye, center, up);
   }

   /// @brief Builds a perspective projection matrix.
   [[nodiscard]] inline Mat4 perspective(Scalar field_of_view_radians, Scalar aspect_ratio, Scalar near_plane,
                                         Scalar far_plane) {
      return glm::perspective(field_of_view_radians, aspect_ratio, near_plane, far_plane);
   }

   /// @brief Returns the inverse of `matrix`.
   [[nodiscard]] inline Mat4 inverse(const Mat4 &matrix) { return glm::inverse(matrix); }

} // namespace vve::math
