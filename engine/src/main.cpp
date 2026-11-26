#include "yonderest/host.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try { return yonder::run(argc, argv, yonder::attach_modules); }
    catch (const std::exception& error) {
        std::cerr << "Yonderest: " << error.what() << '\n';
        return 1;
    }
}
