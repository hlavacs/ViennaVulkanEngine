/** @file @brief Independent Neutral/display-encoding oracle, immutable default-path invariants and HDR readback. */
#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <vulkan/vulkan.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVE.TestSupport;

#include "support/ToneMappingOracle.hpp"

namespace {
using namespace vve::test::tone;
using namespace vve;
constexpr unsigned side = 64; ///< Empty fixture: every pixel is independently known scene data, except the explicit GUI test.
constexpr double hdrRelativeTolerance = .001; ///< FP16 normal rounding <=2^-10; plus 1e-7 for subnormal quantization.
const std::filesystem::path output{VVE_TONEMAP_DIR}; ///< Disposable captures never replace IQ-00 references.

/// @brief Compare all decoded RGBA pixels to an independent transfer/quantization oracle; alpha and order are explicit.
auto capture(simple::Engine &engine, std::string_view name, RGB background, ToneMapping settings, bool unorm, float alpha = 1) {
	auto &render = engine.renderSystem();
	require(bool(render.setToneMapping(settings)), "valid tone-mapping setter");
	render.setBackgroundColor({Vec3{float(background[0]),float(background[1]),float(background[2])}});
	require(bool(engine.renderFrame()), "render background");
	const auto path = output / std::format("{}.png", name);
	require(bool(render.captureFrameToPng(path)), "PNG readback");
	const auto pixels = test::imagePixels(path);
	require(pixels && pixels->size() == side * side * 4, "decoded dimensions");
	const auto expected = settings.op == ToneMappingOperator::none ? background : neutral(background, settings.exposure);
	std::array<int, 4> bytes{0,0,0,int(std::round(alpha*255))};
	// Legacy UNORM quantizes linear RGB before its historical CPU encoding; opt-in UNORM encodes in the shader.
	for (auto c : std::views::iota(0U, 3U)) {
		double value = std::clamp(expected[c], 0.0, 1.0);
		if (unorm && settings.op == ToneMappingOperator::none && value > 0) {
			// Quantization at a half-texel boundary matters: FP16 .1 is just below the UNORM 25.5 rounding tie.
			const double step = std::exp2(std::max(-24.0,std::floor(std::log2(value))-10));
			value = std::round(std::nearbyint(value/step)*step*255)/255;
		}
		bytes[c] = int(std::round(srgb(value) * 255));
	}
	int maximumError{};
	for (auto offset : std::views::iota(0U, side*side)) {
		for (auto c : std::views::iota(0U, 4U)) maximumError = std::max(maximumError, std::abs(int((*pixels)[offset*4+c])-bytes[c]));
		require(std::abs(int((*pixels)[offset*4+3])-bytes[3]) <= 1, "alpha unchanged");
	}
	std::println("fixture={} expected={},{},{} actual={},{},{} maximum_byte_error={} pixels={}",name,
		bytes[0],bytes[1],bytes[2],(*pixels)[0],(*pixels)[1],(*pixels)[2],maximumError,side*side);
	require(maximumError <= 1, "scene-only RGB must match within 1/255");
	const auto linear = render.captureFrameLinear(render.forward().targets.front().handle);
	require(linear && linear->size() == side*side, "linear HDR readback");
	// Display processing must never write back into HDR; use source values, not tone-mapped expected pixels.
	for (const auto &pixel : *linear) {
		const RGB actual{pixel.x,pixel.y,pixel.z};
		for (auto c : std::views::iota(0U, 3U)) {
			if (std::isfinite(background[c])) require(std::abs(actual[c]-background[c]) <= std::abs(background[c])*hdrRelativeTolerance+1e-7, "untouched FP16 HDR");
		}
		require(pixel.w == alpha, "linear alpha");
	}
	const auto &renderer = render.forward();
	require(renderer.targets.front().readbackOutputEncoded == (!unorm || settings.op != ToneMappingOperator::none), "recorded frame encoding");
	require(std::ranges::count(renderer.lastRecordedPassOrder(), simple::ForwardRenderer::RecordedPass::tone_mapping) ==
		unsigned(settings.op != ToneMappingOperator::none), "recorded output pass");
	require(renderer.validationErrorCount() == 0, "zero validation errors");
	return *pixels;
}
/// @brief Opaque widget and raw HUD RGB retain their authored bytes on both mutable and sRGB fallback attachments.
void guiColors(simple::Engine &engine, std::string_view suffix) {
	auto &render = engine.renderSystem();
	require(bool(render.setToneMapping({ToneMappingOperator::khronos_neutral,1})), "GUI tone mapping");
	const auto source = output / "gui-source.ppm";
	{
		std::ofstream file{source,std::ios::binary}; file << "P6\n2 2\n255\n";
		for ([[maybe_unused]] auto i : std::views::iota(0,4)) file << "\x80\x40\xc8";
		require(bool(file), "GUI source fixture");
	}
	const auto hud = engine.gui().loadImage(source);
	require(bool(hud), "HUD load");
	engine.gui().draw([&] {
		const auto *viewport = ImGui::GetMainViewport();
		const auto pos = viewport->Pos;
		auto *draw = ImGui::GetBackgroundDrawList();
		draw->AddRectFilled(pos,{pos.x+32,pos.y+64},IM_COL32(128,64,200,255));
		if (const auto id = engine.gui().imageId(*hud)) draw->AddImage(ImTextureID(id),{pos.x+32,pos.y},{pos.x+64,pos.y+64});
	});
	require(bool(engine.renderFrame()), "GUI render");
	const auto path = output / std::format("gui-{}.png",suffix);
	require(bool(render.captureFrameToPng(path)), "GUI readback");
	const auto pixels = test::imagePixels(path);
	require(pixels && pixels->size() == side*side*4, "GUI dimensions");
	for (unsigned x : {16,48}) {
		const auto *pixel = test::pixelAt(pixels->data(),side,x,32);
		std::println("gui.x={} rgb={},{},{} alpha={}",x,pixel[0],pixel[1],pixel[2],pixel[3]);
		require(std::abs(int(pixel[0])-128)<=1 && std::abs(int(pixel[1])-64)<=1 && std::abs(int(pixel[2])-200)<=1 && pixel[3]==255, "GUI exactly-one encoding");
	}
	engine.gui().draw({});
}
/// @brief Validate one backend, including transitions, bad setters, unsupported outputs and default resource absence.
void backend(RendererBackend selected, bool unorm, bool noMutable) {
	auto engine = test::hiddenEngine("tone-mapping", PixelExtent{side,side});
	auto &render = engine.renderSystem();
	auto &facade = render; // Internal diagnostics remain confined to this native test.
	require(bool(facade.setRendererBackend(selected)), "backend selection");
	require(facade.toneMapping().op == ToneMappingOperator::none && facade.toneMapping().exposure == 1, "fresh defaults");
	require(bool(engine.init()) && bool(engine.renderFrame()), "graphics startup");
	auto &renderer = render.forward();
	auto &target = renderer.targets.front();
	const std::string prefix = unorm ? "unorm" : noMutable ? "no-mutable" : "srgb";
	presentation(renderer);
#ifndef NDEBUG
	require(renderer.validationActive(), "Debug requires active Khronos validation");
#endif
	const auto format = target.swapchain.imageFormat;
	require(unorm == (format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_R8G8B8A8_UNORM), "requested real surface format");
	if (noMutable) require(!renderer.device.swapchainMutableFormat && target.imageViews.ownedGuiViews.empty(), "extension excluded by Profiles");
	std::println("backend={} format={} color_space={} mutable={} validation={} exposure_policy_cap=1000000 hdr_relative_tolerance={}",
		int(selected),int(format),int(target.swapchain.colorSpace),renderer.device.swapchainMutableFormat,renderer.validationActive(),hdrRelativeTolerance);
	require(target.postImage.image == VK_NULL_HANDLE && !target.toneMapLayout.valid() && !target.toneMapPipeline.pipeline.valid() &&
		!target.toneMapPool.descriptorPool.valid() && !target.toneMapPipelineLayout.valid() && !target.toneMapGuiLayout.valid() &&
		!target.toneMapGuiPipeline.pipeline.valid() && !target.hdrSampled && target.toneMapSet == VK_NULL_HANDLE, "fresh default allocates no output objects");
	require(std::ranges::count(renderer.lastRecordedPassOrder(),simple::ForwardRenderer::RecordedPass::tone_mapping) == 0, "fresh default records no output pass");
	constexpr std::array backgrounds{RGB{.02,.18,1},RGB{4,2,.5},RGB{0,0,0},RGB{.08,.08,.08},RGB{.8,.1,.1},RGB{.81,.1,.1}};
	unsigned index{};
	for (auto op : {ToneMappingOperator::none, ToneMappingOperator::khronos_neutral}) {
		for (float exposure : {1.F,2.F}) {
			for (const auto color : backgrounds) capture(engine,std::format("{}-{}-{}-{}",unorm?"unorm":noMutable?"no-mutable":"srgb",int(selected),index++,int(op)),color,{op,exposure},unorm);
		}
	}
	constexpr RGB color{.02,.18,1};
	const ToneMapping good{ToneMappingOperator::khronos_neutral,2};
	const auto before = capture(engine,std::format("{}-invalid-before-{}",prefix,int(selected)),color,good,unorm);
	const float nan = std::numeric_limits<float>::quiet_NaN(), infinity = std::numeric_limits<float>::infinity();
	const std::array invalid{ToneMapping{good.op,0},ToneMapping{good.op,-1},ToneMapping{good.op,nan},ToneMapping{good.op,infinity},
		ToneMapping{static_cast<ToneMappingOperator>(999),1},ToneMapping{ToneMappingOperator::none,nan}};
	for (const auto settings : invalid) {
		const auto failed = facade.setToneMapping(settings);
		require(!failed && failed.error() == Error::invalid_argument, "invalid setter error");
		require(facade.toneMapping().op == good.op && facade.toneMapping().exposure == good.exposure, "invalid setter preserves state");
		const auto path = output / std::format("{}-invalid-{}.png",prefix,int(selected));
		require(bool(render.captureFrameToPng(path)), "invalid setter output readback");
		require(test::imagePixels(path) == before, "invalid setter preserves output exactly");
	}
	// An unsupported colour-space contract is rejected transactionally before another submission.
	const auto original = target.swapchain.colorSpace;
	target.swapchain.colorSpace = VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
	const auto unsupported = facade.setToneMapping(good);
	target.swapchain.colorSpace = original;
	require(!unsupported && unsupported.error() == Error::platform_error && facade.toneMapping().exposure == good.exposure, "unsupported output rejection");
	const auto originalFormat = target.swapchain.imageFormat;
	target.swapchain.imageFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
	const auto badFormat = facade.setToneMapping(good);
	target.swapchain.imageFormat = originalFormat;
	require(!badFormat && badFormat.error() == Error::platform_error && facade.toneMapping().exposure == good.exposure, "unsupported format rejection");
	unsigned policy{};
	for (const auto input : {RGB{-1,.18,1},RGB{nan,.18,1},RGB{infinity,0,0}}) {
		capture(engine,std::format("{}-policy-{}-{}",prefix,int(selected),policy++),input,good,unorm);
	}
	capture(engine,std::format("{}-overflow-{}",prefix,int(selected)),RGB{4,2,.5},{good.op,std::numeric_limits<float>::max()},unorm);
	capture(engine,std::format("{}-disabled-{}",prefix,int(selected)),color,{},unorm);
	require(!target.toneMapLayout.valid() && target.postImage.image == VK_NULL_HANDLE, "disable retires output owners");
	if (selected == RendererBackend::forward) {
		// An attachment clear inside the native colour pass supplies independently known fractional alpha.
		require(bool(render.setNativeDraw([](const NativeDrawContext &context) -> std::expected<void,Error> {
			if (!context.prepare && context.pass == NativeDrawPass::forward_opaque) {
				const VkClearAttachment clear{.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,.colorAttachment=0,
					.clearValue={.color={.float32={.02F,.18F,1.F,.25F}}}};
				const VkClearRect rect{.rect={.extent={context.width,context.height}},.layerCount=1};
				vkCmdClearAttachments(reinterpret_cast<VkCommandBuffer>(context.commandBuffer),1,&clear,1,&rect);
			}
			return {};
		})), "native alpha fixture");
		capture(engine,std::format("alpha-{}-{}",unorm,noMutable),color,good,unorm,.25F);
		require(bool(render.setNativeDraw({})), "clear alpha fixture");
		guiColors(engine,unorm?"unorm":noMutable?"no-mutable":"srgb");
	}
	engine.gui().shutdownVulkan(); render.shutdown();
	require(renderer.validationErrorCount() == 0, "zero validation errors through teardown");
}
/// @brief Smoke-check the pinned vvppl library's non-swapchain destination, without expanding IQ-02b's effect matrix.
void postDestination(bool unorm, bool noMutable) {
	auto engine = test::hiddenEngine("tone-mapping-post-destination",PixelExtent{side,side});
	auto &render = engine.renderSystem();
	render.setPostProcessSetup([](auto &) {}); // Identity chain still performs the library's source/destination blits.
	require(bool(engine.init()), "post-chain startup");
	capture(engine,std::format("post-destination-{}-{}",unorm,noMutable),RGB{.02,.18,1},{ToneMappingOperator::khronos_neutral,2},unorm);
	const auto &target = render.forward().targets.front();
	require(target.postImage.image != VK_NULL_HANDLE && target.postImage.format == VK_FORMAT_R16G16B16A16_SFLOAT, "HDR intermediate exists only with a chain");
	const auto passes = render.forward().lastRecordedPassOrder();
	require(std::ranges::find(passes,simple::ForwardRenderer::RecordedPass::post_process) <
		std::ranges::find(passes,simple::ForwardRenderer::RecordedPass::tone_mapping), "vvppl precedes Neutral");
	engine.gui().shutdownVulkan(); render.shutdown();
	require(render.forward().validationErrorCount()==0,"post-chain validation through teardown");
}
} // namespace

/// @brief Run native scene-only colour oracles with actual presentation; no software renderer is labelled physical evidence.
int main(int argc, char **argv) {
	try {
		const bool unorm = argc == 2 && std::string_view{argv[1]} == "--unorm";
		const bool noMutable = argc == 2 && std::string_view{argv[1]} == "--no-mutable";
		std::filesystem::create_directories(output);
		fixedVectors();
		// Exercise the facade forwarding without exposing any implementation constructor to user programs.
		{ vve::Engine<> engine; auto world = engine.world(); auto &render = world.get<vve::RenderSystem>();
			require(bool(render.setToneMapping({ToneMappingOperator::khronos_neutral,2})) && render.toneMapping().exposure == 2, "public facade forwarding"); }
		for (auto selected : {RendererBackend::forward,RendererBackend::deferred,RendererBackend::hybrid_gi}) backend(selected,unorm,noMutable);
		postDestination(unorm,noMutable);
		std::println("tone_mapping.result=pass backends=3 skips=0"); return 0;
	} catch (const std::exception &error) { std::cerr << "tone_mapping.failure=" << error.what() << '\n'; return 1; }
}
