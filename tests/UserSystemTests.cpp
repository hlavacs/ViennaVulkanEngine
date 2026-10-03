import std;

import VVEngine;

/// @file
/// @brief Exercises system hooks, retry semantics and shared window frame data.

namespace {

struct SharedSystem {
   int value{0};
};

struct CountingSystem {
   int *init_count{};
   int *update_count{};
   std::uint64_t *last_frame{};
   std::size_t *last_window_count{};
   int *shared_value{};
	vve::WindowInfo *last_window{};	///< Window state received by the latest update hook.

   template <typename TWorld> std::expected<void, vve::Error> init(TWorld &world) {
      if (init_count != nullptr) { ++*init_count; }
      if (shared_value != nullptr) {
         auto &shared = world.template get<SharedSystem>();
         *shared_value = shared.value;
         shared.value = 77;
      }
      auto &render_system = world.template get<vve::RenderSystem>();
      render_system.clearScene();
      if (const auto result = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{}); !result) {
         return std::unexpected(result.error());
      }
      render_system.setCamera(vve::Camera{});
      render_system.setDirectionalLight(vve::Direction{}, vve::LinearColor{}, vve::LightIntensity{},
                                        vve::LinearColor{});
      return world.template get<vve::WindowSystem>().windowCount() == 0
                ? std::unexpected(vve::Error::missing_object)
                : std::expected<void, vve::Error>{};
   }

   template <typename TWorld, typename TWindowFrame>
   std::expected<void, vve::Error> update(TWorld &, const vve::FrameContext &frame,
                                          const TWindowFrame &window_frame) {
      if (update_count != nullptr) { ++*update_count; }
      if (last_frame != nullptr) { *last_frame = frame.frame_index.value; }
      if (last_window_count != nullptr) { *last_window_count = window_frame.windows.size(); }
		// Retain the delivered metadata for comparison with the configured window.
		if (last_window != nullptr && !window_frame.windows.empty()) {
			*last_window = window_frame.windows.front();
		}
      return window_frame.windows.empty() ? std::unexpected(vve::Error::missing_object)
                                          : std::expected<void, vve::Error>{};
   }
};

/// @brief Counts init calls and optionally fails the first attempt to exercise ordered retries.
template <bool FailFirst> struct InitRetrySystem {
   int *init_count{}; ///< Init attempts observed by the test.

   /// @brief Returns the same error that the facade must propagate without repeating earlier hooks.
   template <typename TWorld> std::expected<void, vve::Error> init(TWorld &) {
      ++*init_count;
      if (FailFirst && *init_count == 1) { return std::unexpected(vve::Error::io_error); }
      return {};
   }
};

/// @brief Verifies that a retry resumes at the failed hook and completed initialization stays idempotent.
bool hasInitRetryCoverage() {
   int a_init_count = 0;
   int b_init_count = 0;
   auto engine = vve::EngineBuilder<InitRetrySystem<false>, InitRetrySystem<true>>{}
                    .applicationName("user-system-init-retry-tests")
                    .addWindow(vve::WindowSetup{}
                                  .id("main")
                                  .title("user-system-init-retry-tests")
                                  .extent(vve::PixelExtent{.width = 64, .height = 64})
                                  .visible(false))
                    .userSystems(vve::makeUserSystems(InitRetrySystem<false>{&a_init_count},
                                                       InitRetrySystem<true>{&b_init_count}))
                    .build();

   const auto first = engine.init();
   const auto second = engine.init();
   std::println("[user_system_init_retry] first_ok={} second_ok={} A={} B={}",
                first.has_value(), second.has_value(), a_init_count, b_init_count);
   if (first || first.error() != vve::Error::io_error || !second ||
       a_init_count != 1 || b_init_count != 2) { return false; }
   return engine.init().has_value() && a_init_count == 1 && b_init_count == 2;
}

/// @brief Fails the first update so the next hook can verify that engine timing still advances.
struct UpdateRetrySystem {
   int update_count{0}; ///< Number of update attempts, including the failed one.
   vve::FrameContext last_frame{}; ///< Context received by the latest update hook.

   /// @brief Records each context and reports one recoverable update error.
   template <typename TWorld>
   std::expected<void, vve::Error> update(TWorld &, const vve::FrameContext &frame) {
      last_frame = frame;
      if (++update_count == 1) { return std::unexpected(vve::Error::io_error); }
      return {};
   }
};

