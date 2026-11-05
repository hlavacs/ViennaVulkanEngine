/** @file @brief Independent spatial coverage oracle, geometry masks and shared SMAA acceptance functions. */
using Point = std::array<double, 2>; ///< Drawable-pixel coordinates, independent of Vulkan matrices.
using Triangle = std::array<Point, 3>; ///< Deterministic geometry shared only as input.
constexpr unsigned side = 128; ///< Fixed extent, without GUI, atmosphere or animation.
constexpr unsigned grid = 16; ///< 256 independent linear-light box-filter samples per reference pixel.
constexpr std::array<Point, 4> offsets{{{0, 0}, {.25, .375}, {.5, .75}, {.875, .125}}}; ///< Fixed subpixel phases.
constexpr std::array names{"tilted", "thin", "corners", "diagonals", "cutout"}; ///< Individually measured fixtures.
constexpr unsigned filterSupport = 24; ///< Conservative geometry dilation; pinned SMAANeighborhoodBlendingPS mixes only adjacent colour texels, even after HIGH's 16/8 edge searches.
constexpr unsigned edgeRadius = 2; ///< Fixed geometry band includes both sides of each rasterized boundary.
constexpr std::array maxSmaaRatio{.99, 1.0, .99, .99, .99}; ///< Historical limits; thin's 1.0 is diagnostic after the authorized 4 November reconciliation.
constexpr unsigned residualFixture = 4; ///< Cutout retains the largest MSAA residual (.037541); pixel-frequency holes remain aliased.
constexpr double maxCombinedResidualRatio = .99; ///< Require 1% of the remaining cutout error, above the measured zero repeat variation.
constexpr double maxCombinedRegression = 1.0 / (grid * grid); ///< Historical .00390625; filtered results additionally account for the independent reference's own regression.
constexpr double maxAreaRelative = .025; ///< Historical 2.5%; filtered thin-area error is bounded relative to reference SMAA, never a candidate-derived maximum.
constexpr double minThinContrast = .85; ///< Between measured resolved contrast 1 and the independent 3x3 blur plateau 2/3.
constexpr unsigned outsideByteLimit = 1; ///< Only one encoded-byte quantization step is allowed outside the geometry/filter support.
constexpr unsigned repeats = 3; ///< Every backend/mode/fixture/offset has three independent captured frames.
constexpr unsigned repeatByteLimit = 0; ///< All 360 none/MSAA repeat captures were identical in decoded RGBA.
constexpr double repeatMetricLimit = 0; ///< Identical repeats have exactly zero linear-light metric variation.
constexpr std::array<std::array<double, 5>, 2> frozenErrors{{
	{.03106987701406931, .045449522630834516, .03155653651021725, .030798441497800116, .041116474117023266},
	{.011331090528905335, .006978857010126477, .012358454450816068, .012029052826994664, .037541147277326284}
}}; ///< Mean edge MAE over four offsets; identical on forward, deferred and hybrid_gi, all three repeats.
constexpr std::array<std::uint64_t, 2> frozenHashes{0x8fb02ade1fca61ddULL, 0xc8e5b19846691cb0ULL}; ///< Ordered decoded RGBA receipts, identical across all three backends/repeats in the pre-engine freeze.

