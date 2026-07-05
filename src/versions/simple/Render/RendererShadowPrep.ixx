export module VEEngine.Simple.Renderer:RendererShadowPrep;
import std;
import VEEngine.Types;
import VEEngine.Simple.Math;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

/**
	* @file
	* @brief CPU-side shadow matrix and light metadata preparation for the simple forward renderer.
	*
	* Functional objects:
	* - ForwardRendererShadowFrame carries fixed-size shadow uniform arrays prepared from the current scene.
	* - ForwardRendererShadowPrep builds spot, point, and directional shadow view/projection data for ForwardRenderer.
	*/
export namespace vve::simple {

	/// @brief Prepared CPU shadow data copied into the frame uniform buffer.
	struct ForwardRendererShadowFrame {
		std::array<Mat4, kMaxDirectionalLights> dirLightViewProjArray{}; ///< Per-directional light-space matrices.
		std::array<Mat4, kMaxShadowedPointLights * 6U> pointLightFaceViewProjs{}; ///< Per-point-face light-space matrices.
		std::array<Vec4, kMaxShadowedPointLights> pointLightPositionRanges{}; ///< Point xyz positions with ranges in w.
		std::array<Vec4, kMaxShadowedPointLights> pointLightColorIntensities{}; ///< Point rgb colors with intensities in w.
		Vec4 spotLightPositionRange{}; ///< Legacy spot xyz position with range in w.
		Vec4 spotLightColorIntensity{}; ///< Legacy spot rgb color with intensity in w.
		Vec4 spotLightDirection{}; ///< Legacy spot xyz direction with unused w.
		Vec4 spotLightConeAmbient{}; ///< Legacy spot cone cosines, active count, and ambient.
		std::array<Vec4, kMaxShadowedSpotLights> spotLightPositionRanges{}; ///< Per-spot xyz positions with ranges in w.
		std::array<Vec4, kMaxShadowedSpotLights> spotLightColorIntensities{}; ///< Per-spot rgb colors with intensities in w.
		std::array<Vec4, kMaxShadowedSpotLights> spotLightDirections{}; ///< Per-spot xyz directions with unused w.
		std::array<Vec4, kMaxShadowedSpotLights> spotLightConeAmbients{}; ///< Per-spot cone cosines, active count, and ambient.
		std::array<Vec4, kMaxDirectionalLights> directionalLightDirections{}; ///< Per-directional xyz directions with unused w.
		std::array<Vec4, kMaxDirectionalLights> directionalLightColorIntensities{}; ///< Per-directional rgb colors with intensities in w.
		std::array<Vec4, kMaxDirectionalLights> directionalLightAmbients{}; ///< Per-directional active count and ambient.
		std::uint32_t activeDirectionalLightCount{}; ///< Enabled directional light count packed into the frame uniform.
		std::size_t pointLightShadowCount{}; ///< Clamped point-light count used by debug sample preparation.
		std::uint32_t firstPointShadowLayer{}; ///< First point-shadow layer after the spot-shadow layers.
		Scalar shadowNearPlane{}; ///< Shared near plane for spot and point shadow views.
		float shadowCompareBias{}; ///< CPU mirror of the shader-side compare bias.
	};

	/// @brief CRTP mixin that prepares CPU shadow matrices and metadata for the concrete forward renderer.
	template<typename Renderer>
	struct ForwardRendererShadowPrep {
		static constexpr std::size_t pointShadowFaceCount{6U}; ///< One square face per cubemap direction.

