import std;
import VEEngine;

int main() {
    VEEngine engine;
    std::cout << "VVE " << engine.getVersionMajor() << ".0\n";
    return engine.getVersionMajor() == 3 ? 0 : 1;
}