/// @brief Fixed shallow slopes, resolved thin features, concave corners, long diagonal and periodic cutout.
auto triangles(unsigned fixture) {
	std::vector<Triangle> result;
	const auto quad = [&](Point a, Point b, Point c, Point d) {
		result.push_back({a, b, c});
		result.push_back({a, c, d});
	};
	switch (fixture) {
	case 0: quad({30, 25}, {101, 44}, {84, 102}, {13, 83}); break;
	case 1:
		for (double x : {40.0, 64.0, 88.0}) { quad({x, 28}, {x + 2, 28}, {x + 2, 100}, {x, 100}); }
		break;
	case 2:
		quad({35, 29}, {97, 48}, {94, 60}, {32, 41});
		quad({32, 41}, {44, 45}, {27, 99}, {15, 95});
		break;
	case 3: quad({24, 23}, {105, 101}, {100, 106}, {19, 28}); break;
	case 4: quad({28, 28}, {100, 28}, {100, 100}, {28, 100}); break;
	}
	return result;
}
/// @brief Double-precision half-planes, independent of the GPU fixed-function rasterizer.
bool contains(const Triangle &triangle, Point point) {
	double low = 1e30, high = -1e30;
	for (auto i : std::views::iota(0U, 3U)) {
		const auto a = triangle[i], b = triangle[(i + 1) % 3];
		const double cross = (b[0] - a[0]) * (point[1] - a[1]) - (b[1] - a[1]) * (point[0] - a[0]);
		low = std::min(low, cross);
		high = std::max(high, cross);
	}
	return low >= 0 || high <= 0;
}
/// @brief Pinned 16x16 integration of linear white coverage, including independently defined shader holes.
auto reference(unsigned fixture, Point offset) {
	std::vector<double> values(side * side);
	const auto shapes = triangles(fixture);
	const auto hit = [&](Point point) {
		if (fixture == residualFixture && std::fmod(point[1] - .3125 * point[0] + 128, 12) > 6) { return false; }
		return std::ranges::any_of(shapes, [&](const auto &triangle) { return contains(triangle, point); });
	};
	// Each pixel integrates 256 fixed subpixel samples before display encoding.
	for (auto y : std::views::iota(0U, side)) {
		for (auto x : std::views::iota(0U, side)) {
			unsigned hits = 0;
			for (auto sy : std::views::iota(0U, grid)) {
				for (auto sx : std::views::iota(0U, grid)) {
					hits += hit({x + (sx + .5) / grid - offset[0], y + (sy + .5) / grid - offset[1]});
				}
			}
			values[y * side + x] = double(hits) / (grid * grid);
		}
	}
	return values;
}
/// @brief Dilates only independently integrated geometry boundaries; candidate images never enter mask construction.
auto mask(const std::vector<double> &ref, unsigned radius) {
	std::vector<bool> result(side * side);
	std::array<unsigned, (side + 1) * (side + 1)> prefix{};
	// A summed-area table dilates the boundary by the fixed square filter support.
	for (auto y : std::views::iota(0U, side)) {
		for (auto x : std::views::iota(0U, side)) {
			const auto p = y * side + x;
			const bool boundary = (ref[p] > 0 && ref[p] < 1) || (x > 0 && ref[p] != ref[p - 1]) ||
				(x + 1 < side && ref[p] != ref[p + 1]) || (y > 0 && ref[p] != ref[p - side]) || (y + 1 < side && ref[p] != ref[p + side]);
			prefix[(y + 1) * (side + 1) + x + 1] = boundary + prefix[y * (side + 1) + x + 1] +
				prefix[(y + 1) * (side + 1) + x] - prefix[y * (side + 1) + x];
		}
	}
	for (auto y : std::views::iota(0U, side)) {
		for (auto x : std::views::iota(0U, side)) {
			const unsigned l = x > radius ? x - radius : 0, t = y > radius ? y - radius : 0;
			const unsigned r = std::min(side, x + radius + 1), b = std::min(side, y + radius + 1);
			result[y * side + x] = prefix[b * (side + 1) + r] + prefix[t * (side + 1) + l] >
				prefix[t * (side + 1) + r] + prefix[b * (side + 1) + l];
		}
	}
	return result;
}
/// @brief Decode display bytes so coverage errors and filtering use linear light.
double decode(unsigned char value) {
	const double c = value / 255.0;
	return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4);
}
/// @brief Encode only after linear integration, including CPU blur controls and reference sheets.
unsigned char encode(double value) {
	return std::lround(255 * std::clamp(value <= .0031308 ? 12.92 * value : 1.055 * std::pow(value, 1 / 2.4) - .055, 0.0, 1.0));
}
/// @brief Measure edge MAE, relative integrated-coverage error and resolved contrast against independent coverage.
auto measure(const std::vector<unsigned char> &rgba, const std::vector<double> &ref) {
	const auto edges = mask(ref, edgeRadius);
	double error = 0, mass = 0, area = 0, peak = 0;
	unsigned count = 0;
	for (auto i : std::views::iota(0U, side * side)) {
		const double value = decode(rgba[i * 4]);
		if (edges[i]) {
			error += std::abs(value - ref[i]);
			++count;
		}
		mass += value;
		area += ref[i];
		peak = std::max(peak, value);
	}
	require(count > 200 && area > 250, "undersized reference/edge region");
	return std::array{error / count, std::abs(mass - area) / area, peak};
}
/// @brief Gate the selected fixture's aggregate SMAA error against none; thin lines are preservation-only.
bool evaluateSmaaEffect(double candidate, double none, unsigned fixture) {
	return std::isfinite(candidate) && none > 0 && candidate <= none * maxSmaaRatio[fixture];
}
/// @brief Gate the preselected combined-mode cutout residual, using the same rule for real results and controls.
bool evaluateCombinedResidual(double candidate, double msaa) {
	return std::isfinite(candidate) && msaa > 0 && candidate <= msaa * maxCombinedResidualRatio;
}
/// @brief Retain the historical combined-regression rule for analytic metric checks and explicit defect diagnostics.
bool evaluateRegressionVsMsaa(double candidate, double msaa) {
	return std::isfinite(candidate) && candidate <= msaa + maxCombinedRegression;
}
/// @brief Preserve unfiltered coverage/contrast; filtered modes use independent reference-relative area bounds.
bool evaluateThinFeature(const std::array<double, 3> &metrics, unsigned fixture) {
	return std::isfinite(metrics[1]) && metrics[1] <= maxAreaRelative && (fixture != 1 || metrics[2] >= minThinContrast);
}
/// @brief Permit at most one encoded byte of change beyond the pinned geometry/filter support, including alpha.
bool evaluateOutsideMask(const std::vector<unsigned char> &candidate, const std::vector<unsigned char> &base,
	const std::vector<bool> &affected) {
	unsigned outside = 0;
	for (auto i : std::views::iota(0U, side * side)) {
		if (affected[i]) { continue; }
		++outside;
		for (auto c : std::views::iota(0U, 4U)) {
			if (std::abs(int(candidate[i * 4 + c]) - int(base[i * 4 + c])) > outsideByteLimit) { return false; }
		}
	}
	return outside > 100;
}
/// @brief Construct an actual RGBA 3x3 linear box blur; it must be rejected by the production detail gate.
auto boxBlur(const std::vector<unsigned char> &base) {
	auto result = base;
	for (int y = 1; y < int(side) - 1; ++y) {
		for (int x = 1; x < int(side) - 1; ++x) {
			for (auto c : std::views::iota(0U, 3U)) {
				double sum = 0;
				for (int dy : {-1, 0, 1}) {
					for (int dx : {-1, 0, 1}) { sum += decode(base[((y + dy) * side + x + dx) * 4 + c]); }
				}
				result[(y * side + x) * 4 + c] = encode(sum / 9);
			}
		}
	}
	return result;
}
/// @brief Extend an ordered decoded-byte receipt, independently of PNG container metadata.
void hashPixels(std::uint64_t &hash, const std::vector<unsigned char> &rgba) {
	for (auto byte : rgba) { hash = (hash ^ byte) * 1099511628211ULL; }
}
