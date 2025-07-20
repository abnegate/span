#pragma once

#include <iostream>
#include <mutex>
#include <string>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace dev {
    class Logger {
    public:
        enum class Level {
            DEBUG,
            INFO,
            WARNING,
            ERROR
        };

        static void setLogLevel(const Level level) {
            getLogger().currentLevel = level;
        }

        template<typename... Args>
        static void debug(Args&&... args) {
            log(Level::DEBUG, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void info(Args&&... args) {
            log(Level::INFO, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void warning(Args&&... args) {
            log(Level::WARNING, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void error(Args&&... args) {
            log(Level::ERROR, std::forward<Args>(args)...);
        }

    private:
        static Logger& getLogger() {
            static Logger instance;
            return instance;
        }

        template<typename... Args>
        static void log(const Level level, Args&&... args) {
            if (level < getLogger().currentLevel) {
                return;
            }

            std::stringstream message;
            (message << ... << std::forward<Args>(args));

            std::lock_guard<std::mutex> lock(getLogger().mutex);
            const auto now = std::chrono::system_clock::now();
            const auto time = std::chrono::system_clock::to_time_t(now);

            std::cout << "["
                     << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
                     << "]["
                     << getLevelString(level)
                     << "] "
                     << message.str()
                     << std::endl;
        }

        static const char* getLevelString(const Level level) {
            switch (level) {
                case Level::DEBUG:   return "DEBUG";
                case Level::INFO:    return "INFO";
                case Level::WARNING: return "WARN";
                case Level::ERROR:   return "ERROR";
                default:            return "UNKNOWN";
            }
        }

        Logger() : currentLevel(Level::INFO) {}
        Level currentLevel;
        std::mutex mutex;
    };
}
