/*
    Entry point for the Black Hole Simulation application
*/

#include "../headers/app.hpp"
#include "../headers/renderer.hpp"

int main() {
    App app(1280, 720, "Black Hole Simulation");
    app.run();
    return 0;
}
