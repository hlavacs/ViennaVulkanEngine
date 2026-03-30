import VEEngine;

int main() {
    VEEngineDelegator engine;
    return engine.getVersionMajor() == 3 ? 0 : 1;
}