/// @brief Verifies that a failed update consumes its frame index and the frame cap remains unchanged.
bool hasUpdateRetryCoverage() {
   auto engine = vve::EngineBuilder<UpdateRetrySystem>{}
                    .applicationName("user-system-update-retry-tests")
                    .maxFrames(vve::MaxFrames{.value = vve::FrameCount{.value = 2}})
                    .addWindow(vve::WindowSetup{}
                                  .id("main")
                                  .title("user-system-update-retry-tests")
                                  .extent(vve::PixelExtent{.width = 64, .height = 64})
                                  .visible(false))
                    .build();
   if (!engine.init()) { return false; }
   const auto first = engine.step();
   const auto &system = engine.world().get<UpdateRetrySystem>();
   const auto first_frame = system.last_frame;
   // Separate the polls even on platforms with a coarse steady clock.
   std::this_thread::sleep_for(std::chrono::milliseconds{2});
   const auto second = engine.step();
   const auto context = std::as_const(engine).frameContext();
   const bool stopped = second && *second == vve::FrameStatus::stopped;
   const bool first_delta_default = first_frame.delta_time.seconds == vve::DeltaTime{}.seconds;
   const bool second_delta_positive = system.last_frame.delta_time.seconds > 0;
   std::println("[user_system_update_retry] first_ok={} second_stopped={} updates={} first_frame={} "
                "first_delta_default={} second_frame={} second_delta_positive={} public_frame={}",
                first.has_value(), stopped, system.update_count, first_frame.frame_index.value,
                first_delta_default, system.last_frame.frame_index.value, second_delta_positive,
                context.frame_index.value);
   return !first && first.error() == vve::Error::io_error && stopped && system.update_count == 2 &&
          first_frame.frame_index.value == 0 && first_delta_default &&
          system.last_frame.frame_index.value == 1 && second_delta_positive &&
          context.frame_index.value == 1 && context.delta_time.seconds == system.last_frame.delta_time.seconds;
}

} // namespace

int main() {
	// Shared diagnostics retain the renderer's sentinel defaults through the facade.
	const vve::RenderShadowDepthSample sample{};
	if (sample.shadow_factor != 1.0F || sample.gpu_depth != -1.0F || sample.error != -1.0F) { return 16; }
   if (!hasInitRetryCoverage()) { return 11; }
   if (!hasUpdateRetryCoverage()) { return 12; }
   int init_count = 0;
   int update_count = 0;
   std::uint64_t last_frame = 99;
   std::size_t last_window_count = 0;
   int shared_value = 0;
	vve::WindowInfo last_window{};

   auto engine = vve::EngineBuilder<SharedSystem, CountingSystem>{}
                    .applicationName("user-system-tests")
                    .maxFrames(vve::MaxFrames{.value = vve::FrameCount{.value = 2}})
                    .addWindow(vve::WindowSetup{}
                                  .id("main")
                                  .title("user-system-tests")
                                  .extent(vve::PixelExtent{.width = 64, .height = 64})
                                  .visible(false))
                    .userSystems(vve::makeUserSystems(SharedSystem{.value = 42},
                                                       CountingSystem{.init_count = &init_count,
                                                                      .update_count = &update_count,
                                                                      .last_frame = &last_frame,
                                                                      .last_window_count = &last_window_count,
                                                                      .shared_value = &shared_value,
																	  .last_window = &last_window}))
                    .build();

   if (!engine.init()) { return 1; }
   auto world = engine.world();
   auto &render_system = world.get<vve::RenderSystem>();
   if (render_system.sceneMeshCount() != 1 || render_system.sceneMaterialCount() != 1 ||
       render_system.sceneInstanceCount() != 1) {
      return 8;
   }
   if (!render_system.hasSceneCamera() || !render_system.hasSceneDirectionalLight()) { return 9; }

   const auto first = engine.step();
   const auto second = engine.step();
   if (!first || !second || *first != vve::FrameStatus::running || *second != vve::FrameStatus::stopped) {
      return 4;
   }
   if (init_count != 1 || update_count != 2 || last_frame != 1 || last_window_count != 1) { return 5; }
   if (shared_value != 42) { return 6; }
   if (engine.world().get<SharedSystem>().value != 77) { return 7; }
   if (render_system.renderedFrameCount() != 2 || render_system.lastRenderedWindowCount() != 1) { return 10; }

	// Check the shared frame payload against the setup supplied to EngineBuilder.
	std::println("[user_system_window] id={} title={} extent={}x{}", last_window.id, last_window.title,
		last_window.extent.width, last_window.extent.height);
	if (last_window.id != "main") { return 13; }
	if (last_window.title != "user-system-tests") { return 14; }
	if (last_window.extent.width != 64 || last_window.extent.height != 64) { return 15; }

   return 0;
}
