module;

#if defined(_WIN32)
#if defined(VVE_ENGINE_BUILD)
#define VVE_API __declspec(dllexport)
#else
#define VVE_API __declspec(dllimport)
#endif
#else
#define VVE_API
#endif

export module VEEngine;
import std;
export import :ECS;
export import :System;
export import :Handle;
export import :Math;
export import :Result;

export namespace vve::detail {

class EngineImpl;

} // namespace vve::detail

export namespace vve {

// Engine configuration options 

enum class EngineVersion {
    v3
};

enum class GraphicsApi {
    vulkan,
    direct3d12,
    metal
};

enum class RendererKind {
    forward_renderer,
    deferred_renderer,
    path_tracing
};

enum class ShadowKind {
    none,
    shadow_map,
    ray_traced
};

enum class FrameStatus {
    continue_running = 0,
    should_close
};

/// @brief Configuration for creating an Engine instance. Can be constructed with arbitrary options using the set() method or the variadic constructor.
struct ApplicationName {
    std::string value;
};

struct EnableValidation {
    bool value = false;
};

struct PreferredGraphicsApi {
    GraphicsApi value = GraphicsApi::vulkan;
};

struct PreferredRenderer {
    RendererKind value = RendererKind::forward_renderer;
};

struct PreferredShadow {
    ShadowKind value = ShadowKind::none;
};

struct EnableImGui {
    bool value = true;
};

struct WindowDesc {
    std::string id{"main"};
    std::string title{"ViennaVulkanEngine"};
    std::uint32_t width{1280};
    std::uint32_t height{720};
    bool resizable{true};
    bool visible{true};
};

struct Windows {
    std::vector<WindowDesc> value{};
};

class EngineConfig {
public:
    EngineConfig() = default;

    template <typename... TOptions>
    explicit EngineConfig(TOptions&&... options) {
        (set(std::forward<TOptions>(options)), ...);
    }

    template <typename TOption>
    EngineConfig& set(TOption&& option) {
        using TStoredOption = std::remove_cvref_t<TOption>;
        options_[std::type_index(typeid(TStoredOption))] = std::forward<TOption>(option);
        return *this;
    }

    template <typename TOption>
    [[nodiscard]] std::optional<TOption> tryGet() const {
        const auto entry = options_.find(std::type_index(typeid(TOption)));
        if (entry == options_.end()) {
            return std::nullopt;
        }

        if (const auto* value = std::any_cast<TOption>(&entry->second)) {
            return *value;
        }

        return std::nullopt;
    }

private:
    std::unordered_map<std::type_index, std::any> options_;
};

class VVE_API Engine {
public:
    explicit Engine(EngineVersion version, EngineConfig config = {});

    template <typename... TOptions>
        requires (sizeof...(TOptions) > 0)
    explicit Engine(EngineVersion version, TOptions&&... options)
        : Engine(version, EngineConfig(std::forward<TOptions>(options)...)) {
    }

    ~Engine();

    Engine(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine& operator=(Engine&&) = delete;

    [[nodiscard]] std::expected<void, Result> init();
    [[nodiscard]] std::expected<void, Result> run();
    [[nodiscard]] std::expected<FrameStatus, Result> step();
    [[nodiscard]] std::expected<bool, Result> isInitialized() const noexcept;
    [[nodiscard]] std::expected<int, Result> getVersionMajor() const noexcept;
    [[nodiscard]] std::expected<void, Result> loadFile(
        const std::filesystem::path& file_path);

private:
    std::unique_ptr<detail::EngineImpl> impl_{nullptr};
};

} // namespace vve

namespace vve::detail {

class EngineImpl {
public:
    virtual ~EngineImpl() = default;

    [[nodiscard]] virtual std::expected<void, vve::Result> init() = 0;
    [[nodiscard]] virtual std::expected<void, vve::Result> run() = 0;
    [[nodiscard]] virtual std::expected<vve::FrameStatus, vve::Result> step() = 0;
    [[nodiscard]] virtual bool isInitialized() const noexcept = 0;
    [[nodiscard]] virtual std::expected<int, vve::Result> getVersionMajor() const noexcept = 0;
    [[nodiscard]] virtual std::expected<void, vve::Result> loadFile(
        const std::filesystem::path& file_path) = 0;
};

} // namespace vve::detail
