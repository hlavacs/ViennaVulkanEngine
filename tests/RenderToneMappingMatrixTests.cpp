/** @file @brief MSAA, vvppl composition and two-window tone-mapping lifecycle with fenced ordinary-frame readback. */
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_error.h>
#include <imgui.h>
#include <vulkan/vulkan.h>
#include <VVPPL.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVE.TestSupport;
#include "support/ToneMappingOracle.hpp"

namespace {
using namespace vve;
using namespace vve::test::tone;
using Renderer = simple::ForwardRenderer; ///< Internal diagnostics stay confined to this native test.
const std::filesystem::path root{VVE_TONEMAP_MATRIX_DIR}; ///< Candidate-only artifacts, separate for each actual surface mode.
constexpr RGB background{.125,.25,2}; ///< Exactly representable FP16 input; whole image is a scene-only mask, with no GUI.
constexpr RGB gains{2,.5,.25}; ///< Exact per-channel analytic colour-grade gain; all other grade parameters are identities.

/// @brief Independent double Narkowicz twin, pinned vvppl e69f3e56; this deliberate ACES-then-Neutral case tone-maps twice.
RGB postColor(unsigned chain) {
	auto color = background;
	for (auto c : std::views::iota(0U,3U)) {
		if (chain == 2) color[c] *= gains[c];
		if (chain == 3) {
			const double x = color[c] * .85;
			color[c] = std::clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.0,1.0);
		}
	}
	return color;
}
/// @brief Independently rounded FP16 input for the legacy linear UNORM quantization stage.
double half(double value) {
	if (value == 0) return 0;
	const double step = std::exp2(std::max(-24.0,std::floor(std::log2(value))-10));
	return std::nearbyint(value/step)*step;
}
/// @brief Expected decoded PNG bytes, including the historical UNORM quantization before its CPU sRGB encoding.
auto expectedBytes(unsigned chain, ToneMapping settings, bool unorm) {
	auto color = postColor(chain);
	if (settings.op != ToneMappingOperator::none) color = neutral(color,settings.exposure);
	std::array<int,4> result{0,0,0,255};
	for (auto c : std::views::iota(0U,3U)) {
		double value = std::clamp(color[c],0.0,1.0);
		if (unorm && settings.op == ToneMappingOperator::none) value = std::round(half(value)*255)/255;
		result[c] = int(std::round(srgb(value)*255));
	}
	return result;
}
/// @brief Fresh startup must allocate no optional output resource and record no Neutral pass on any backend.
void freshDefault(const Renderer &renderer) {
	require(renderer.toneMapResourceCounts() == std::array<std::size_t,2>{0,0},"fresh default live/retired count");
	for (const auto &t : renderer.targets) {
		require(t.postImage.image == VK_NULL_HANDLE && !t.toneMapLayout.valid() && !t.toneMapPipelineLayout.valid() &&
			!t.toneMapPipeline.pipeline.valid() && !t.toneMapPool.descriptorPool.valid() && t.toneMapSet == VK_NULL_HANDLE &&
			!t.toneMapGuiLayout.valid() && !t.toneMapGuiPipeline.pipeline.valid() && t.toneMapGeneration==0 && !t.hdrSampled,"fresh default object absence");
	}
	require(std::ranges::count(renderer.lastRecordedPassOrder(),Renderer::RecordedPass::tone_mapping)==0,"fresh default pass absence");
}
/// @brief Setters must preserve state after failure; valid AA remains startup-only, invalid AA retains error precedence.
void invalidSetters(simple::RenderSystem &render, ToneMapping good) {
	const auto bad = render.setToneMapping({ToneMappingOperator::khronos_neutral,0});
	require(!bad && bad.error()==Error::invalid_argument && render.toneMapping().op==good.op &&
		render.toneMapping().exposure==good.exposure,"in-flight invalid setter preserves settings");
	const auto late = render.setAntialiasing(Antialiasing::none);
	require(!late && late.error()==Error::already_initialized,"late AA unchanged");
	const auto unknown = render.setAntialiasing(static_cast<Antialiasing>(999));
	require(!unknown && unknown.error()==Error::invalid_argument,"unknown AA error precedence");
}
/// @brief Queue ordinary frames; no capture, explicit host fence wait or device/queue idle enters this transition helper.
void queue(simple::Engine &engine, ToneMapping settings) {
	auto &render = engine.renderSystem(); auto &renderer = render.forward();
	const auto waits = renderer.deviceWaitIdleCount();
	const auto live = renderer.toneMapResourceCounts()[0];
	require(bool(render.setToneMapping(settings)),"in-flight valid setter");
	if (settings.op==ToneMappingOperator::none && live) {
		const auto counts = renderer.toneMapResourceCounts();
		require(counts[0]==0 && counts[1]>=live,"disable retains entire submitted owner bundle in retirement");
		require(renderer.lastSubmitSerial()>renderer.completedSubmitSerial(),"disable occurs before last-use fence collection");
	}
	invalidSetters(render,settings);
	const auto receipts = renderer.outputDiagnostics.size();
	require(bool(engine.renderFrame()) && render.lastRenderedWindowCount()==2,"ordinary two-window transition submission");
	if (renderer.debugOutputReadback) {
		require(renderer.outputDiagnostics.size()==receipts+2,"one immutable receipt per submitted window");
		for (auto i : {receipts,receipts+1}) {
			const auto &frame=renderer.outputDiagnostics[i];
			require(frame.settings.op==settings.op && frame.settings.exposure==settings.exposure,"next recorded frame uses requested settings");
		}
	}
	require(renderer.deviceWaitIdleCount()==waits,"no new idle wait during transitions");
	const auto counts = renderer.toneMapResourceCounts();
	std::println("transition.op={} exposure={} submit={} completed={} live_objects={} pending_objects={}",
		int(settings.op),settings.exposure,renderer.lastSubmitSerial(),renderer.completedSubmitSerial(),counts[0],counts[1]);
	// Two windows x at most eight objects per bundle; at most two uncollected generations in the two-slot queue.
	require(counts[0]<=16 && counts[1]<=32,"bounded live and retired tone-map objects");
}
/// @brief Validate independently known output and actual pass order only after ordinary slot fences complete.
void inspect(Renderer &renderer, unsigned chain, bool unorm, const std::filesystem::path &directory) {
	unsigned index{};
	std::array<std::vector<unsigned char>,2> implicit;
	for (const auto &frame : renderer.outputDiagnostics) {
		require(frame.serial && frame.serial<=renderer.completedSubmitSerial(),"diagnostic covered by completed fence");
		const bool mapped = frame.settings.op!=ToneMappingOperator::none;
		require(frame.encoded==(!unorm || mapped) && frame.preserveAlpha==mapped,"immutable per-frame encoding policy");
		const auto post = std::ranges::find(frame.passes,Renderer::RecordedPass::post_process);
		const auto tone = std::ranges::find(frame.passes,Renderer::RecordedPass::tone_mapping);
		require((post!=frame.passes.end())==(chain!=0) && (tone!=frame.passes.end())==mapped,"actual pass presence");
		if (mapped && chain) require(post<tone,"vvppl -> Neutral recorded ordering");
		const auto scene = std::ranges::find(frame.passes,renderer.rendererBackend()==RendererBackend::forward ?
			Renderer::RecordedPass::forward_color : Renderer::RecordedPass::forward_transparency);
		require(scene!=frame.passes.end() && (!mapped || scene<tone) && (!chain || scene<post),"scene precedes output passes");
		const auto path = directory / std::format("frame-{:03}-serial-{}.png",index,frame.serial);
		require(bool(renderer.writeOutputDiagnostic(frame,path)),"fenced PNG conversion with old frame policy");
		const auto pixels = test::imagePixels(path);
		require(pixels && pixels->size()==std::size_t(frame.extent.width)*frame.extent.height*4,"recorded extent and alpha dimensions");
		const auto expected = expectedBytes(chain,frame.settings,unorm);
		int error{};
		const bool gui=std::ranges::find(frame.passes,Renderer::RecordedPass::gui)!=frame.passes.end();
		const auto guiFormat=renderer.targets.front().swapchain.guiFormat;
		const bool srgbGui=guiFormat==VK_FORMAT_B8G8R8A8_SRGB || guiFormat==VK_FORMAT_R8G8B8A8_SRGB;
		for (auto i : std::views::iota(std::size_t{},pixels->size())) {
			// Independently defined GUI rectangle is excluded from the whole-scene mask and checked against authored bytes.
			const bool widget=gui && (i/4)%frame.extent.width<8 && (i/4)/frame.extent.width<8;
			const int widgetByte=i%4==3 ? 255 : (!mapped && (!frame.encoded || srgbGui) ? int(std::round(srgb(128.0/255)*255)) : 128);
			error = std::max(error,std::abs(int((*pixels)[i])-(widget ? widgetByte : expected[i%4])));
			if (i%4==3) require((*pixels)[i]==255,"exact opaque alpha");
		}
		std::println("frame={} serial={} window={} extent={}x{} expected={},{},{} actual={},{},{} maximum_byte_error={} encoded={}",
			index,frame.serial,frame.window.value,frame.extent.width,frame.extent.height,expected[0],expected[1],expected[2],
			(*pixels)[0],(*pixels)[1],(*pixels)[2],error,frame.encoded);
		require(error<=1,"matrix CPU oracle within 1/255 on every scene-only pixel");
		// First two submissions per window differ only by explicit ToneMapping{}; this is an API check, not preservation evidence.
		if (index<2) implicit[index]=*pixels;
		if (index>=2 && index<4) require(*pixels==implicit[index-2],"default equals explicit default byte-for-byte");
		++index;
	}
	require(index>=60,"all lifecycle receipts present");
}
/// @brief All backends use 4x scene coverage, independent windows, resize and bounded 24-toggle retirement stress.
void matrix(RendererBackend backend, unsigned chain, Antialiasing aa, bool unorm, bool noMutable) {
	auto engine = simple::Engine{simple::Windows{.value={WindowDesc{.id="a",.extent={48,40},.visible=false},
		WindowDesc{.id="b",.extent={64,48},.visible=false}}}};
	auto &render = engine.renderSystem(); auto &renderer = render.forward();
	require(bool(render.setRendererBackend(backend)) && bool(render.setAntialiasing(aa)),"matrix startup selections");
	render.setBackgroundColor({Vec3{background[0],background[1],background[2]}});
	if (chain) render.setPostProcessSetup([chain](vvppl::PostProcessing &post) {
		if (chain==2) { auto &grade = post.addColorGrade(); for (auto c : std::views::iota(0U,3U)) grade.gain[c]=float(gains[c]); }
		if (chain==3) post.addTonemap().exposure=.85F;
	});
	require(bool(engine.init()) && test::renderFrames(engine,2),"matrix graphics startup");
	presentation(renderer);
#ifndef NDEBUG
	require(renderer.validationActive(),"Debug Khronos validation active");
#endif
	freshDefault(renderer);
	require(renderer.sampleCount()==(aa==Antialiasing::msaa4 ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT),"actual selected scene sample count");
	for (const auto &t : renderer.targets) {
		require(unorm==(t.swapchain.imageFormat==VK_FORMAT_B8G8R8A8_UNORM || t.swapchain.imageFormat==VK_FORMAT_R8G8B8A8_UNORM),"actual requested surface format");
		if (noMutable) require(t.imageViews.ownedGuiViews.empty() && !renderer.device.swapchainMutableFormat,"actual mutable extension exclusion");
	}
	const auto directory = root / (unorm?"unorm":noMutable?"no-mutable":"srgb") / std::format("backend-{}-chain-{}-aa-{}",int(backend),chain,int(aa));
	std::filesystem::create_directories(directory);
	renderer.debugOutputReadback=true;
	require(bool(engine.renderFrame()),"implicit default ordinary frame");
	const auto premature=renderer.writeOutputDiagnostic(renderer.outputDiagnostics.back(),directory/"premature.png");
	require(!premature && premature.error()==Error::missing_object,"unobserved submission cannot expose diagnostic pixels");
	queue(engine,{});
	const ToneMapping one{ToneMappingOperator::khronos_neutral,1}, two{one.op,2};
	for (const auto settings : {one,two,ToneMapping{},one}) queue(engine,settings);

	// Successful creation generations prove a rebuild even when the driver reuses destroyed Vulkan handle values.
	auto &a=renderer.targets.front(); auto &b=renderer.targets.back();
	const auto otherHdr=static_cast<VkImage>(b.hdrImage.image);
	const auto otherSwap=static_cast<VkSwapchainKHR>(b.swapchain.swapchain);
	const auto pipeline=static_cast<VkPipeline>(a.toneMapPipeline.pipeline);
	const auto outputGeneration=a.toneMapGeneration;
	SDL_ClearError();
	const bool resizeRequested=SDL_SetWindowSize(a.window,80,56);
	const std::string resizeError{SDL_GetError()};
	SDL_ClearError();
	const bool resizeSynced=resizeRequested && SDL_SyncWindow(a.window);
	const std::string syncError{SDL_GetError()};
	require(resizeRequested,"resize real SDL window");
	const auto resizeDeadline=std::chrono::steady_clock::now()+std::chrono::seconds{5};
	auto drawable=renderer.currentWindowPixelExtent(a);
	// SDL's short native synchronization timeout may precede the requested drawable acknowledgement.
	while (drawable.width!=80 || drawable.height!=56) {
		require(bool(engine.windowSystem().poll()),"resize window events");
		drawable=renderer.currentWindowPixelExtent(a);
		if (std::chrono::steady_clock::now()>=resizeDeadline) break;
		std::this_thread::sleep_for(std::chrono::milliseconds{1});
	}
	std::println("matrix.resize.backend={} chain={} aa={} requested={} synced={} drawable={}x{} native_flags={} request_error={} sync_error={}",
		int(backend),chain,int(aa),resizeRequested,resizeSynced,drawable.width,drawable.height,SDL_GetWindowFlags(a.window),resizeError,syncError);
	require(drawable.width==80 && drawable.height==56,"native drawable confirms requested resize");
	// A surface can temporarily reject acquisition after its native resize acknowledgement.
	require(test::renderFrames(engine,1,std::chrono::seconds{10},2),"ordinary resize frame presents both windows");
	std::println("matrix.resize.swapchain={}x{} pipeline.live={} pipeline.reused={} generation.before={} generation.after={}",a.swapchain.extent.width,a.swapchain.extent.height,
		a.toneMapPipeline.pipeline!=VK_NULL_HANDLE,static_cast<VkPipeline>(a.toneMapPipeline.pipeline)==pipeline,outputGeneration,a.toneMapGeneration);
	require(a.swapchain.extent.width==80 && a.swapchain.extent.height==56 &&
		a.toneMapPipeline.pipeline!=VK_NULL_HANDLE && a.toneMapGeneration>outputGeneration,"resized output pipeline rebuilt");
	if (chain) require(a.postImage.extent.width==80 && a.postImage.extent.height==56 &&
		a.postImage.image!=VK_NULL_HANDLE,"resized HDR postImage rebuilt");
	require(static_cast<VkImage>(b.hdrImage.image)==otherHdr && static_cast<VkSwapchainKHR>(b.swapchain.swapchain)==otherSwap &&
		b.swapchain.extent.width==64 && b.swapchain.extent.height==48,"other window survives resize");
	// The API is renderer-wide, between frame calls. Record different selections for the two windows without adding a per-window API.
	require(bool(render.setToneMapping(two)) && bool(renderer.renderFrame(a)),"window a Neutral frame");
	require(bool(render.setToneMapping({})) && bool(renderer.renderFrame(b)),"window b legacy frame");
	const auto &mapped=renderer.outputDiagnostics[renderer.outputDiagnostics.size()-2];
	const auto &legacy=renderer.outputDiagnostics.back();
	require(mapped.window==a.handle && mapped.settings.op==two.op && mapped.settings.exposure==two.exposure &&
		legacy.window==b.handle && legacy.settings.op==ToneMappingOperator::none,"independent recorded selections for two windows");
	engine.gui().draw([] {
		const auto p=ImGui::GetMainViewport()->Pos;
		ImGui::GetBackgroundDrawList()->AddRectFilled(p,{p.x+8,p.y+8},IM_COL32(128,128,128,255));
	});
	// Exercise the same minimized-window snapshot contract as RenderFrameCountTests, retaining the target's output owners.
	auto *window=engine.windowSystem().findWindow("b");
	require(window!=nullptr && bool(render.setToneMapping(one)),"minimize fixture");
	const auto retainedSwap=static_cast<VkSwapchainKHR>(b.swapchain.swapchain);
	window->info().minimized=true;
	require(bool(engine.renderFrame()) && render.lastRenderedWindowCount()==1,"minimized window skips submission");
	window->info().minimized=false;
	require(bool(engine.renderFrame()) && render.lastRenderedWindowCount()==2 &&
		static_cast<VkSwapchainKHR>(b.swapchain.swapchain)==retainedSwap,"resumed window retains its target");
	for (auto toggle : std::views::iota(0U,24U)) queue(engine,toggle%2 ? ToneMapping{} : (toggle%4 ? two : one));

	// Leave the current settings different from older enabled receipts before converting their recorded UNORM pixels.
	require(bool(render.setToneMapping({})),"final disable"); renderer.debugOutputReadback=false;
	require(test::renderFrames(engine,3),"ordinary slot reuse observes every diagnostic/retirement fence");
	require(renderer.toneMapResourceCounts()==std::array<std::size_t,2>{0,0},"all optional objects drain after last-use fences");
	inspect(renderer,chain,unorm,directory);
	render.waitIdle(); // Teardown boundary only: the final ordinary frames can still reference ImGui's own buffers.
	engine.gui().shutdownVulkan(); render.shutdown();
	require(renderer.validationErrorCount()==0,"zero validation errors including teardown");
	std::println("matrix.backend={} chain={} aa={} result=pass skips=0",int(backend),chain,int(aa));
}
} // namespace

/// @brief Actual sRGB/UNORM/no-mutable presentation runs; no GPU image is ever an analytic reference.
int main(int argc,char **argv) try {
	const bool unorm=argc==2 && std::string_view{argv[1]}=="--unorm";
	const bool noMutable=argc==2 && std::string_view{argv[1]}=="--no-mutable";
	fixedVectors();
	// Independent ACES fixed scalar: x=1 gives 2.54/3.16=127/158 before Neutral.
	require(std::abs((2.51+.03)/(2.43+.59+.14)-127.0/158)<1e-15,"ACES rational fixed vector");
	for (auto backend : {RendererBackend::forward,RendererBackend::deferred,RendererBackend::hybrid_gi}) {
		for (auto chain : std::views::iota(0U,4U)) {
			for (auto aa : {Antialiasing::none,Antialiasing::msaa4}) matrix(backend,chain,aa,unorm,noMutable);
		}
	}
	std::println("tone_mapping_matrix.result=pass backends=3 chains=4 toggles_per_case=24 skips=0"); return 0;
} catch (const std::exception &error) { std::println(stderr,"tone_mapping_matrix.failure={}",error.what()); return 1; }
