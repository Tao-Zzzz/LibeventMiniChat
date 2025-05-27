#pragma once
#include <string>
#include <fmt/format.h>

class Logger {
public:
    template<typename... Args>
    static void Info(const std::string& format, Args&&... args) {
        fmt::print("[INFO] {}\n", fmt::format(format, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Error(const std::string& format, Args&&... args) {
        fmt::print("[ERROR] {}\n", fmt::format(format, std::forward<Args>(args)...));
    }
};