/** @file @brief Captures the real airport paving and quantifies painted surface coverage at near and distant approach angles. */
import std;
import VVEngine;
import Island.Scene;
import VVE.TestSupport;

namespace {
constexpr vve::PixelExtent extent{1920,1080}; ///< Full-HD captures retain useful subpixel coverage at three kilometres.

/** @brief Reports a stable failure name instead of leaving a missing capture as the only diagnostic. */
void check(bool condition,std::string_view name) {
	if (!condition) { throw std::runtime_error{std::string{name}}; }
}
/** @brief Measures projected paint area independently from GPU rasterization for fully visible far approaches. */
double projectedPaintArea(const std::array<vve::MeshDescriptor,18> &meshes,const vve::Camera &camera) {
	const auto forward=vve::math::normalize(camera.forward.value);
	const auto right=vve::math::normalize(vve::math::cross(forward,{0,1,0}));
	const auto up=vve::math::cross(right,forward);
	const double focal=extent.height/(2.0*std::tan(camera.fov_y.radians*0.5));
	const auto project=[&](vve::Vec3 p) {
		const auto relative=vve::math::subtract(p,camera.position.value);
		const double depth=vve::math::dot(relative,forward);
		return std::pair{focal*vve::math::dot(relative,right)/depth,focal*vve::math::dot(relative,up)/depth};
	};
	double area{};
	for (const auto material : {8U,9U}) { const auto &mesh=meshes[material];
		for (std::size_t i{}; i<mesh.indices.size(); i+=3U) {
			const auto a=project(mesh.positions[mesh.indices[i]]),b=project(mesh.positions[mesh.indices[i+1U]]),c=project(mesh.positions[mesh.indices[i+2U]]);
			area+=std::abs((b.first-a.first)*(c.second-a.second)-(b.second-a.second)*(c.first-a.first))*0.5;
		}
	}
	return area;
}
/** @brief Converts displayed byte values to approximate linear coverage before comparing subpixel antialiasing contributions. */
double linear(unsigned char value) {
	const double encoded=value/255.0;
	return encoded<=0.04045?encoded/12.92:std::pow((encoded+0.055)/1.055,2.4);
}
/** @brief Runs the environment-selected backend through both actual ground LODs and several small camera shifts. */
void captureApproaches() {
	auto engine=vve::EngineBuilder<>{}.applicationName("island-marking-tests")
		.addWindow(vve::WindowSetup{}.id("main").title("Island markings").extent(extent).visible(false)).build();
	auto world=engine.world(); auto &render=world.get<vve::RenderSystem>();
	vve::test::configureRenderer(render);
	check(render.setAntialiasing(vve::Antialiasing::msaa4).has_value(),"request_four_sample_coverage");
	check(engine.init().has_value(),"hidden_engine_initializes");
	const std::string backend=render.rendererBackend()==vve::RendererBackend::deferred?"deferred":"forward";
	const auto directory=std::filesystem::path{VVE_TEST_TMP_DIR}/backend;
	std::filesystem::create_directories(directory);
	render.setBackgroundColor({.value={0.02F,0.02F,0.02F}});
	std::size_t captures{};
	// One hundred metres exercises close paint; 1.5 km and 3 km exercise subpixel marks and the distant scenery LOD.
	for (const float distance : {100.0F,1500.0F,3000.0F}) {
		const bool detailed=distance<2000;
		const auto meshes=island::groundMeshBatches(island::islandCenter(0),detailed);
		render.clearScene(); std::vector<vve::RenderObjectHandle> paint;
		for (const auto material : {4U,5U,8U,9U}) {
			if (meshes[material].indices.empty()) { continue; }
			auto mesh=meshes[material];
			// Diagnostic colours isolate geometry coverage from textures, shadows and exposure changes.
			mesh.base_color_texture.clear(); mesh.color.value=material>=8U?vve::Vec3{1,1,1}:vve::Vec3{0.02F,0.02F,0.02F};
			const auto object=render.addMesh(std::move(mesh)); check(object.has_value(),"actual_ground_batch_uploads");
			check(render.setObjectUnlit(*object,true).has_value(),"coverage_fixture_is_unlit");
			check(render.setObjectCastsShadow(*object,false).has_value(),"coverage_fixture_has_no_shadow_pass");
			if (material>=8U) { paint.push_back(*object); }
		}
		check(!paint.empty(),"actual_lod_contains_runway_paint");
		for (const float shift : {-0.35F,0.0F,0.35F}) {
			const auto camera=vve::Camera::lookAt({.value={distance*0.17F+shift,island::runwayHeight+distance*0.18F,distance}},
				{.value={0,island::runwayHeight,-100}}, {}, {}, {.near_plane=0.1F,.far_plane=12000});
			render.setCamera(camera);
			const auto prefix=std::format("approach-{}m-shift-{:.2f}",static_cast<int>(distance),shift);
			const auto capture=[&](std::string_view suffix) {
				const auto frame=engine.step(); check(frame && *frame!=vve::FrameStatus::stopped,"bounded_capture_frame_renders");
				const auto path=directory/(prefix+std::string{suffix}+".png");
				check(render.captureFrameToPng(path).has_value(),"ground_capture_writes_png");
				const auto pixels=vve::test::imagePixels(path);
				check(pixels && pixels->size()==std::size_t{extent.width}*extent.height*4U,"ground_capture_has_expected_extent");
				++captures; return *pixels;
			};
			for (const auto object : paint) { check(render.setObjectVisible(object,true).has_value(),"paint_enabled_for_coverage"); }
			const auto visible=capture("-paint");
			for (const auto object : paint) { check(render.setObjectVisible(object,false).has_value(),"paint_hidden_for_baseline"); }
			const auto baseline=capture("-baseline");
			std::size_t changed{}; double signal{};
			// Only the white material changes between captures; signal remains useful even when individual lines are subpixel.
			for (std::size_t pixel{}; pixel<visible.size(); pixel+=4U) {
				const int difference=int(visible[pixel])-int(baseline[pixel]);
				if (difference>2) { ++changed; }
				signal+=std::max(0.0,linear(visible[pixel])-linear(baseline[pixel]));
			}
			check(changed>0 && signal>0.5,"runway_paint_remains_visible_at_each_approach_and_camera_shift");
			const double projected=distance>=1500?projectedPaintArea(meshes,camera):0;
			// Far quads lie completely in view. Broad bounds permit MSAA sample quantization without accepting nearly absent paint.
			if (projected>0) { check(signal/projected>0.25 && signal/projected<2.5,"far_paint_signal_matches_projected_geometry"); }
			std::println("[IslandDeliveryMarkingTests] renderer={} distance={} detailed={} shift={} changed_pixels={} linear_signal={} projected_area={} capture={}",
				backend,distance,detailed,shift,changed,signal,projected,(directory/(prefix+"-paint.png")).string());
		}
	}
	check(captures==18 && render.renderedFrameCount()>=18,"all_bounded_marking_captures_rendered");
	std::println("[IslandDeliveryMarkingTests] passed=true renderer={} msaa=4 captures={}",backend,captures);
}
}

/** @brief Emits a named failure and leaves all PNG fixtures in the test output directory for visual inspection. */
int main() {
	try { captureApproaches(); return 0; }
	catch (const std::exception &error) { std::cerr<<"[IslandDeliveryMarkingTests] passed=false error="<<error.what()<<'\n'; return 1; }
}
