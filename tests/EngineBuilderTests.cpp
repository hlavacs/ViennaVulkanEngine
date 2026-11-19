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
	bool direct_ok{};
	{
		auto direct = vve::Engine<OwnedSystem>{vve::makeUserSystems(OwnedSystem{std::move(direct_resource)})};
		direct_ok = direct.world().get<OwnedSystem>().value.get() == direct_pointer;
	}

	auto chained_resource = std::make_unique<int>(22);
	const auto *chained_pointer = chained_resource.get();
	bool chained_ok{};
	{
		auto chained = vve::EngineBuilder<OwnedSystem>{vve::makeUserSystems(OwnedSystem{std::make_unique<int>(0)})}
			.applicationName("move-only-builder-tests")
			.maxFrames(vve::MaxFrames{.value = vve::FrameCount{.value = 1}})
			.windows(vve::WindowSetups{})
			.addWindow(vve::WindowSetup{}.id("unused"))
			.userSystems(vve::makeUserSystems(OwnedSystem{std::move(chained_resource)}))
			.build();
		chained_ok = chained.world().get<OwnedSystem>().value.get() == chained_pointer;
	}

	auto named_resource = std::make_unique<int>(33);
	const auto *named_pointer = named_resource.get();
	auto builder = vve::EngineBuilder<OwnedSystem>{vve::makeUserSystems(OwnedSystem{std::move(named_resource)})};
	(void)builder.applicationName("named-move-only-builder-tests");
	auto named = std::move(builder).build();
	const bool named_ok = named.world().get<OwnedSystem>().value.get() == named_pointer;
	std::println("[engine_builder_move] direct={} chained={} named={}", direct_ok, chained_ok, named_ok);
	return direct_ok && chained_ok && named_ok;
}

/// @brief Const and mutable copyable builders keep their systems available for repeated builds.
bool preservesCopyableSystems() {
	auto builder = vve::EngineBuilder<int>{vve::makeUserSystems(44)};
	{
		auto first = builder.build();
		first.world().get<int>() = 55;
	}
	const auto read = [](auto &source) {
		auto engine = source.build();
		return engine.world().template get<int>();
	};
	const auto second = read(std::as_const(builder));
	const auto third = read(builder);
	const bool independent = second == 44 && third == 44;
	std::println("[engine_builder_copy] independent={} values={},{}", independent, second, third);
	return independent;
}

/// @brief Rejects overlapping construction across facade specializations and allows reuse after destruction.
bool enforcesSingleInstance() {
	bool rejected{};
	{
		vve::Engine<> first{};
		try {
			vve::Engine<int> second{};
		} catch (const std::logic_error &) {
			rejected = true;
		}
	}
	auto replacement = vve::EngineBuilder<int>{vve::makeUserSystems(44)}.build();
	const bool reusable = replacement.world().get<int>() == 44;
	std::println("[engine_instance] second_rejected={} replacement_created={}", rejected, reusable);
	return rejected && reusable;
}

} // namespace

/// @brief Emits deterministic ownership diagnostics and fails at the affected builder path.
int main() {
	if (!transfersOwnedSystems()) { return 1; }
	if (!preservesCopyableSystems()) { return 2; }
	if (!enforcesSingleInstance()) { return 3; }
	return 0;
}
