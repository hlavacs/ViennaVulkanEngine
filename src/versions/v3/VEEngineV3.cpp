module VEEngine.V3;

namespace vve::v3 {

namespace {

class Engine final : public VEEngine {
public:
    Engine()
        : VEEngine(ConstructionMode::implementation) {
    }

    [[nodiscard]] int getVersionMajor() const noexcept override {
        return 3;
    }
};

} // namespace

std::unique_ptr<VEEngine> makeEngine() {
    return std::make_unique<Engine>();
}

} // namespace vve::v3
