/** @file @brief Independent scalar SMAA luma/search/area oracle; coordinates are texel centres, with no GPU-derived inputs. */

/// @brief Apply the Vulkan top-left coverage rule to the clockwise drawable-pixel triangles used by the native fixtures.
bool stageCovered(const Triangle &triangle, Point point) {
	for (unsigned i : {0U, 1U, 2U}) {
		const auto a = triangle[i], b = triangle[(i + 1) % 3];
		const double dx = b[0] - a[0], dy = b[1] - a[1];
		const double cross = dx * (point[1] - a[1]) - dy * (point[0] - a[0]);
		if (cross < 0 || (cross == 0 && !(dy < 0 || (dy == 0 && dx > 0)))) { return false; }
	}
	return true;
}

/// @brief Standard Vulkan sample coverage and pixel-frequency cutouts, independent of all GPU readbacks.
auto stageInput(const std::vector<Triangle> &shapes, Point offset, bool detail, unsigned samples, bool legacyUnorm = false) {
	std::vector<unsigned char> input(side * side);
	constexpr std::array<Point, 4> sample4{{{.375, .125}, {.875, .375}, {.125, .625}, {.625, .875}}};
	for (unsigned y : std::views::iota(0U, side)) {
		for (unsigned x : std::views::iota(0U, side)) {
			unsigned hits = 0;
			for (unsigned s : std::views::iota(0U, samples)) {
				const auto sample = samples == 4 ? sample4[s] : Point{.5, .5};
				const bool hole = detail && std::fmod(y + .5 - offset[1] - .3125 * (x + .5 - offset[0]) + 128, 12) > 6;
				hits += !hole && std::ranges::any_of(shapes, [&](const auto &t) {
					return stageCovered(t, {x + sample[0] - offset[0], y + sample[1] - offset[1]});
				});
			}
			// Legacy UNORM blits quantize linear light before CPU PNG encoding; SMAA's input is sRGB.
			const double linear = double(hits) / samples;
			input[y * side + x] = encode(legacyUnorm ? std::round(linear * 255) / 255 : linear);
		}
	}
	return input;
}

/// @brief Expand a scalar analytic fixture to opaque RGBA; every channel, including alpha, is checked later.
auto stageRgba(const std::vector<unsigned char> &input) {
	std::vector<unsigned char> result(side * side * 4, 255);
	for (unsigned i : std::views::iota(0U, side * side)) {
		for (unsigned c : {0U, 1U, 2U}) { result[i * 4 + c] = input[i]; }
	}
	return result;
}

/// @brief Clamp and bilinearly sample original rows; optionally model Mesa 25.2.8's documented 8.8 UNORM sampler.
/// @see Mesa tag mesa-25.2.8, gallivm/lp_bld_sample_aos.c and lp_bld_arit.c (lp_build_lerp_2d):
/// coordinates round to 1/256; each horizontal/vertical byte lerp rounds to nearest. This is a device model, not SMAA logic.
double stageSample(std::span<const unsigned char> bytes, int width, int height, unsigned channels,
	double x, double y, unsigned channel, bool fixed8 = false) {
	if (fixed8) { x = std::nearbyint(x * 256) / 256; y = std::nearbyint(y * 256) / 256; }
	const int ix = int(std::floor(x)), iy = int(std::floor(y));
	const double fx = x - ix, fy = y - iy;
	const auto at = [&](int px, int py) {
		return bytes[(std::clamp(py, 0, height - 1) * width + std::clamp(px, 0, width - 1)) * channels + channel] / 255.0;
	};
	const auto lerp = [fixed8](double a, double b, double t) {
		const double value = std::lerp(a, b, t);
		return fixed8 ? std::floor(value * 255 + .5) / 255 : value;
	};
	return lerp(lerp(at(ix, iy), at(ix + 1, iy), fx), lerp(at(ix, iy + 1), at(ix + 1, iy + 1), fx), fy);
}

