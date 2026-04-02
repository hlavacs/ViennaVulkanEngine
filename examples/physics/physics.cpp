import VEEngine;
import VEEngine.V3;

int main() {
    auto engine = vve::makeEngine(
        vve::ApplicationName{"physics"},
        vve::Windows{
            .value = {
                vve::WindowDesc{
                    .id = "physics.main",
                    .title = "VVE Physics",
                    .width = 1280,
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

    return *version_major == 3 ? 0 : 1;
}
