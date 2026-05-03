module;

#define GLM_ENABLE_EXPERIMENTAL
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

   /// @brief Adds two 2D vectors without exposing GLM operators at call sites.
   [[nodiscard]] inline Vec2 add(const Vec2 &lhs, const Vec2 &rhs) { return lhs + rhs; }

   /// @brief Adds two 3D vectors without exposing GLM operators at call sites.
   [[nodiscard]] inline Vec3 add(const Vec3 &lhs, const Vec3 &rhs) { return lhs + rhs; }

   /// @brief Adds two 4D vectors without exposing GLM operators at call sites.
   [[nodiscard]] inline Vec4 add(const Vec4 &lhs, const Vec4 &rhs) { return lhs + rhs; }

   /// @brief Subtracts one 2D vector from another without exposing GLM operators at call sites.
   [[nodiscard]] inline Vec2 subtract(const Vec2 &lhs, const Vec2 &rhs) { return lhs - rhs; }

   /// @brief Subtracts one 3D vector from another without exposing GLM operators at call sites.
   [[nodiscard]] inline Vec3 subtract(const Vec3 &lhs, const Vec3 &rhs) { return lhs - rhs; }

   /// @brief Subtracts one 4D vector from another without exposing GLM operators at call sites.
   [[nodiscard]] inline Vec4 subtract(const Vec4 &lhs, const Vec4 &rhs) { return lhs - rhs; }

   /// @brief Scales a 2D vector by a scalar factor.
   [[nodiscard]] inline Vec2 scale(const Vec2 &value, Scalar factor) { return value * factor; }

   /// @brief Scales a 3D vector by a scalar factor.
   [[nodiscard]] inline Vec3 scale(const Vec3 &value, Scalar factor) { return value * factor; }

   /// @brief Scales a 4D vector by a scalar factor.
   [[nodiscard]] inline Vec4 scale(const Vec4 &value, Scalar factor) { return value * factor; }

   /// @brief Returns the dot product of two 2D vectors.
   [[nodiscard]] inline Scalar dot(const Vec2 &lhs, const Vec2 &rhs) { return glm::dot(lhs, rhs); }

   /// @brief Returns the dot product of two 3D vectors.
   [[nodiscard]] inline Scalar dot(const Vec3 &lhs, const Vec3 &rhs) { return glm::dot(lhs, rhs); }

   /// @brief Returns the dot product of two 4D vectors.
   [[nodiscard]] inline Scalar dot(const Vec4 &lhs, const Vec4 &rhs) { return glm::dot(lhs, rhs); }

   /// @brief Returns the cross product of two 3D vectors.
   [[nodiscard]] inline Vec3 cross(const Vec3 &lhs, const Vec3 &rhs) { return glm::cross(lhs, rhs); }

   /// @brief Returns the squared length of a 2D vector.
   [[nodiscard]] inline Scalar lengthSquared(const Vec2 &value) { return dot(value, value); }

   /// @brief Returns the squared length of a 3D vector.
   [[nodiscard]] inline Scalar lengthSquared(const Vec3 &value) { return dot(value, value); }

   /// @brief Returns the squared length of a 4D vector.
   [[nodiscard]] inline Scalar lengthSquared(const Vec4 &value) { return dot(value, value); }

   /// @brief Returns the Euclidean length of a 2D vector.
   [[nodiscard]] inline Scalar length(const Vec2 &value) { return glm::length(value); }

   /// @brief Returns the Euclidean length of a 3D vector.
   [[nodiscard]] inline Scalar length(const Vec3 &value) { return glm::length(value); }

   /// @brief Returns the Euclidean length of a 4D vector.
   [[nodiscard]] inline Scalar length(const Vec4 &value) { return glm::length(value); }

   /// @brief Returns a unit-length 2D vector pointing in the same direction.
   [[nodiscard]] inline Vec2 normalize(const Vec2 &value) { return glm::normalize(value); }

   /// @brief Returns a unit-length 3D vector pointing in the same direction.
   [[nodiscard]] inline Vec3 normalize(const Vec3 &value) { return glm::normalize(value); }

   /// @brief Returns a unit-length 4D vector pointing in the same direction.
   [[nodiscard]] inline Vec4 normalize(const Vec4 &value) { return glm::normalize(value); }

   /// @brief Returns the smaller scalar value.
   [[nodiscard]] inline constexpr Scalar min(Scalar lhs, Scalar rhs) noexcept { return lhs < rhs ? lhs : rhs; }

   /// @brief Returns the component-wise minimum of two 2D vectors.
   [[nodiscard]] inline Vec2 min(const Vec2 &lhs, const Vec2 &rhs) { return glm::min(lhs, rhs); }

   /// @brief Returns the component-wise minimum of two 3D vectors.
   [[nodiscard]] inline Vec3 min(const Vec3 &lhs, const Vec3 &rhs) { return glm::min(lhs, rhs); }

   /// @brief Returns the component-wise minimum of two 4D vectors.
   [[nodiscard]] inline Vec4 min(const Vec4 &lhs, const Vec4 &rhs) { return glm::min(lhs, rhs); }

   /// @brief Returns the larger scalar value.
   [[nodiscard]] inline constexpr Scalar max(Scalar lhs, Scalar rhs) noexcept { return lhs > rhs ? lhs : rhs; }

   /// @brief Returns the component-wise maximum of two 2D vectors.
   [[nodiscard]] inline Vec2 max(const Vec2 &lhs, const Vec2 &rhs) { return glm::max(lhs, rhs); }

   /// @brief Returns the component-wise maximum of two 3D vectors.
   [[nodiscard]] inline Vec3 max(const Vec3 &lhs, const Vec3 &rhs) { return glm::max(lhs, rhs); }

   /// @brief Returns the component-wise maximum of two 4D vectors.
   [[nodiscard]] inline Vec4 max(const Vec4 &lhs, const Vec4 &rhs) { return glm::max(lhs, rhs); }

   /// @brief Clamps a scalar value into an inclusive range.
   [[nodiscard]] inline constexpr Scalar clamp(Scalar value, Scalar low, Scalar high) noexcept {
      return max(low, min(value, high));
   }

   /// @brief Clamps a 3D vector component-wise into an inclusive range.
   [[nodiscard]] inline Vec3 clamp(const Vec3 &value, const Vec3 &low, const Vec3 &high) {
      return glm::clamp(value, low, high);
   }

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
