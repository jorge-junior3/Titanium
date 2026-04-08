#include "core/Engine.h"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        Engine engine;
        engine.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}