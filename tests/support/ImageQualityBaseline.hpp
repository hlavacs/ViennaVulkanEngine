/** @file @brief Frozen pre-renderer-change IQ-00 reference receipts; never derive limits from candidate output.
 * Entries are fixture name, three decoded RGBA FNV-1a64 hashes, maximum changed RGB pixels,
 * maximum RGB byte error and maximum mean RGB byte error. Dimensions and alpha must match exactly.
 * Measured on llvmpipe/Mesa 25.2.8, X11, Debug Clang 18.1.3: every repeat was identical.
 * These exact limits apply to this archived environment; other GPUs/drivers need separate evidence.
 * The recording executable emits this table only during the initial, explicitly requested freeze.
 */
#pragma once
namespace vve::test {
inline constexpr std::string_view baselineRevision = "59f56667951477407907aee2326245b54d14ad3f"; ///< Unmodified renderer revision.
using ImageQualityReceipt = std::tuple<std::string_view, std::array<std::uint64_t, 3>, std::size_t, unsigned, double>; ///< Name, hashes and measured RGB limits.
inline constexpr std::array<ImageQualityReceipt, 22>
	imageQualityLimits{{ ///< Pairwise maxima of three independent runs; no added tolerance.
	{"lighting-lit-forward", {18179140065675043985ULL, 18179140065675043985ULL, 18179140065675043985ULL}, 0, 0, 0},
	{"lighting-unlit-forward", {15433285345639360495ULL, 15433285345639360495ULL, 15433285345639360495ULL}, 0, 0, 0},
	{"lighting-flat-forward", {6126854534839885484ULL, 6126854534839885484ULL, 6126854534839885484ULL}, 0, 0, 0},
	{"lighting-plane-forward", {9430193283104514853ULL, 9430193283104514853ULL, 9430193283104514853ULL}, 0, 0, 0},
	{"sponza-lit-forward", {14090867229110269804ULL, 14090867229110269804ULL, 14090867229110269804ULL}, 0, 0, 0},
	{"sponza-unlit-forward", {12940808709304888613ULL, 12940808709304888613ULL, 12940808709304888613ULL}, 0, 0, 0},
	{"background-default-forward", {5143932082586854181ULL, 5143932082586854181ULL, 5143932082586854181ULL}, 0, 0, 0},
	{"background-set-forward", {17263465165633119013ULL, 17263465165633119013ULL, 17263465165633119013ULL}, 0, 0, 0},
	{"aa-forward-none", {4500747220196347142ULL, 4500747220196347142ULL, 4500747220196347142ULL}, 0, 0, 0},
	{"aa-forward-msaa4", {992720951006725185ULL, 992720951006725185ULL, 992720951006725185ULL}, 0, 0, 0},
	{"lighting-lit-deferred", {12937388499847308050ULL, 12937388499847308050ULL, 12937388499847308050ULL}, 0, 0, 0},
	{"lighting-unlit-deferred", {15433285345639360495ULL, 15433285345639360495ULL, 15433285345639360495ULL}, 0, 0, 0},
	{"lighting-flat-deferred", {16647471103686411414ULL, 16647471103686411414ULL, 16647471103686411414ULL}, 0, 0, 0},
	{"lighting-plane-deferred", {9430193283104514853ULL, 9430193283104514853ULL, 9430193283104514853ULL}, 0, 0, 0},
	{"sponza-lit-deferred", {14090867229110269804ULL, 14090867229110269804ULL, 14090867229110269804ULL}, 0, 0, 0},
	{"sponza-unlit-deferred", {12940808709304888613ULL, 12940808709304888613ULL, 12940808709304888613ULL}, 0, 0, 0},
	{"background-default-deferred", {5143932082586854181ULL, 5143932082586854181ULL, 5143932082586854181ULL}, 0, 0, 0},
	{"background-set-deferred", {17263465165633119013ULL, 17263465165633119013ULL, 17263465165633119013ULL}, 0, 0, 0},
	{"aa-deferred-none", {4500747220196347142ULL, 4500747220196347142ULL, 4500747220196347142ULL}, 0, 0, 0},
	{"aa-deferred-msaa4", {992720951006725185ULL, 992720951006725185ULL, 992720951006725185ULL}, 0, 0, 0},
	{"aa-hybrid_gi-none", {4500747220196347142ULL, 4500747220196347142ULL, 4500747220196347142ULL}, 0, 0, 0},
	{"aa-hybrid_gi-msaa4", {992720951006725185ULL, 992720951006725185ULL, 992720951006725185ULL}, 0, 0, 0},
	}};
inline constexpr std::array<ImageQualityReceipt, 4> imageQualityUnormLimits{{ ///< Forced legacy-UNORM receipts; independent of sRGB limits.
	{"background-set-forward-unorm", {15284330199871963941ULL, 15284330199871963941ULL, 15284330199871963941ULL}, 0, 0, 0},
	{"lighting-lit-forward-unorm", {1556009198124887523ULL, 1556009198124887523ULL, 1556009198124887523ULL}, 0, 0, 0},
	{"sponza-lit-forward-unorm", {11946678227560350847ULL, 11946678227560350847ULL, 11946678227560350847ULL}, 0, 0, 0},
	{"aa-forward-unorm", {9349477555766551046ULL, 9349477555766551046ULL, 9349477555766551046ULL}, 0, 0, 0},
}};
}
