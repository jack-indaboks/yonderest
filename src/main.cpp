#include "app.hpp"
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try { return yonder::run(argc, argv); }
    catch (const std::exception& error) {
        std::cerr << "Yonderest: " << error.what() << '\n';
        return 1;
    }
}
