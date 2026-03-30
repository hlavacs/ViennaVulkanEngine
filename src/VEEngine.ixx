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

export enum class VEEngineVersion {
    v3
};

export class VVE_API VEEngine {
public:
    explicit VEEngine(VEEngineVersion version);
    ~VEEngine();

    VEEngine(const VEEngine&) = delete;
    VEEngine(VEEngine&&) = delete;
    VEEngine& operator=(const VEEngine&) = delete;
    VEEngine& operator=(VEEngine&&) = delete;

    [[nodiscard]] virtual int getVersionMajor() const noexcept;
    [[nodiscard]] virtual std::string loadFile(const std::filesystem::path& file_path) const;

protected:
    VEEngine() noexcept;

private:
    std::unique_ptr<VEEngine> impl_;
};