/// @brief Map the reference's distances/crossings into raw top-to-bottom RG8 rows; zero selects the unjittered first 80 rows.
Point stageArea(Point distance, Point crossing, bool diagonal, unsigned subsample = 0, bool fixed8 = false) {
	const double x = diagonal ? 80 + distance[0] + 20 * crossing[0] : std::sqrt(distance[0]) + 16 * std::nearbyint(4 * crossing[0]);
	const double y = (diagonal ? distance[1] + 20 * crossing[1] : std::sqrt(distance[1]) + 16 * std::nearbyint(4 * crossing[1])) + 80 * subsample;
	return {stageSample(areaTexBytes, AREATEX_WIDTH, AREATEX_HEIGHT, 2, x, y, 0, fixed8),
		stageSample(areaTexBytes, AREATEX_WIDTH, AREATEX_HEIGHT, 2, x, y, 1, fixed8)};
}

/// @brief Expand SMAASearchLength's 66x33 scale/bias into packed 64x16 texel coordinates; retain reference linear filtering.
double stageSearchLength(double crossing, double line, bool positive, bool fixed8 = false) {
	return stageSample(searchTexBytes, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 1, 32 * crossing + (positive ? 33 : 0), 32 - 32 * line, 0, fixed8);
}

/// @brief Check hand-integrated areas independently of stageWeights, the GPU upload and the renderer's coordinate system.
/// @see Pinned upstream Scripts/AreaTex.py (areaortho/areadiag): geometric definition and 30x30 diagonal quadrature.
void stageTableChecks() {
	static_assert(AREATEX_WIDTH == 160 && AREATEX_HEIGHT == 560 && AREATEX_PITCH == 320);
	static_assert(SEARCHTEX_WIDTH == 64 && SEARCHTEX_HEIGHT == 16 && SEARCHTEX_PITCH == 64);
	// SMAAArea stores sqrt(distance) in five 16-wide crossing slots. Bilinear codes .25/ .75 mean above/below crossings.
	// For L(1,4), length=6: the reconstructed half-L descends from height 1/2 to zero over 3 pixels.
	// On pixel [1,2], heights are 1/3 and 1/6: trapezoid area=1/4. Reflection exchanges R/G.
	// Z(1,4) spans all 6 pixels, giving the same area; centred Z(1,1) splits into two triangles of area 1/24.
	// U(16,16), length=33, exceeds the reference's short-U smoothing range: two triangles give 2*(1/2*1/2*1/66)=1/132.
	constexpr std::array<Point, 6> distances{{{1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 1}, {16, 16}}};
	constexpr std::array<Point, 6> crossings{{{.25, 0}, {.75, 0}, {.25, .75}, {.75, .25}, {.25, .75}, {.25, .25}}};
	constexpr std::array<Point, 6> texels{{{17, 2}, {49, 2}, {17, 50}, {49, 18}, {17, 49}, {20, 20}}};
	constexpr std::array<Point, 6> analytic{{{0, .25}, {.25, 0}, {0, .25}, {.25, 0}, {1.0 / 24, 1.0 / 24}, {0, 1.0 / 132}}};
	bool detectsFlip = false, detectsSubsample = false;
	for (unsigned i : std::views::iota(0U, unsigned(texels.size()))) {
		const auto fetched = stageArea(distances[i], crossings[i], false);
		for (unsigned c : {0U, 1U}) {
			const auto byte = areaTexBytes[int(texels[i][1]) * AREATEX_PITCH + int(texels[i][0]) * 2 + c];
			require(byte == unsigned(255 * analytic[i][c]), "hand-derived orthogonal area byte (floor quantization)");
			const double raw = byte / 255.0;
			require(fetched[c] == raw && std::abs(raw - analytic[i][c]) < 1.0 / 255, "analytic orthogonal AreaTex indexing/area");
			const auto flipped = areaTexBytes[(559 - int(texels[i][1])) * AREATEX_PITCH + int(texels[i][0]) * 2 + c] / 255.0;
			detectsFlip |= std::abs(flipped - analytic[i][c]) > 1.0 / 255;
			detectsSubsample |= std::abs(stageArea(distances[i], crossings[i], false, 1)[c] - analytic[i][c]) > 1.0 / 255;
		}
		std::println("cpu.area.orthogonal={} texel={} analytic={} fetched={}", i, texels[i], analytic[i], fetched);
	}
	require(detectsFlip && detectsSubsample, "area controls must reject flipped rows and nonzero subsample");
	// SMAAAreaDiag uses four 20-wide slots in the right half, without sqrt. A terminated 45-degree line
	// with distances (2,2), crossings (1,2), cuts one unit square in half and only touches its neighbour: RG=(1/2,0).
	// The upstream diagonal table uses a 30x30 endpoint grid, so equality-on-line samples allow 1/60 area error plus 1 byte.
	// Hand count: 30*29/2=435 samples lie strictly below the diagonal; R=1-435/900=31/60, G=0.
	require(areaTexBytes[42 * AREATEX_PITCH + 102 * 2] == unsigned(255 * (31.0 / 60)) &&
		areaTexBytes[42 * AREATEX_PITCH + 102 * 2 + 1] == 0, "hand-counted diagonal quadrature byte");
	const auto diagonal = stageArea({2, 2}, {1, 2}, true);
	for (unsigned c : {0U, 1U}) {
		const double expected = c == 0 ? .5 : 0;
		require(diagonal[c] == areaTexBytes[42 * AREATEX_PITCH + 102 * 2 + c] / 255.0 &&
			std::abs(diagonal[c] - expected) <= 1.0 / 60 + 1.0 / 255, "analytic diagonal AreaTex indexing/area");
	}
	// Pseudo-gather coefficients are (1,3,7,21)/32. Full line/no crossing advances 2;
	// a current-only line advances 1; a current crossing stops right immediately but permits one left step.
	require(stageSearchLength(0, 1, false) * 255 == 254 && stageSearchLength(0, 1, true) * 255 == 254, "search full line");
	require(stageSearchLength(0, 21.0 / 32, false) * 255 == 127 && stageSearchLength(0, 21.0 / 32, true) * 255 == 127, "search isolated line");
	require(stageSearchLength(21.0 / 32, 1, false) * 255 == 127 && stageSearchLength(21.0 / 32, 1, true) == 0, "search crossing direction");
	// Between packed columns 2 (zero) and 3 (127), a quarter-texel shift must interpolate, rather than round.
	require(std::abs(stageSearchLength(2.25 / 32, 1, false) * 255 - 127.0 / 4) < 1e-12, "SearchTex requires linear filtering");
	// Independently expand the pseudo-gather's two rounded byte lerps: round(255*3/4)=191; round(191*7/8)=167.
	constexpr std::array<unsigned char, 4> corner{0, 0, 0, 255};
	require(stageSample(corner, 2, 2, 1, .75, .875, 0, true) == 167.0 / 255, "fixed-point sampler arithmetic");
	std::println("cpu.area.diagonal.texel=102,42 analytic=[0.5,0] fetched={} cpu.search=PASS", diagonal);
}

