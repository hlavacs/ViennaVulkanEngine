import VEEngine;

int main() {
    VEEngine engine(
        VeEngineVersion::v3,
        VeApplicationName{"physics"});
    const auto version_major = engine.getVersionMajor();
    if (!version_major) {
        return 1;
    }

    return *version_major == 3 ? 0 : 1;
}
