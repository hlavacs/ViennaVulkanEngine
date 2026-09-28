module VEEngine.Simple.Renderer;
import std;
import VEEngine.Simple.Types;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

/// @file
/// @brief ForwardRenderer CPU shadow preparation: packs contributing spot, point, and directional lights and builds matrices only for shadow casters.

namespace vve::simple {

	/**
		* @brief Fills the frame uniforms directly with camera data, packed lights and shadow matrices.
		*
		* @param cameraView Current world-to-view camera matrix.
		* @param cameraVerticalFov Current vertical camera field of view in radians.
		* @param cameraAspect Current camera viewport aspect ratio.
		* @param cameraNear Current camera near clipping distance.
		* @param cameraFar Current camera far clipping distance.
		*/
	void ForwardRenderer::prepareShadowFrame(const Mat4 &cameraView, Scalar cameraVerticalFov, Scalar cameraAspect, Scalar cameraNear, Scalar cameraFar) {
		frameUniforms_ = {.view = cameraView,
			.projection = perspectiveVulkan(cameraVerticalFov, cameraAspect, cameraNear, cameraFar),
			.ambient = static_cast<float>(scene.ambient)};
		shadowLightMeta.clear();
		const auto addMeta = [this](std::uint32_t type, std::size_t packedIndex, std::uint32_t firstLayer, const Mat4 &view, const Mat4 &projection, Scalar nearPlane, Scalar farPlane) {
			shadowLightMeta.push_back({.light_index = static_cast<std::uint32_t>(packedIndex), .light_type = type,
														 .first_layer = firstLayer, .view = view, .projection = projection,
														 .near_plane = nearPlane, .far_plane = farPlane});
		};
		const auto positiveRange = [](Scalar range) { return std::isfinite(range) && range > zero() ? range : static_cast<Scalar>(0.001); };

		// Pack lit or ambient-only spots; only positive intensity needs a shadow projection.
		for (const ForwardSpotLight &spot : scene.spotLights | std::views::take(kMaxShadowedSpotLights)) {
			if (!spot.enabled || (spot.intensity.value <= zero() && spot.ambient <= zero())) { continue; }
			const std::size_t packed{frameUniforms_.activeSpotLightCount++};
			const Vec3 direction{normalize(spot.direction)};
			frameUniforms_.spotLightPositionRanges[packed] = Vec4{spot.position.x, spot.position.y, spot.position.z, spot.range.value};
			frameUniforms_.spotLightColorIntensities[packed] = Vec4{spot.color.x, spot.color.y, spot.color.z, spot.intensity.value};
			frameUniforms_.spotLightDirections[packed] = Vec4{direction.x, direction.y, direction.z, spot.intensity.value <= zero() ? one() : zero()};
			// The shader's smoothstep(cos outer, cos inner) needs cos inner > cos outer, i.e. an inner cone narrower than the outer one.
			const Scalar cosOuter{std::cos(spot.outerConeAngle.radians)};
			const Scalar cosInner{std::max(std::cos(spot.innerConeAngle.radians), cosOuter + static_cast<Scalar>(0.0001))};
			frameUniforms_.spotLightConeAmbients[packed] = Vec4{cosInner, cosOuter, zero(), spot.ambient};
			if (spot.intensity.value <= zero()) { continue; }
			const Scalar fov{std::clamp(spot.outerConeAngle.radians * static_cast<Scalar>(2), static_cast<Scalar>(0.001), static_cast<Scalar>(3.0))};
			const Scalar farPlane{positiveRange(spot.range.value)};
			const Mat4 view{lookAt(spot.position, add(spot.position, direction), detail::stableUp(direction))};
			const Mat4 projection{perspectiveVulkan(fov, one(), shadowNearPlane, farPlane)};
			frameUniforms_.shadowViewProjs[kShadowMatrixSpotBase + packed] = multiply(projection, view);
			addMeta(1U, packed, static_cast<std::uint32_t>(packed), view, projection, shadowNearPlane, farPlane);
		}

		// Six independent light-space views per enabled point light with positive intensity.
		constexpr std::array<Vec3, pointShadowFaceCount> faceDirections{Vec3{one(), zero(), zero()}, Vec3{-one(), zero(), zero()}, Vec3{zero(), one(), zero()},
																							 Vec3{zero(), -one(), zero()}, Vec3{zero(), zero(), one()}, Vec3{zero(), zero(), -one()}};
		constexpr std::array<Vec3, pointShadowFaceCount> faceUps{Vec3{zero(), -one(), zero()}, Vec3{zero(), -one(), zero()}, Vec3{zero(), zero(), one()},
																					Vec3{zero(), zero(), -one()}, Vec3{zero(), -one(), zero()}, Vec3{zero(), -one(), zero()}};
		constexpr Scalar pointShadowFov{static_cast<Scalar>(1.6057029118347832)}; ///< Two-degree face overlap keeps PCF away from cube seams.
		for (const ForwardPointLight &point : scene.pointLights | std::views::take(kMaxShadowedPointLights)) {
			if (!point.enabled || point.intensity <= zero()) { continue; }
			const std::size_t packed{frameUniforms_.activePointLightCount++};
			const Scalar farPlane{positiveRange(point.range)};
			const Mat4 projection{perspectiveVulkan(pointShadowFov, one(), shadowNearPlane, farPlane)};
			for (std::size_t face{}; face < pointShadowFaceCount; ++face) {
				const Mat4 view{lookAt(point.position, add(point.position, faceDirections[face]), faceUps[face])};
				const std::size_t layer{packed * pointShadowFaceCount + face};
				frameUniforms_.shadowViewProjs[kShadowMatrixPointBase + layer] = multiply(projection, view);
				addMeta(2U, packed, static_cast<std::uint32_t>(layer), view, projection, shadowNearPlane, farPlane);
			}
			frameUniforms_.pointLightPositionRanges[packed] = Vec4{point.position.x, point.position.y, point.position.z, point.range};
			frameUniforms_.pointLightColorIntensities[packed] = Vec4{point.color.x, point.color.y, point.color.z, point.intensity};
		}

		const Scalar safeCameraNear{max(cameraNear, static_cast<Scalar>(0.001))}; ///< Positive near plane keeps logarithmic splits finite.
		const Scalar safeCameraFar{max(cameraFar, safeCameraNear + static_cast<Scalar>(0.001))}; ///< Ordered far plane limits cascade coverage.
		const Scalar cascadeShadowFar{max(safeCameraNear + static_cast<Scalar>(0.001), min(shadowDistance, safeCameraFar))}; ///< Directional shadows stop at the configured distance or camera far plane.
		constexpr Scalar splitLambda{static_cast<Scalar>(0.5)}; ///< Practical splits blend uniform and logarithmic spacing evenly.
		const Scalar splitRatio{cascadeShadowFar / safeCameraNear}; ///< Logarithmic split ratio shared by all four cascades.
		for (std::size_t cascadeIndex{}; cascadeIndex < kNumShadowCascades; ++cascadeIndex) {
			const Scalar fraction{static_cast<Scalar>(cascadeIndex + 1U) / static_cast<Scalar>(kNumShadowCascades)}; ///< Normalized far boundary for this cascade.
			const Scalar uniformSplit{safeCameraNear + (cascadeShadowFar - safeCameraNear) * fraction}; ///< Evenly spaced cascade far distance.
			const Scalar logarithmicSplit{safeCameraNear * std::pow(splitRatio, fraction)}; ///< Perspective-weighted cascade far distance.
			frameUniforms_.cascadeSplits[cascadeIndex] = uniformSplit * (one() - splitLambda) + logarithmicSplit * splitLambda;
		}

		const Mat4 cameraInverseView{inverse(cameraView)}; ///< Camera-to-world transform reconstructs frustum corners without exposing camera internals.
		const Scalar safeAspect{max(cameraAspect, static_cast<Scalar>(0.001))}; ///< Positive aspect keeps horizontal frustum extents valid.
		const Scalar halfFovTangent{static_cast<Scalar>(std::tan(clamp(cameraVerticalFov, static_cast<Scalar>(0.001), static_cast<Scalar>(3.13)) * static_cast<Scalar>(0.5)))}; ///< Vertical ray spread for camera-space corners.
		const std::size_t directionalLightCount{std::min(scene.directionalLights.size(), kMaxDirectionalLights)}; ///< Clamped directional-light count fits fixed arrays.
		// Build one stable light-space matrix for every cascade of each active directional light.
		for (std::size_t directionalIndex{}; directionalIndex < directionalLightCount; ++directionalIndex) {
			const ForwardDirectionalLight &activeDirectional = scene.directionalLights[directionalIndex]; ///< Scene directional light copied into a fixed uniform slot.
			if (!activeDirectional.enabled || (activeDirectional.intensity.value <= zero() && activeDirectional.ambient <= zero())) { continue; }
			const std::size_t packedDirectionalIndex{frameUniforms_.activeDirectionalLightCount++}; ///< Dense shader slot for this enabled directional light.
			const Vec3 activeDirectionalDirection{normalize(activeDirectional.direction)}; ///< Normalized direction mirrors the legacy single-light path.
			frameUniforms_.directionalLightDirections[packedDirectionalIndex] = Vec4{activeDirectionalDirection.x, activeDirectionalDirection.y, activeDirectionalDirection.z, activeDirectional.intensity.value <= zero() ? one() : zero()};
			frameUniforms_.directionalLightColorIntensities[packedDirectionalIndex] = Vec4{activeDirectional.color.x, activeDirectional.color.y, activeDirectional.color.z, activeDirectional.intensity.value};
			frameUniforms_.directionalLightAmbients[packedDirectionalIndex] = Vec4{zero(), zero(), zero(), activeDirectional.ambient};
			if (activeDirectional.intensity.value <= zero()) { continue; }
			Scalar splitNear{safeCameraNear}; ///< Each cascade begins at the previous practical split.
			for (std::size_t cascadeIndex{}; cascadeIndex < kNumShadowCascades; ++cascadeIndex) {
				const Scalar splitFar{frameUniforms_.cascadeSplits[cascadeIndex]}; ///< Current camera-space far boundary.
				std::array<Vec3, 8U> frustumCorners{}; ///< World-space corners of this camera sub-frustum.
				std::size_t cornerIndex{}; ///< Dense destination index across two planes and four corners.
				// Reconstruct near and far plane corners in camera space, then move them into world space.
				for (const Scalar distance : std::array{splitNear, splitFar}) {
					const Scalar halfHeight{halfFovTangent * distance}; ///< Half-height of this frustum plane.
					const Scalar halfWidth{halfHeight * safeAspect}; ///< Half-width follows the live viewport aspect.
					for (const Scalar ySign : std::array{-one(), one()}) {
						for (const Scalar xSign : std::array{-one(), one()}) {
							const Vec4 worldCorner{multiply(cameraInverseView, Vec4{xSign * halfWidth, ySign * halfHeight, -distance, one()})}; ///< Camera looks down negative view-space Z.
							const Scalar inverseW{worldCorner.w != zero() ? one() / worldCorner.w : one()}; ///< Rigid camera transforms ordinarily keep w at one.
							frustumCorners[cornerIndex++] = Vec3{worldCorner.x * inverseW, worldCorner.y * inverseW, worldCorner.z * inverseW};
						}
					}
				}

				Vec3 sphereCenter{zeroVec3()}; ///< Average corner position provides a stable bounding-sphere center.
				for (const Vec3 &corner : frustumCorners) { sphereCenter = add(sphereCenter, corner); }
				sphereCenter = scale(sphereCenter, one() / static_cast<Scalar>(frustumCorners.size()));
				Scalar cascadeRadius{}; ///< Maximum center-to-corner distance encloses the whole sub-frustum.
				for (const Vec3 &corner : frustumCorners) { cascadeRadius = max(cascadeRadius, length(subtract(corner, sphereCenter))); }
				cascadeRadius = max(static_cast<Scalar>(0.0625), std::ceil(cascadeRadius * static_cast<Scalar>(16.0)) / static_cast<Scalar>(16.0)); ///< Quantized radius prevents projection-scale shimmer.

				const Vec3 lightEye{subtract(sphereCenter, scale(activeDirectionalDirection, cascadeRadius + zBackoff))}; ///< Backoff includes casters behind the visible slice.
				const Mat4 lightView{lookAt(lightEye, sphereCenter, detail::stableUp(activeDirectionalDirection))}; ///< World up, or +Z for a vertical light.
				const Scalar lightFar{static_cast<Scalar>(2.0) * (cascadeRadius + zBackoff)}; ///< Symmetric depth coverage encloses the sphere and backoff volume.
				Mat4 lightProjection{orthoVulkan(-cascadeRadius, cascadeRadius, -cascadeRadius, cascadeRadius, shadowNearPlane, lightFar)}; ///< Existing helper applies Vulkan clip-space Y orientation.
				const Vec4 shadowOrigin{scale(multiply(multiply(lightProjection, lightView), Vec4{zero(), zero(), zero(), one()}), static_cast<Scalar>(ShadowMap::resolution) * static_cast<Scalar>(0.5))}; ///< World origin measured in shadow texels.
				const Scalar shadowMapExtent{static_cast<Scalar>(2.0) * cascadeRadius}; ///< World-space width covered by this cascade.
				const Scalar texelSize{shadowMapExtent / static_cast<Scalar>(ShadowMap::resolution)}; ///< World-space size of one shadow texel.
				const Scalar translationScale{texelSize / cascadeRadius}; ///< Converts a rounded texel delta back to clip-space translation.
				lightProjection[3][0] += (std::round(shadowOrigin.x) - shadowOrigin.x) * translationScale;
				lightProjection[3][1] += (std::round(shadowOrigin.y) - shadowOrigin.y) * translationScale;

				const std::size_t cascadeLayer{packedDirectionalIndex * kNumShadowCascades + cascadeIndex}; ///< Flattened matrix and image-array layer index.
				frameUniforms_.shadowViewProjs[kShadowMatrixDirBase + cascadeLayer] = multiply(lightProjection, lightView);
				shadowLightMeta.push_back({.light_index = static_cast<std::uint32_t>(packedDirectionalIndex),
												 .light_type = 3U,
												 .first_layer = static_cast<std::uint32_t>(cascadeLayer),
												 .view = lightView,
												 .projection = lightProjection,
												 .near_plane = shadowNearPlane,
												 .far_plane = lightFar}); ///< One metadata row identifies each cascade layer.
				splitNear = splitFar;
			}
		}
	}

} // namespace vve::simple