/// @brief Evaluate threshold .1 and local contrast adaptation 2 on encoded luma, independently of the shader adapter.
auto stageEdges(const std::vector<unsigned char> &encoded) {
	std::vector<unsigned char> result(side * side * 2);
	const auto luma = [&](int x, int y) { return stageSample(encoded, side, side, 1, x, y, 0); };
	for (int y : std::views::iota(0, int(side))) {
		for (int x : std::views::iota(0, int(side))) {
			const double c = luma(x, y), left = luma(x - 1, y), top = luma(x, y - 1);
			const std::array delta{std::abs(c - left), std::abs(c - top)};
			const double maximum = std::max({delta[0], delta[1], std::abs(c - luma(x + 1, y)),
				std::abs(c - luma(x, y + 1)), std::abs(left - luma(x - 2, y)), std::abs(top - luma(x, y - 2))});
			for (unsigned axis : {0U, 1U}) { result[(y * side + x) * 2 + axis] = delta[axis] >= .1 && 2 * delta[axis] >= maximum ? 255 : 0; }
		}
	}
	return result;
}

/// @brief Reconstruct HIGH weights from independently calculated edges and original AreaTex/SearchTex bytes.
auto stageWeights(const std::vector<unsigned char> &edges, bool fixed8 = false) {
	std::vector<unsigned char> result(side * side * 4);
	const auto edge = [&](double x, double y, unsigned c) { return stageSample(edges, side, side, 2, x, y, c, fixed8); };
	const auto decodeDiag = [](double r, double g) { return Point{std::nearbyint(r * std::abs(5 * r - 3.75)), std::nearbyint(g)}; };
	for (int y : std::views::iota(0, int(side))) {
		for (int x : std::views::iota(0, int(side))) {
			std::array<double, 4> weights{};
			bool west = edge(x, y, 0) != 0;
			if (edge(x, y, 1) != 0) {
				// Diagonal searches use eight steps, preserving their crossing codes and diagonal priority.
				const auto diagonal = [&](int dx, int dy, bool second) {
					double px = x + (second ? .25 : 0), py = y, distance = -1, active = 1;
					Point last{};
					while (distance < 7 && active > .9) {
						px += dx; py += dy; ++distance;
						last = {edge(px, py, 0), edge(px, py, 1)};
						if (second) { last = decodeDiag(last[0], last[1]); }
						active = (last[0] + last[1]) * .5;
					}
					return std::array{distance, active, last[1]};
				};
				auto a = west ? diagonal(-1, 1, false) : std::array{0.0, 0.0, 0.0};
				a[0] += a[2] > .9;
				auto b = diagonal(1, -1, false);
				if (a[0] + b[0] > 2) {
					const auto c0 = decodeDiag(edge(x - a[0] - .75, y + a[0], 0), edge(x - a[0] - .75, y + a[0], 1));
					const auto c1 = decodeDiag(edge(x + b[0] + 1, y - b[0] - .25, 0), edge(x + b[0] + 1, y - b[0] - .25, 1));
					const auto value = stageArea({a[0], b[0]}, {a[1] >= .9 ? 0 : 2 * c0[1] + c0[0],
						b[1] >= .9 ? 0 : 2 * c1[1] + c1[0]}, true, 0, fixed8);
					weights[0] += value[0]; weights[1] += value[1];
				}
				a = diagonal(-1, -1, true);
				b = edge(x + 1, y, 0) > 0 ? diagonal(1, 1, true) : std::array{0.0, 0.0, 0.0};
				b[0] += b[2] > .9;
				if (a[0] + b[0] > 2) {
					const double c0 = 2 * edge(x - a[0] - 1, y - a[0], 1) + edge(x - a[0], y - a[0] - 1, 0);
					const double c1 = 2 * edge(x + b[0] + 1, y + b[0], 1) + edge(x + b[0] + 1, y + b[0], 0);
					const auto value = stageArea({a[0], b[0]}, {a[1] >= .9 ? 0 : c0, b[1] >= .9 ? 0 : c1}, true, 0, fixed8);
					weights[0] += value[1]; weights[1] += value[0];
				}
				if (weights[0] != 0 || weights[1] != 0) { west = false; }
			}
			// Orthogonal searches share the same scalar algorithm with transposed coordinates/channels.
			for (unsigned axis : {0U, 1U}) {
				if (axis == 0 ? edge(x, y, 1) == 0 || weights[0] != 0 || weights[1] != 0 : !west) { continue; }
				const auto sample = [&](double along, double across, unsigned c) {
					return axis == 0 ? edge(along, across, 1 - c) : edge(across, along, c);
				};
				const double along = axis == 0 ? x : y, across = axis == 0 ? y : x;
				std::array<double, 2> ends{}, distances{}, crossing{};
				for (unsigned direction : {0U, 1U}) {
					const double sign = direction == 0 ? -1 : 1, start = along + (direction == 0 ? -.25 : 1.25);
					double position = start, line = 1, cross = 0;
					while (sign * (position - start) < 32 && line > .8281 && cross == 0) {
						line = sample(position, across - .125, 0); cross = sample(position, across - .125, 1);
						position += 2 * sign;
					}
					ends[direction] = position - sign * (3.25 - (255.0 / 127) * stageSearchLength(cross, line, direction != 0, fixed8));
					distances[direction] = std::abs(std::nearbyint(ends[direction] - along));
					crossing[direction] = sample(ends[direction] + direction, across - .25, 1);
				}
				auto value = stageArea(distances, crossing, false, 0, fixed8);
				// HIGH corner rounding is 25%; transposition also preserves the weight channel order.
				const Point near{double(distances[0] <= distances[1]), double(distances[1] <= distances[0])};
				for (unsigned c : {0U, 1U}) {
					const double shift = c == 0 ? 1 : -2;
					const double factor = 1 - .75 / (near[0] + near[1]) *
						(near[0] * sample(ends[0], across + shift, 1) + near[1] * sample(ends[1] + 1, across + shift, 1));
					weights[axis * 2 + c] = value[c] * std::clamp(factor, 0.0, 1.0);
				}
			}
			for (unsigned c : {0U, 1U, 2U, 3U}) { result[(y * side + x) * 4 + c] = std::lround(std::clamp(weights[c], 0.0, 1.0) * 255); }
		}
	}
	return result;
}

