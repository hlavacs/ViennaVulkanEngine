import std;
import VVEngine;

/// @file
/// @brief Checks consuming builder chains and reusable copyable builders without initializing a window or GPU.

namespace {

/// @brief A move-only system that requires an explicitly supplied resource.
struct OwnedSystem {
	std::unique_ptr<int> value; ///< Resource that must reach the engine without copying or replacement.

	/// @brief Accepts ownership; intentionally provides no default constructor.
	explicit OwnedSystem(std::unique_ptr<int> resource) : value{std::move(resource)} {}
};

/// @brief Checks whether a builder can preserve its systems when building from an lvalue.
template <typename T> concept BuildsFromLvalue = requires(T &builder) { builder.build(); };
static_assert(!std::default_initializable<OwnedSystem> && !std::copy_constructible<OwnedSystem>);
static_assert(!BuildsFromLvalue<vve::EngineBuilder<OwnedSystem>>);
static_assert(BuildsFromLvalue<vve::EngineBuilder<int>>);

/// @brief Verifies direct construction, all fluent rvalue setters and explicit consumption of a named builder.
bool transfersOwnedSystems() {
	auto direct_resource = std::make_unique<int>(11);
	const auto *direct_pointer = direct_resource.get();
	auto direct = vve::Engine<OwnedSystem>{vve::makeUserSystems(OwnedSystem{std::move(direct_resource)})};
	const bool direct_ok = direct.world().get<OwnedSystem>().value.get() == direct_pointer;

	auto chained_resource = std::make_unique<int>(22);
	const auto *chained_pointer = chained_resource.get();
	auto chained = vve::EngineBuilder<OwnedSystem>{vve::makeUserSystems(OwnedSystem{std::make_unique<int>(0)})}
		.applicationName("move-only-builder-tests")
		.maxFrames(vve::MaxFrames{.value = vve::FrameCount{.value = 1}})
		.windows(vve::WindowSetups{})
		.addWindow(vve::WindowSetup{}.id("unused"))
		.userSystems(vve::makeUserSystems(OwnedSystem{std::move(chained_resource)}))
		.build();
	const bool chained_ok = chained.world().get<OwnedSystem>().value.get() == chained_pointer;

	auto named_resource = std::make_unique<int>(33);
	const auto *named_pointer = named_resource.get();
	auto builder = vve::EngineBuilder<OwnedSystem>{vve::makeUserSystems(OwnedSystem{std::move(named_resource)})};
	(void)builder.applicationName("named-move-only-builder-tests");
	auto named = std::move(builder).build();
	const bool named_ok = named.world().get<OwnedSystem>().value.get() == named_pointer;
	std::println("[engine_builder_move] direct={} chained={} named={} values={},{},{}", direct_ok, chained_ok,
		named_ok, direct_ok ? *direct.world().get<OwnedSystem>().value : -1,
		chained_ok ? *chained.world().get<OwnedSystem>().value : -1,
		named_ok ? *named.world().get<OwnedSystem>().value : -1);
	return direct_ok && chained_ok && named_ok;
}

/// @brief Const and mutable copyable builders keep their systems available for repeated builds.
bool preservesCopyableSystems() {
	auto builder = vve::EngineBuilder<int>{vve::makeUserSystems(44)};
	auto first = builder.build();
	auto second = std::as_const(builder).build();
	first.world().get<int>() = 55;
	auto third = builder.build();
	const bool independent = second.world().get<int>() == 44 && third.world().get<int>() == 44;
	std::println("[engine_builder_copy] independent={} values={},{},{}", independent,
		first.world().get<int>(), second.world().get<int>(), third.world().get<int>());
	return independent;
}

} // namespace

/// @brief Emits deterministic ownership diagnostics and fails at the affected builder path.
int main() {
	if (!transfersOwnedSystems()) { return 1; }
	if (!preservesCopyableSystems()) { return 2; }
	return 0;
}
