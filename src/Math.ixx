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

/**
 * @file
 * @brief Math and geometry facade used by every engine version.
 *
 * Engine-facing code should depend on this module instead of raw GLM types so
 * the scalar precision policy and common geometry value types stay unified.
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

export namespace vve {

   /// @brief Strong wrapper for world or local position values.
   struct Position {
      math::Vec3 value{math::zeroVec3()}; ///< Wrapped coordinate.
   };

   /// @brief Strong wrapper for vectors that should be interpreted as directions.
   struct Direction {
      math::Vec3 value{math::Vec3(math::zero(), math::zero(), -math::one())}; ///< Wrapped direction.
   };

   /// @brief Strong wrapper for non-uniform scale factors.
   struct Scale {
      math::Vec3 value{math::oneVec3()}; ///< Wrapped scale vector.
   };

   /// @brief Strong wrapper for quaternion rotations.
   struct Rotation {
      math::Quat value{math::identityQuat()}; ///< Wrapped orientation.
   };

   /// @brief Standard transform component shared by all engine versions.
   struct Transform {
      math::Vec3 translation{math::zeroVec3()}; ///< Local or world-space translation.
      math::Quat rotation{math::identityQuat()}; ///< Local or world-space orientation.
      math::Vec3 scale{math::oneVec3()};        ///< Local or world-space non-uniform scale.
   };

   /// @brief Axis-aligned bounds described by minimum and maximum positions.
   struct Bounds {
      Position minimum{}; ///< Minimum corner.
      Position maximum{}; ///< Maximum corner.
      bool valid{false};  ///< False until at least one point has been included.
   };

   /// @brief Public camera description used by game code and renderers.
   struct Camera {
      math::Vec3 position{math::Vec3(math::zero(), static_cast<math::Scalar>(1.5),
                                     static_cast<math::Scalar>(6.0))}; ///< World-space camera position.
      math::Mat4 view_transform{math::translate(
          math::identityMat4(),
          math::Vec3(math::zero(), static_cast<math::Scalar>(-1.5),
                     static_cast<math::Scalar>(-6.0)))}; ///< World-to-view transform.
      math::Scalar vertical_fov_radians{static_cast<math::Scalar>(1.0471975511965976)}; ///< Vertical FOV.
      math::Scalar near_plane{static_cast<math::Scalar>(0.1)}; ///< Near clip plane.
      math::Scalar far_plane{static_cast<math::Scalar>(10000.0)}; ///< Far clip plane.

      /// @brief Builds a camera from an eye position and target point.
      [[nodiscard]] static Camera lookAt(const math::Vec3 &position, const math::Vec3 &target,
                                         const math::Vec3 &up = math::Vec3(math::zero(), math::one(), math::zero()),
                                         math::Scalar vertical_fov_radians =
                                             static_cast<math::Scalar>(1.0471975511965976),
                                         math::Scalar near_plane = static_cast<math::Scalar>(0.1),
                                         math::Scalar far_plane = static_cast<math::Scalar>(10000.0)) {
         Camera camera{};
         camera.position = position;
         camera.view_transform = math::lookAt(position, target, up);
         camera.vertical_fov_radians = vertical_fov_radians;
         camera.near_plane = near_plane;
         camera.far_plane = far_plane;
         return camera;
      }
   };

} // namespace vve