/// @brief Apply the reference neighbourhood rule in linear light using only CPU source coverage and CPU table weights.
auto stageBlend(const std::vector<unsigned char> &input, const std::vector<unsigned char> &weights) {
	std::vector<unsigned char> result(side * side * 4, 255);
	const auto index = [](int x, int y) { return std::clamp(y, 0, int(side) - 1) * side + std::clamp(x, 0, int(side) - 1); };
	const auto sample = [&](double x, double y) {
		const int ix = int(std::floor(x)), iy = int(std::floor(y));
		return std::lerp(std::lerp(decode(input[index(ix, iy)]), decode(input[index(ix + 1, iy)]), x - ix),
			std::lerp(decode(input[index(ix, iy + 1)]), decode(input[index(ix + 1, iy + 1)]), x - ix), y - iy);
	};
	for (int y : std::views::iota(0, int(side))) {
		for (int x : std::views::iota(0, int(side))) {
			const std::array a{weights[index(x + 1, y) * 4 + 3] / 255.0, weights[index(x, y + 1) * 4 + 1] / 255.0,
				weights[index(x, y) * 4 + 2] / 255.0, weights[index(x, y) * 4] / 255.0};
			double color = sample(x, y);
			if (std::accumulate(a.begin(), a.end(), 0.0) >= 1e-5) {
				const bool horizontal = std::max(a[0], a[2]) > std::max(a[1], a[3]);
				const double positive = a[horizontal ? 0 : 1], negative = a[horizontal ? 2 : 3];
				color = (positive * sample(x + (horizontal ? positive : 0), y + (horizontal ? 0 : positive)) +
					negative * sample(x - (horizontal ? negative : 0), y - (horizontal ? 0 : negative))) / (positive + negative);
			}
			for (unsigned c : {0U, 1U, 2U}) { result[index(x, y) * 4 + c] = encode(color); }
		}
	}
	return result;
}

