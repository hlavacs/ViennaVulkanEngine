import VEEngine;
import std;

int main() {
    ve::Engine engine(
        ve::EngineVersion::v3,
        ve::ApplicationName{"game"},
        ve::EnableValidation{true});
    const auto version_major = engine.getVersionMajor();
    if (!version_major) {
        return 1;
    }

    std::cout << "VVE " << *version_major << ".0\n";
    return *version_major == 3 ? 0 : 1;
}
