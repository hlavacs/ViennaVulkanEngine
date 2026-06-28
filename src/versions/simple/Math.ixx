export module VEEngine.Simple.Math;
export import VEEngine.V5.Math;

/**
	* @file
	* @brief Simple-engine math aliases backed by the v5 GLM-based math helper.
	*
	* Functional objects:
	* - Scalar, Vec2, Vec3, Vec4, Quat, and Mat4 name the shared math value surface.
	* - Arithmetic, transform, camera matrix, and projection helpers forward to v5 math.
	* - Light and shadow code should use these shared values without adding renderer data here.
	*
	* The simple engine reuses `VEEngine.V5.Math` directly. Scalar policy, GLM storage, and
	* matrix/vector operations remain owned by the v5 module; this module only names the surface.
	*/
export namespace vve::simple {

	using Scalar = vve::v5::math::Scalar; ///< Shared scalar type for transforms, cameras, lights, and shadows.
	using Vec2 = vve::v5::math::Vec2;     ///< Shared 2D vector type for screen and texture-space values.
	using Vec3 = vve::v5::math::Vec3;     ///< Shared 3D vector type for positions, directions, colors, and scales.
	using Vec4 = vve::v5::math::Vec4;     ///< Shared 4D vector type for homogeneous coordinates and packed values.
	using Quat = vve::v5::math::Quat;     ///< Shared quaternion type for transform rotations.
	using Mat4 = vve::v5::math::Mat4;     ///< Shared 4x4 matrix type for world, view, projection, and light-space data.

	using vve::v5::math::add;               ///< Adds shared scalar/vector values where the v5 overload exists.
	using vve::v5::math::clamp;             ///< Clamps scalar/vector values with v5 semantics.
	using vve::v5::math::cross;             ///< Computes a shared 3D cross product.
	using vve::v5::math::dot;               ///< Computes shared scalar/vector dot products.
	using vve::v5::math::identityMat4;      ///< Returns the shared identity matrix.
	using vve::v5::math::identityQuat;      ///< Returns the shared identity rotation.
	using vve::v5::math::inverse;           ///< Computes an inverse matrix for camera or light-space transforms.
	using vve::v5::math::length;            ///< Computes vector length.
	using vve::v5::math::lengthSquared;     ///< Computes squared vector length.
	using vve::v5::math::lookAt;            ///< Builds a camera or light view matrix.
	using vve::v5::math::max;               ///< Returns component-wise or scalar maximum values.
	using vve::v5::math::min;               ///< Returns component-wise or scalar minimum values.
	using vve::v5::math::multiply;          ///< Multiplies matrices, vectors, or quaternions through v5 overloads.
	using vve::v5::math::normalize;         ///< Normalizes shared direction vectors.
	using vve::v5::math::one;               ///< Returns scalar one in the selected v5 scalar policy.
	using vve::v5::math::oneVec3;           ///< Returns a 3D one vector for default scales.
	using vve::v5::math::perspective;       ///< Builds a projection matrix with v5 math semantics.
	using vve::v5::math::perspectiveVulkan; ///< Builds a Vulkan-space projection matrix.
	using vve::v5::math::scale;             ///< Scales vectors or matrices through v5 overloads.
	using vve::v5::math::subtract;          ///< Subtracts shared vector values.
	using vve::v5::math::translate;         ///< Applies a translation to a shared matrix.
	using vve::v5::math::zero;              ///< Returns scalar zero in the selected v5 scalar policy.
	using vve::v5::math::zeroVec3;          ///< Returns a 3D zero vector for default positions.

} // namespace vve::simple
