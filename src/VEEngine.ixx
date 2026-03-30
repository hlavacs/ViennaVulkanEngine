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

export class VVE_API VEEngine {
public:
    virtual ~VEEngine();

    [[nodiscard]] virtual int getVersionMajor() const noexcept = 0;
};

export class VVE_API VEEngineDelegator final : public VEEngine {
public:
    VEEngineDelegator();
    ~VEEngineDelegator() override;

    VEEngineDelegator(const VEEngineDelegator&) = delete;
    VEEngineDelegator(VEEngineDelegator&&) = delete;
    VEEngineDelegator& operator=(const VEEngineDelegator&) = delete;
    VEEngineDelegator& operator=(VEEngineDelegator&&) = delete;

    [[nodiscard]] int getVersionMajor() const noexcept override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
