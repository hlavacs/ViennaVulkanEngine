module VEEngine.V3;

namespace vve::v3 {

namespace {

class Engine final : public VEEngine {
public:
    explicit Engine(const VEEngineConfig& config)
        : application_name_("ViennaVulkanEngine"),
          validation_enabled_(false) {
        if (const auto application_name = config.tryGet<VEApplicationName>()) {
            application_name_ = application_name->value;
        }

        if (const auto enable_validation = config.tryGet<VEEnableValidation>()) {
            validation_enabled_ = enable_validation->value;
        }
    }

    [[nodiscard]] std::expected<int, VeResult> getVersionMajor() const noexcept override {
        return 3;
    }

    [[nodiscard]] std::expected<std::string, VeResult> loadFile(
        const std::filesystem::path& file_path) const override {
        if (file_path.empty()) {
            return std::unexpected(VeResult::invalid_argument);
        }

        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            return std::unexpected(VeResult::file_not_found);
        }

        std::string file_contents = {
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };

        if (!file.eof() && file.fail()) {
            return std::unexpected(VeResult::io_error);
        }

        return file_contents;
    }

private:
    std::string application_name_;
    bool validation_enabled_;
};

} // namespace

std::unique_ptr<VEEngine> makeEngine(const VEEngineConfig& config) {
    return std::make_unique<Engine>(config);
}

} // namespace vve::v3
