#pragma once

#include <iostream>
#include <mutex>
#include <string>

namespace dev::logger {

    class Logger {
    public:
        static void logMessage(const std::string &msg) {
            std::lock_guard<std::mutex> lock(getMutex());
            std::cout << msg << std::endl;
        }
        static void logError(const std::string &msg) {
            std::lock_guard<std::mutex> lock(getMutex());
            std::cerr << msg << std::endl;
        }
    private:
        static std::mutex & getMutex() {
            static std::mutex mtx;
            return mtx;
        }
    };

} // namespace dev::packages