/** @brief Propagate the independent weight/sampler precision into a per-pixel final-colour interval.
 * RADV uses the tighter one-encoded-byte oracle. Mesa's 8.8 sampler permits one weight byte,
 * half a coordinate quantum per axis and one linear byte for its UNORM interpolation/decoded colour.
 * Interval arithmetic includes either neighbourhood direction when one-byte weights can break a tie.
 * No measured GPU value enters these bounds; flat pixels remain exact apart from output quantization.
 */
auto stageOutputBounds(const std::vector<unsigned char> &input, const std::vector<unsigned char> &weights, bool fixed8) {
	const auto expected = stageBlend(input, weights);
	auto low = expected, high = expected;
	const auto index = [](int x, int y) { return std::clamp(y, 0, int(side) - 1) * side + std::clamp(x, 0, int(side) - 1); };
	for (int y : std::views::iota(0, int(side))) {
		for (int x : std::views::iota(0, int(side))) {
			double minimum = decode(expected[index(x, y) * 4]), maximum = minimum;
			if (fixed8) {
				const std::array a{weights[index(x + 1, y) * 4 + 3], weights[index(x, y + 1) * 4 + 1],
					weights[index(x, y) * 4 + 2], weights[index(x, y) * 4]};
				std::array<double, 4> lo{}, hi{};
				for (unsigned c : {0U, 1U, 2U, 3U}) {
					lo[c] = std::max(0, int(a[c]) - 1) / 255.0; hi[c] = std::min(255, int(a[c]) + 1) / 255.0;
				}
				const double centre = decode(input[index(x, y)]);
				for (unsigned axis : {0U, 1U}) {
					const unsigned p = axis, n = axis + 2, other = 1 - axis;
					if (std::max(hi[p], hi[n]) < std::max(lo[other], lo[other + 2])) { continue; }
					const double positive = decode(input[index(x + (axis == 0), y + (axis == 1))]);
					const double negative = decode(input[index(x - (axis == 0), y - (axis == 1))]);
					// Independent interval evaluation of normalized weights and the two adjacent linear samples.
					const double fractionLow = lo[p] / std::max(lo[p] + hi[n], 1e-20);
					const double fractionHigh = hi[p] / std::max(hi[p] + lo[n], 1e-20);
					const double sampling = (std::abs(positive - centre) + std::abs(negative - centre)) * (.5 / 256 + 1.0 / 255);
					for (double fraction : {fractionLow, fractionHigh}) {
						for (double ps : {lo[p], hi[p]}) {
							for (double ns : {lo[n], hi[n]}) {
								const double value = fraction * std::lerp(centre, positive, ps) + (1 - fraction) * std::lerp(centre, negative, ns);
								minimum = std::min(minimum, value - sampling); maximum = std::max(maximum, value + sampling);
							}
						}
					}
				}
			}
			for (unsigned c : {0U, 1U, 2U}) {
				low[index(x, y) * 4 + c] = std::max(0, int(encode(std::max(0.0, minimum))) - 1);
				high[index(x, y) * 4 + c] = std::min(255, int(encode(std::min(1.0, maximum))) + 1);
			}
		}
	}
	return std::pair{low, high};
}

