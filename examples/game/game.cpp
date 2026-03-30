import VEEngine;
import std;

int main() {
    VEEngine engine(
        VeEngineVersion::v3,
        VeApplicationName{"game"},
        VeEnableValidation{true});
    const auto version_major = engine.getVersionMajor();
    if (!version_major) {
        return 1;
    }

    std::cout << "VVE " << *version_major << ".0\n";
    return *version_major == 3 ? 0 : 1;
}
