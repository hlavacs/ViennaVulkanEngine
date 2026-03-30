module VEEngine.V3;

namespace vve::v3 {

namespace {

class Engine final : public VEEngine {
public:
    Engine() = default;

    [[nodiscard]] int getVersionMajor() const noexcept override {
        return 3;
    }

    [[nodiscard]] std::string loadFile(const std::filesystem::path& file_path) const override {
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            return {};
        }

        return {
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };
    }
};

} // namespace

std::unique_ptr<VEEngine> makeEngine() {
    return std::make_unique<Engine>();
}

} // namespace vve::v3
