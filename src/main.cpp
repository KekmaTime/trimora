#include "gui/application.hpp"
#include <iostream>
#include <exception>

int main(int /*argc*/, char** /*argv*/) {
    try {
        trimora::Application app;
        
        if (!app.initialize()) {
            std::cerr << "Failed to initialize application" << std::endl;
            return 1;
        }
        
        app.run();
        app.shutdown();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

