import VEEngine;

int main() {
    VEEngine engine(VEEngineVersion::v3);
    return engine.getVersionMajor() == 3 ? 0 : 1;
}
