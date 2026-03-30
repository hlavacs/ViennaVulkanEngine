import VEEngine;

int main() {
    ve::Engine engine(
        ve::EngineVersion::v3,
        ve::ApplicationName{"physics"});
    const auto version_major = engine.getVersionMajor();
    if (!version_major) {
        return 1;
    }

    return *version_major == 3 ? 0 : 1;
}
