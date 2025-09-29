/*
    Entry point for the Black Hole Simulation application
*/

#include "../headers/app.hpp"
#include "../headers/renderer.hpp"
#include <iostream>

int main() {
    try {
        App app(1280, 720, "Black Hole Simulation");
        app.run();
    }
    catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        system("pause");
    }
    return 0;
}
