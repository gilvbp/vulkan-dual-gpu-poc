#include "app.h"
#include <iostream>

int main() {
    try {
        App app;
        app.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
}
