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

export class VVE_API VEEngine {
public:
    VEEngine();
    ~VEEngine();

    [[nodiscard]] int getVersionMajor() const noexcept;
};