		/**
			* @brief Rebuilds all CPU shadow view/projection data from the current renderer scene.
			*
			* @param lightCenter World-space center covered by directional shadow cameras.
			* @param lightExtent Directional orthographic half-extent.
			* @param spot Legacy single spot light mirrored into the fixed uniform fields.
			* @param spotDirection Normalized legacy spot direction.
			* @return Prepared shadow arrays and counts for the frame uniform and retained diagnostics.
			*/
		[[nodiscard]] ForwardRendererShadowFrame prepareShadowFrame(Vec3 lightCenter, Scalar lightExtent, const SpotLight &spot, Vec3 spotDirection) {
			auto &renderer = static_cast<Renderer &>(*this);
			auto frame = ForwardRendererShadowFrame{
				.spotLightPositionRange = Vec4{spot.position.x, spot.position.y, spot.position.z, spot.range.value},
				.spotLightColorIntensity = Vec4{spot.color.x, spot.color.y, spot.color.z, spot.intensity.value},
				.spotLightDirection = Vec4{spotDirection.x, spotDirection.y, spotDirection.z, zero()},
				.spotLightConeAmbient = Vec4{std::cos(spot.innerConeAngle.radians), std::cos(spot.outerConeAngle.radians), zero(), spot.ambient},
				.firstPointShadowLayer = static_cast<std::uint32_t>(kMaxShadowedSpotLights),
				.shadowNearPlane = static_cast<Scalar>(0.1),
				.shadowCompareBias = 0.001F};
			const Scalar spotLightFov{std::clamp(spot.outerConeAngle.radians * static_cast<Scalar>(2), static_cast<Scalar>(0.001), static_cast<Scalar>(3.0))}; ///< Legacy spot-light cone field of view.
			const Scalar spotLightFar{std::isfinite(spot.range.value) && spot.range.value > zero() ? spot.range.value : static_cast<Scalar>(0.001)}; ///< Legacy positive far plane.
			renderer.spotLightViewProjCount = std::min(renderer.scene.spotLights.size(), renderer.spotLightViewProjs.size());
			std::uint32_t activeSpotLightViewProjCount{}; ///< Enabled spot-light count packed into the frame uniform.
			frame.pointLightShadowCount = std::min(renderer.scene.pointLights.size(), kMaxShadowedPointLights);
			renderer.shadowLightMeta.clear();
			renderer.shadowLightMeta.reserve(renderer.spotLightViewProjCount + frame.pointLightShadowCount * pointShadowFaceCount);

			// Build one light-space projection row per enabled spot light.
			for (std::size_t spotIndex{}; spotIndex < renderer.spotLightViewProjCount; ++spotIndex) {
				const SpotLight &activeSpot = renderer.scene.spotLights[spotIndex]; ///< Scene spot light copied into a future shadow slot.
				if (!activeSpot.enabled) { continue; }
				const std::size_t packedSpotIndex{activeSpotLightViewProjCount++}; ///< Dense shader slot for this enabled spot light.
				const Vec3 activeSpotDirection{normalize(activeSpot.direction)}; ///< Normalized direction mirrors the legacy single-light path.
				const Scalar activeSpotLightFov{std::clamp(activeSpot.outerConeAngle.radians * static_cast<Scalar>(2), static_cast<Scalar>(0.001), static_cast<Scalar>(3.0))}; ///< Spot-light cone field of view.
				const Scalar activeSpotLightFar{std::isfinite(activeSpot.range.value) && activeSpot.range.value > zero() ? activeSpot.range.value : static_cast<Scalar>(0.001)}; ///< Positive spot-light far plane.
				const Mat4 activeSpotView{lookAt(activeSpot.position, add(activeSpot.position, activeSpotDirection), Vec3{zero(), one(), zero()})}; ///< Light-space view for this spot slot.
				const Mat4 activeSpotProjection{perspectiveVulkan(activeSpotLightFov, one(), frame.shadowNearPlane, activeSpotLightFar)}; ///< Light-space projection for this spot slot.
				renderer.spotLightViewProjs[packedSpotIndex] = multiply(activeSpotProjection, activeSpotView);
				renderer.shadowLightMeta.push_back({.light_index = static_cast<std::uint32_t>(packedSpotIndex),
															 .light_type = 1U,
															 .shadow_slot = static_cast<std::uint32_t>(packedSpotIndex),
															 .first_layer = static_cast<std::uint32_t>(packedSpotIndex),
															 .layer_count = 1U,
															 .view = activeSpotView,
															 .projection = activeSpotProjection,
															 .near_plane = frame.shadowNearPlane,
															 .far_plane = activeSpotLightFar,
															 .depth_bias = static_cast<Scalar>(frame.shadowCompareBias),
															 .resolution = ShadowMap::resolution}); ///< One spot light maps to one array layer.
				frame.spotLightPositionRanges[packedSpotIndex] = Vec4{activeSpot.position.x, activeSpot.position.y, activeSpot.position.z, activeSpot.range.value};
				frame.spotLightColorIntensities[packedSpotIndex] = Vec4{activeSpot.color.x, activeSpot.color.y, activeSpot.color.z, activeSpot.intensity.value};
				frame.spotLightDirections[packedSpotIndex] = Vec4{activeSpotDirection.x, activeSpotDirection.y, activeSpotDirection.z, zero()};
				frame.spotLightConeAmbients[packedSpotIndex] = Vec4{std::cos(activeSpot.innerConeAngle.radians), std::cos(activeSpot.outerConeAngle.radians), zero(), activeSpot.ambient};
			}
			frame.spotLightConeAmbient.z = static_cast<Scalar>(activeSpotLightViewProjCount);
			for (std::size_t spotIndex{}; spotIndex < activeSpotLightViewProjCount; ++spotIndex) { frame.spotLightConeAmbients[spotIndex].z = static_cast<Scalar>(activeSpotLightViewProjCount); }

			constexpr std::array<Vec3, pointShadowFaceCount> pointShadowFaceDirections{Vec3{one(), zero(), zero()}, Vec3{-one(), zero(), zero()},
																												Vec3{zero(), one(), zero()}, Vec3{zero(), -one(), zero()},
																												Vec3{zero(), zero(), one()}, Vec3{zero(), zero(), -one()}}; ///< Cubemap face forward vectors.
			constexpr std::array<Vec3, pointShadowFaceCount> pointShadowFaceUps{Vec3{zero(), -one(), zero()}, Vec3{zero(), -one(), zero()},
																										 Vec3{zero(), zero(), one()}, Vec3{zero(), zero(), -one()},
																										 Vec3{zero(), -one(), zero()}, Vec3{zero(), -one(), zero()}}; ///< Cubemap face up vectors.
			constexpr Scalar kPointShadowFov{static_cast<Scalar>(1.5707963267948966)}; ///< Square cubemap face field of view.
			std::size_t activePointLightCount{}; ///< Enabled point-light count packed into shader-visible arrays.

			// Append six independent light-space views for every shadowed point light.
			for (std::size_t pointIndex{}; pointIndex < frame.pointLightShadowCount; ++pointIndex) {
				const PointLight &activePoint = renderer.scene.pointLights[pointIndex]; ///< Scene point light copied into six shadow faces.
				if (!activePoint.enabled) { continue; }
				const std::size_t packedPointIndex{activePointLightCount++}; ///< Dense shader slot for this enabled point light.
				const Scalar activePointLightFar{std::isfinite(activePoint.range) && activePoint.range > zero() ? activePoint.range : static_cast<Scalar>(0.001)}; ///< Positive point-light far plane.
				const Mat4 activePointProjection{perspectiveVulkan(kPointShadowFov, one(), frame.shadowNearPlane, activePointLightFar)}; ///< Shared square projection for all faces.
				for (std::size_t faceIndex{}; faceIndex < pointShadowFaceCount; ++faceIndex) {
					const Vec3 faceTarget{add(activePoint.position, pointShadowFaceDirections[faceIndex])}; ///< Face center in world space.
					const Mat4 activePointView{lookAt(activePoint.position, faceTarget, pointShadowFaceUps[faceIndex])}; ///< Face-specific point-light view.
					renderer.shadowLightMeta.push_back({.light_index = static_cast<std::uint32_t>(packedPointIndex),
																	 .light_type = 2U,
																	 .shadow_slot = static_cast<std::uint32_t>(packedPointIndex),
																	 .first_layer = frame.firstPointShadowLayer + static_cast<std::uint32_t>(packedPointIndex * pointShadowFaceCount + faceIndex),
																	 .layer_count = 1U,
																	 .view = activePointView,
																	 .projection = activePointProjection,
																	 .near_plane = frame.shadowNearPlane,
																	 .far_plane = activePointLightFar,
																	 .depth_bias = static_cast<Scalar>(frame.shadowCompareBias),
																	 .resolution = ShadowMap::resolution}); ///< One point face maps to one array layer.
				}
			}

			// Copy clamped point-light properties into the fixed shader-visible arrays.
			std::size_t pointUniformIndex{}; ///< Dense uniform slot for enabled point lights.
			for (std::size_t pointIndex{}; pointIndex < frame.pointLightShadowCount; ++pointIndex) {
				const PointLight &activePoint = renderer.scene.pointLights[pointIndex]; ///< Scene point light copied into the frame uniform slot.
				if (!activePoint.enabled) { continue; }
				frame.pointLightPositionRanges[pointUniformIndex] = Vec4{activePoint.position.x, activePoint.position.y, activePoint.position.z, activePoint.range};
				frame.pointLightColorIntensities[pointUniformIndex] = Vec4{activePoint.color.x, activePoint.color.y, activePoint.color.z, activePoint.intensity};
				++pointUniformIndex;
			}
			// Mirror point-light metadata rows into the GPU uniform without rebuilding views or projections.
			for (const auto &shadowMeta : renderer.shadowLightMeta) {
				if (shadowMeta.light_type != 2U || shadowMeta.first_layer < frame.firstPointShadowLayer) { continue; }
				const std::uint32_t pointFaceIndex{shadowMeta.first_layer - frame.firstPointShadowLayer}; ///< Dense point-face array index derived from the metadata layer.
				if (pointFaceIndex < frame.pointLightFaceViewProjs.size()) { frame.pointLightFaceViewProjs[pointFaceIndex] = multiply(shadowMeta.projection, shadowMeta.view); }
			}

			const std::size_t directionalLightCount{std::min(renderer.scene.directionalLights.size(), kMaxDirectionalLights)}; ///< Clamped directional-light count fits fixed arrays.
			// Build one directional light-space matrix per active light.
			for (std::size_t directionalIndex{}; directionalIndex < directionalLightCount; ++directionalIndex) {
				const DirectionalLight &activeDirectional = renderer.scene.directionalLights[directionalIndex]; ///< Scene directional light copied into a fixed uniform slot.
				if (!activeDirectional.enabled) { continue; }
				const std::size_t packedDirectionalIndex{frame.activeDirectionalLightCount++}; ///< Dense shader slot for this enabled directional light.
				const Vec3 activeDirectionalDirection{normalize(activeDirectional.direction)}; ///< Normalized direction mirrors the legacy single-light path.
				const Vec3 activeDirectionalEye{subtract(lightCenter, scale(activeDirectionalDirection, lightExtent))}; ///< Directional shadow camera aimed at the scene origin.
				frame.dirLightViewProjArray[packedDirectionalIndex] = multiply(orthoVulkan(-lightExtent, lightExtent, -lightExtent, lightExtent, static_cast<Scalar>(0.1), static_cast<Scalar>(16.0)), lookAt(activeDirectionalEye, lightCenter, Vec3{zero(), one(), zero()}));
				frame.directionalLightDirections[packedDirectionalIndex] = Vec4{activeDirectionalDirection.x, activeDirectionalDirection.y, activeDirectionalDirection.z, zero()};
				frame.directionalLightColorIntensities[packedDirectionalIndex] = Vec4{activeDirectional.color.x, activeDirectional.color.y, activeDirectional.color.z, activeDirectional.intensity.value};
				frame.directionalLightAmbients[packedDirectionalIndex] = Vec4{zero(), zero(), zero(), activeDirectional.ambient};
			}
			for (std::size_t directionalIndex{}; directionalIndex < frame.activeDirectionalLightCount; ++directionalIndex) { frame.directionalLightAmbients[directionalIndex].z = static_cast<Scalar>(frame.activeDirectionalLightCount); }
			if (activeSpotLightViewProjCount == 0U) {
				renderer.spotLightViewProjs[0] = multiply(perspectiveVulkan(spotLightFov, one(), frame.shadowNearPlane, spotLightFar), lookAt(spot.position, add(spot.position, spotDirection), Vec3{zero(), one(), zero()})); ///< Legacy empty-vector fallback.
			}
			return frame;
		}
	};

} // namespace vve::simple
