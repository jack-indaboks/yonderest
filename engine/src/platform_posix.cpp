#include "platform.hpp"

#include <cstdlib>
#include <fcntl.h>
#include <stdexcept>
#include <sys/file.h>
#include <unistd.h>

namespace yonder {

std::filesystem::path program_folder() {
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
}

std::filesystem::path data_folder() {
    std::filesystem::path base;
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) base = xdg;
    else if (const char* home = std::getenv("HOME"); home && *home) base = std::filesystem::path(home) / ".local" / "share";
    else throw std::runtime_error("Neither XDG_DATA_HOME nor HOME is set");
    auto folder = base / "yonderest";
    std::filesystem::create_directories(folder);
    return folder;
}

int lock_folder(const std::filesystem::path& folder) {
    int lock = open(folder.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (lock < 0) throw std::runtime_error("Cannot open " + folder.string());
    if (flock(lock, LOCK_EX | LOCK_NB) != 0) {
        close(lock);
        throw std::runtime_error(folder.string() + " is already open in another game");
    }
    return lock;
}

void unlock_folder(int lock) {
    close(lock);
}

}
