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
export import :Handle;

export namespace vve::detail {

class EngineImpl;

} // namespace vve::detail

export namespace vve {

enum class EngineVersion {
    v3
};

enum class Result {
    success = 0,
    not_initialized,
    already_initialized,
    invalid_argument,
    file_not_found,
    io_error,
    unsupported_version,
    internal_error
};

struct ApplicationName {
    std::string value;
};

struct EnableValidation {
    bool value = false;
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
    [[nodiscard]] std::expected<void, Result> step();
    [[nodiscard]] std::expected<bool, Result> isInitialized() const noexcept;
    [[nodiscard]] std::expected<int, Result> getVersionMajor() const noexcept;
    [[nodiscard]] std::expected<std::string, Result> loadFile(
        const std::filesystem::path& file_path) const;

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
    [[nodiscard]] virtual std::expected<void, vve::Result> step() = 0;
    [[nodiscard]] virtual bool isInitialized() const noexcept = 0;
    [[nodiscard]] virtual std::expected<int, vve::Result> getVersionMajor() const noexcept = 0;
    [[nodiscard]] virtual std::expected<std::string, vve::Result> loadFile(
        const std::filesystem::path& file_path) const = 0;
};

} // namespace vve::detail
