export module VEEngine:Math;
import std;
import VEEngine.V4;

/**
 * @file
 * @brief Public math contract backed by the selected engine implementation.
 */
export namespace vve::math {

   using Scalar = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::Scalar; ///< Facade scalar type.
   using Vec2   = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::Vec2;   ///< Facade 2D vector.
   using Vec3   = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::Vec3;   ///< Facade 3D vector.
   using Vec4   = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::Vec4;   ///< Facade 4D vector.
   using Quat   = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::Quat;   ///< Facade quaternion.
   using Mat4   = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::Mat4;   ///< Facade 4x4 matrix.

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
   using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::scale;
   using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::subtract;
   using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::translate;
   using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::zero;
   using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::math::zeroVec3;

   template <typename T> concept ScalarLike = std::same_as<std::remove_cvref_t<T>, Scalar>; ///< Scalar contract.
   template <typename T> concept Vec2Like   = std::same_as<std::remove_cvref_t<T>, Vec2>;   ///< 2D vector contract.
   template <typename T> concept Vec3Like   = std::same_as<std::remove_cvref_t<T>, Vec3>;   ///< 3D vector contract.
   template <typename T> concept Vec4Like   = std::same_as<std::remove_cvref_t<T>, Vec4>;   ///< 4D vector contract.
   template <typename T> concept QuatLike   = std::same_as<std::remove_cvref_t<T>, Quat>;   ///< Quaternion contract.
   template <typename T> concept Mat4Like   = std::same_as<std::remove_cvref_t<T>, Mat4>;   ///< 4x4 matrix contract.

   template <typename = void> concept ZeroFunctionLike = requires { { zero() } -> std::same_as<Scalar>; };
   template <typename = void> concept OneFunctionLike = requires { { one() } -> std::same_as<Scalar>; };
   template <typename = void> concept IdentityQuatFunctionLike = requires {
      { identityQuat() } -> std::same_as<Quat>;
   };
   template <typename = void> concept IdentityMat4FunctionLike = requires {
      { identityMat4() } -> std::same_as<Mat4>;
   };
   template <typename = void> concept UnitVectorFunctionLike = requires {
      { zeroVec3() } -> std::same_as<Vec3>;
      { oneVec3() } -> std::same_as<Vec3>;
   };
   template <typename = void> concept ArithmeticFunctionLike =
      requires(Vec2 a2, Vec2 b2, Vec3 a3, Vec3 b3, Vec4 a4, Vec4 b4, Scalar scalar) {
         { add(a2, b2) } -> std::same_as<Vec2>;
         { add(a3, b3) } -> std::same_as<Vec3>;
         { add(a4, b4) } -> std::same_as<Vec4>;
         { subtract(a2, b2) } -> std::same_as<Vec2>;
         { subtract(a3, b3) } -> std::same_as<Vec3>;
         { subtract(a4, b4) } -> std::same_as<Vec4>;
         { scale(a2, scalar) } -> std::same_as<Vec2>;
         { scale(a3, scalar) } -> std::same_as<Vec3>;
         { scale(a4, scalar) } -> std::same_as<Vec4>;
      };
   template <typename = void> concept GeometryFunctionLike = requires(Vec3 a, Vec3 b, Mat4 m, Scalar scalar) {
      { dot(a, b) } -> std::same_as<Scalar>;
      { cross(a, b) } -> std::same_as<Vec3>;
      { length(a) } -> std::same_as<Scalar>;
      { lengthSquared(a) } -> std::same_as<Scalar>;
      { normalize(a) } -> std::same_as<Vec3>;
      { lookAt(a, b, a) } -> std::same_as<Mat4>;
      { perspective(scalar, scalar, scalar, scalar) } -> std::same_as<Mat4>;
      { translate(m, a) } -> std::same_as<Mat4>;
      { inverse(m) } -> std::same_as<Mat4>;
   };
   template <typename = void> concept ComparisonFunctionLike =
      requires(Scalar scalar, Vec3 a, Vec3 b) {
         { min(scalar, scalar) } -> std::same_as<Scalar>;
         { max(scalar, scalar) } -> std::same_as<Scalar>;
         { min(a, b) } -> std::same_as<Vec3>;
         { max(a, b) } -> std::same_as<Vec3>;
         { clamp(scalar, scalar, scalar) } -> std::same_as<Scalar>;
         { clamp(a, b, a) } -> std::same_as<Vec3>;
      };
   template <typename = void> concept MultiplyFunctionLike = requires(Quat q, Mat4 m) {
      { multiply(q, q) } -> std::same_as<Quat>;
      { multiply(m, m) } -> std::same_as<Mat4>;
   };

} // namespace vve::math

export namespace vve {

   using Scalar = math::Scalar; ///< Facade scalar type.
   using Vec2   = math::Vec2;   ///< Facade 2D vector.
   using Vec3   = math::Vec3;   ///< Facade 3D vector.
   using Vec4   = math::Vec4;   ///< Facade 4D vector.
   using Quat   = math::Quat;   ///< Facade quaternion.
   using Mat4   = math::Mat4;   ///< Facade 4x4 matrix.

   using math::identityMat4;
   using math::identityQuat;
   using math::one;
   using math::oneVec3;
   using math::zero;
   using math::zeroVec3;

   template <typename T> concept ScalarLike = math::ScalarLike<T>; ///< Scalar contract.
   template <typename T> concept Vec2Like   = math::Vec2Like<T>;   ///< 2D vector contract.
   template <typename T> concept Vec3Like   = math::Vec3Like<T>;   ///< 3D vector contract.
   template <typename T> concept Vec4Like   = math::Vec4Like<T>;   ///< 4D vector contract.
   template <typename T> concept QuatLike   = math::QuatLike<T>;   ///< Quaternion contract.
   template <typename T> concept Mat4Like   = math::Mat4Like<T>;   ///< 4x4 matrix contract.

} // namespace vve
