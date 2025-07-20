#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <functional>
#include <chrono>
#include <memory>
#include <thread>
#include "cache.h"

namespace dev::packages {
    // Custom exception for package management errors
    class PackageManagerError final : public std::runtime_error {
    public:
        explicit PackageManagerError(const std::string& message)
            : std::runtime_error(message) {}
    };

    class Manager {
    public:
        using ProgressCallback = std::function<void(const std::string& package, float progress)>;

        explicit Manager(std::shared_ptr<Cache> cache);
        virtual ~Manager() = default;

        /**
         * Check if the given directory contains a project of this type
         * @param directory The directory to check
         * @return True if this is the correct project type, false otherwise
         * @throws PackageManagerError if the directory cannot be accessed
         */
        virtual bool isProjectType(const std::string& directory) = 0;

        /**
         * Get the list of files that define dependencies
         * @return Vector of dependency file names
         */
        virtual std::vector<std::string> getDependencyFiles() = 0;

        /**
         * Get map of installed package versions
         * @param directory The project directory
         * @return Map of package names to versions
         * @throws PackageManagerError if version information cannot be retrieved
         */
        virtual std::unordered_map<std::string, std::string> getInstalledVersions(
            const std::string& directory
        ) = 0;

        /**
         * Install all dependencies for the project
         * @param directory The project directory
         * @return true if all dependencies were installed successfully
         * @throws PackageManagerError if installation fails
         */
        virtual bool installDependencies(
            const std::string& directory
        );

        /**
         * Link installed dependencies into the project
         * @param directory The project directory
         * @return true if all dependencies were linked successfully
         * @throws PackageManagerError if linking fails
         */
        bool linkDependencies(const std::string& directory);

        void setProgressCallback(ProgressCallback callback);
        void setTimeout(std::chrono::seconds timeout);
        void setMaxConcurrentInstalls(size_t max);

    protected:
        std::shared_ptr<Cache> cache;
        ProgressCallback progressCallback;
        std::chrono::seconds timeout{300};
        size_t maxConcurrentInstalls{std::thread::hardware_concurrency()};

        virtual bool installSingleDependency(
            const std::string& directory,
            const std::string& package,
            const std::string& version
        ) = 0;

        virtual std::string getManagerName() const = 0;
        virtual std::string getInstallDirectory() const = 0;
    };
}