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

export enum class VeEngineVersion {
    v3
};

export enum class VeResult {
    success = 0,
    invalid_argument,
    file_not_found,
    io_error,
    unsupported_version,
    internal_error
};

export struct VEApplicationName {
    std::string value;
};

export struct VEEnableValidation {
    bool value = false;
};

export class VEEngineConfig {
public:
    VEEngineConfig() = default;

    template <typename... TOptions>
    explicit VEEngineConfig(TOptions&&... options) {
        (set(std::forward<TOptions>(options)), ...);
    }

    template <typename TOption>
    VEEngineConfig& set(TOption&& option) {
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

export class VVE_API VEEngine {
public:
    explicit VEEngine(VeEngineVersion version, VEEngineConfig config = {});

    template <typename... TOptions>
        requires (sizeof...(TOptions) > 0)
    explicit VEEngine(VeEngineVersion version, TOptions&&... options)
        : VEEngine(version, VEEngineConfig(std::forward<TOptions>(options)...)) {
    }

    ~VEEngine();

    VEEngine(const VEEngine&) = delete;
    VEEngine(VEEngine&&) = delete;
    VEEngine& operator=(const VEEngine&) = delete;
    VEEngine& operator=(VEEngine&&) = delete;

    [[nodiscard]] virtual std::expected<int, VeResult> getVersionMajor() const noexcept;
    [[nodiscard]] virtual std::expected<std::string, VeResult> loadFile(
        const std::filesystem::path& file_path) const;

protected:
    VEEngine() noexcept;

private:
    std::unique_ptr<VEEngine> impl_;
};
