
#include <iostream>
#include <utility>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "vulkan/vulkan.h"

#include "VEEngine.h"


class MyGUI : public vve::System {
public:
    MyGUI() = default;
    ~MyGUI() = default;
    void onDrawGUI(vve::Message message) {
        std::cout << "Draw GUI\n";
    }
private:
};

MyGUI my_gui;

int main() {

    vve::Engine engine;
    engine.RegisterSystem(std::make_shared<MyGUI>(), {vve::MessageType::DRAW_GUI});

    engine.Run();

    return 0;
}

