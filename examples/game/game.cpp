import VEEngine;
import std;

int main() {
    vve::Engine engine(
        vve::EngineVersion::v3,
        vve::ApplicationName{"game"},
        vve::EnableValidation{true});
    const auto version_major = engine.getVersionMajor();
    if (!version_major) {
        return 1;
    }

    std::cout << "VVE " << *version_major << ".0\n";
    return *version_major == 3 ? 0 : 1;
}