/// @brief Shared real/control conformance path; opaque alpha has no precision allowance.
bool stageWithin(std::span<const unsigned char> actual, const std::vector<unsigned char> &low, const std::vector<unsigned char> &high) {
	if (actual.size() != low.size() || low.size() != high.size()) { return false; }
	for (unsigned i : std::views::iota(0U, unsigned(actual.size()))) {
		if (actual[i] < low[i] || actual[i] > high[i]) { return false; }
	}
	return true;
}

/// @brief Compare complete intermediate maps, recording the first divergent stage/pixel before reporting failure.
bool stageCompare(std::span<const std::byte> actual, const std::vector<unsigned char> &expected,
	unsigned channels, std::string_view fixture, unsigned stage) {
	require(actual.size() == expected.size(), "stage oracle extent");
	unsigned maximum = 0, failures = 0;
	for (unsigned i : std::views::iota(0U, unsigned(expected.size()))) {
		const auto value = std::to_integer<unsigned char>(actual[i]);
		const unsigned error = std::abs(int(value) - expected[i]);
		maximum = std::max(maximum, error);
		if (error > (stage == 0 || (stage == 2 && i % 4 == 3) ? 0 : 1) && failures++ == 0) {
			std::println("oracle.first.fixture={} stage={} xy={},{} channel={} expected={} actual={}",
				fixture, stage, i / channels % side, i / channels / side, i % channels, expected[i], value);
		}
	}
	std::println("oracle.fixture={} stage={} max.bytes={} failed.samples={} samples={}", fixture, stage, maximum, failures, expected.size());
	return failures == 0;
}

