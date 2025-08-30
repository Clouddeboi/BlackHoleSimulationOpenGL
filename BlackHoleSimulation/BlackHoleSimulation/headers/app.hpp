#pragma once
#include <string>

//Forward-declare GLFWwindow to avoid heavy includes in the header
struct GLFWwindow;

class App {
public:
    App(int width, int height, const std::string& title);
    ~App();

    void run();//Main loop

private:
    void initGLFW();
    void initGLAD();
    void processInput();

    int m_width, m_height;
    std::string m_title;
    GLFWwindow* m_window;
};
