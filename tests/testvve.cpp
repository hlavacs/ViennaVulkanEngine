
#include <iostream>
#include <utility>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "vulkan/vulkan.h"

#include "VEEngine.h"


class MyGUI : public vve::VeSystem {
public:
    MyGUI() = default;
    ~MyGUI() = default;
    void onDrawGUI(vve::VeMessage message) {
        std::cout << "Draw GUI\n";
    }
private:
};

MyGUI my_gui;

int main() {

    vve::VeEngine engine;

    engine.Run();

    std::cout << "Hello world\n";
    return 0;
}

