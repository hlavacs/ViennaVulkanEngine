import VEEngine;
import std;

int main() {
    vve::Engine engine(
        vve::EngineVersion::v3,
        vve::ApplicationName{"game"},
        vve::EnableValidation{true},
        vve::Windows{
            .value = {
                vve::WindowDesc{
                    .id = "main",
                    .title = "VVE Game",
                    .width = 1600,
                    .height = 900,
                    .resizable = true,
                    .visible = true
                },
                vve::WindowDesc{
                    .id = "tools",
                    .title = "VVE Tools",
                    .width = 960,
                    .height = 720,
                    .resizable = true,
                    .visible = true
                }
            }
        });

    if (!engine.init()) {
        return 1;
    }

    if (!engine.run()) {
        return 1;
    }

    const auto version_major = engine.getVersionMajor();
    if (!version_major) {
        return 1;
    }

    std::cout << "VVE " << *version_major << ".0\n";
    return *version_major == 3 ? 0 : 1;
}