/// @brief Render canonical steps, a one-pixel horizontal line and a 45-degree edge; expected maps come only from geometry/tables.
bool stageOracle(simple::Engine &engine, NativeFixture &native, const References &references, const std::filesystem::path &directory) {
	auto &render = engine.renderSystem(); auto &gpu = render.forward();
	gpu.debugSmaaReadback = gpu.debugOutputReadback = true;
	VkPhysicalDeviceDriverProperties driver{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
	VkPhysicalDeviceProperties2 properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &driver};
	vkGetPhysicalDeviceProperties2(gpu.physicalDevice.physicalDevice, &properties);
	const bool fixed8 = driver.driverID == VK_DRIVER_ID_MESA_LLVMPIPE;
	require(!fixed8 || properties.properties.limits.subTexelPrecisionBits == 8, "unsupported llvmpipe sampler precision");
	std::println("oracle.sampler={} device={} driver={} info={}", fixed8 ? "Mesa_8.8_UNORM" : "ideal_linear",
		std::string_view{properties.properties.deviceName}, std::string_view{driver.driverName}, std::string_view{driver.driverInfo});
	bool accepted = true;
	constexpr std::array labels{"horizontal-step", "vertical-step", "horizontal-thin", "diagonal-45"};
	for (unsigned f : std::views::iota(0U, unsigned(labels.size() + names.size() * offsets.size()))) {
		native.offset = {}; native.detail = false;
		// Full-extent step edges avoid accidental end crossings; the finite line exercises crossing patterns/corners.
		const std::array<Point, 4> rectangle = f == 0 ? std::array<Point, 4>{{{0, 64}, {128, 64}, {128, 128}, {0, 128}}} :
			f == 1 ? std::array<Point, 4>{{{64, 0}, {128, 0}, {128, 128}, {64, 128}}} :
			std::array<Point, 4>{{{28, 64}, {100, 64}, {100, 65}, {28, 65}}};
		native.triangles = f == 3 ? std::vector<Triangle>{{{{-.25, 0}, {127.75, 128}, {-.25, 128}}}} :
			std::vector<Triangle>{{rectangle[0], rectangle[1], rectangle[2]}, {rectangle[0], rectangle[2], rectangle[3]}};
		const unsigned quality = f >= labels.size() ? (f - labels.size()) / offsets.size() : 0;
		const unsigned phase = f >= labels.size() ? (f - labels.size()) % offsets.size() : 0;
		const auto label = f < labels.size() ? std::string{labels[f]} : std::format("{}-o{}", names[quality], phase);
		if (f >= labels.size()) { native.triangles = triangles(quality); native.offset = offsets[phase]; native.detail = quality == residualFixture; }
		const auto input = stageInput(native.triangles, native.offset, native.detail, native.samples);
		const auto expectedEdges = stageEdges(input), expectedWeights = stageWeights(expectedEdges, fixed8);
		const auto expectedOutput = stageBlend(input, expectedWeights);
		render.notifyNativeDrawChanged();
		require(bool(engine.renderFrame()), "oracle frame"); render.waitIdle();
		auto &target = gpu.targets.front();
		for (unsigned stage : {0U, 1U}) {
			accepted = stageCompare(target.smaaReadback[stage].bytes(), stage == 0 ? expectedEdges : expectedWeights,
				stage == 0 ? 2 : 4, label, stage) && accepted;
		}
		// Assert final output against independently propagated format/sampler precision, including exact alpha.
		const auto stem = directory / std::format("oracle-b{}-m{}-{}", unsigned(render.rendererBackend()), native.samples, label);
		require(gpu.outputDiagnostics.size() == 1, "oracle output receipt");
		require(bool(gpu.writeOutputDiagnostic(gpu.outputDiagnostics.front(), stem.string() + "-output.png")), "oracle output capture");
		const auto actualOutput = pixels(stem.string() + "-output.png");
		unsigned outputError = 0;
		for (unsigned i : std::views::iota(0U, unsigned(expectedOutput.size()))) {
			outputError = std::max(outputError, unsigned(std::abs(int(actualOutput[i]) - expectedOutput[i])));
		}
		const auto [low, high] = stageOutputBounds(input, expectedWeights, fixed8);
		const bool outputAccepted = stageWithin(actualOutput, low, high);
		accepted = outputAccepted && accepted;
		std::println("oracle.fixture={} output.max.bytes={} output.accepted={} source=CPU_geometry_edges_tables_neighbourhood bound={}",
			label, outputError, outputAccepted, fixed8 ? "propagated_8.8_sampler_interval+1_output_byte" : "1_encoded_byte");
		require(stbi_write_png((stem.string() + "-cpu.png").c_str(), side, side, 4, expectedOutput.data(), side * 4), "CPU output write");
		if (f >= labels.size()) {
			const auto &ref = references[quality * offsets.size() + phase];
			std::println("oracle.reference.fixture={} samples={} cpu.metrics={} ideal.metrics={} gpu.metrics={}", label, native.samples,
				measure(expectedOutput, ref), measure(stageBlend(input, stageWeights(expectedEdges)), ref), measure(actualOutput, ref));
		}
		gpu.outputDiagnostics.clear();
		intermediateSheet(target, directory / std::format("oracle-b{}-m{}-{}.png", unsigned(render.rendererBackend()), native.samples, label), false, false);
	}
	gpu.debugOutputReadback = false;
	return accepted;
}

