module VEEngine.V3;

namespace vve::v3 {

namespace {

class EngineImpl final : public vve::detail::EngineImpl {
public:
    explicit EngineImpl(const vve::EngineConfig& config)
        : application_name_("ViennaVulkanEngine"),
          validation_enabled_(false),
          initialized_(false),
          running_(false) {
        if (const auto application_name = config.tryGet<vve::ApplicationName>()) {
            application_name_ = application_name->value;
        }

        if (const auto enable_validation = config.tryGet<vve::EnableValidation>()) {
            validation_enabled_ = enable_validation->value;
        }
    }

    [[nodiscard]] std::expected<void, vve::Result> init() override {
        initialized_ = true;
        running_ = false;
        return {};
    }

    [[nodiscard]] std::expected<void, vve::Result> run() override {
        if (!initialized_) {
            if (auto init_result = init(); !init_result) {
                return init_result;
            }
        }

        running_ = true;
        return {};
    }

    [[nodiscard]] std::expected<void, vve::Result> step() override {
        if (!initialized_) {
            return std::unexpected(vve::Result::internal_error);
        }

        return {};
    }

    [[nodiscard]] std::expected<int, vve::Result> getVersionMajor() const noexcept override {
        return 3;
    }

    [[nodiscard]] std::expected<std::string, vve::Result> loadFile(
        const std::filesystem::path& file_path) const override {
        if (file_path.empty()) {
            return std::unexpected(vve::Result::invalid_argument);
        }

        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            return std::unexpected(vve::Result::file_not_found);
        }

        std::string file_contents = {
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };

        if (!file.eof() && file.fail()) {
            return std::unexpected(vve::Result::io_error);
        }

        return file_contents;
    }

private:
    std::string application_name_;
    bool validation_enabled_{false};
    bool initialized_{false};
    bool running_{false};
};

} // namespace

std::unique_ptr<vve::detail::EngineImpl> makeEngine(const vve::EngineConfig& config) {
    return std::make_unique<EngineImpl>(config);
}

} // namespace vve::v3
