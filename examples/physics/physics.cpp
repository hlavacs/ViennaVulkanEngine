import VEEngine;

int main() {
    vve::Engine engine(
        vve::EngineVersion::v3,
        vve::ApplicationName{"physics"});
    const auto version_major = engine.getVersionMajor();
    if (!version_major) {
        return 1;
    }

    return *version_major == 3 ? 0 : 1;
}
