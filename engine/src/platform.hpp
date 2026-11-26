#pragma once

// What the engine needs from the operating system. One implementation per platform.

#include <filesystem>

namespace yonder {

// The folder the program was started from.
std::filesystem::path program_folder();
// The user's folder for this game's data, made if it is missing.
std::filesystem::path data_folder();
// Holds a folder for this process alone until unlocked; the system releases it however the game ends.
int lock_folder(const std::filesystem::path& folder);
void unlock_folder(int lock);

}