/// @brief Focused diagnostic entry point; normal CTest also runs these checks in every backend and SMAA mode.
int runStageOracle() {
	const auto directory = output / "current";
	std::filesystem::create_directories(directory);
	const auto references = prepareReferences(false);
	bool accepted = true;
	for (auto backend : {RendererBackend::forward, RendererBackend::deferred, RendererBackend::hybrid_gi}) {
		for (auto mode : {Antialiasing::smaa, Antialiasing::msaa4_smaa}) {
			auto engine = test::hiddenEngine("smaa-stage-oracle", PixelExtent{side, side});
			auto &render = engine.renderSystem();
			require(bool(engine.init()) && bool(render.setRendererBackend(backend)) && bool(render.setAntialiasing(mode)), "stage startup");
			render.setBackgroundColor(LinearColor{Vec3{0}}); render.clearLights();
			auto native = std::make_shared<NativeFixture>();
			native->triangles = triangles(0);
			require(bool(render.setNativeDraw([native](const NativeDrawContext &c) { return native->record(c); })), "stage callback");
			require(bool(engine.renderFrame()), "stage pipeline initialization");
#ifndef NDEBUG
			require(render.forward().validationActive(), "oracle requires active validation");
#endif
			accepted = stageOracle(engine, *native, references, directory) && accepted;
			render.waitIdle(); native.reset(); engine.gui().shutdownVulkan(); render.shutdown();
			require(render.forward().validationErrorCount() == 0, "oracle validation including teardown");
			std::println("oracle.backend={} mode={} validation.errors=0", unsigned(backend), unsigned(mode));
		}
	}
	require(accepted, "stage oracle mismatch");
	std::println("SMAA stage oracle PASS: 144 fixtures; device and driver receipts above");
	return 0;
}
