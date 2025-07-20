#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>

namespace dev::packages {
    // Custom exception for package management errors
    class PackageManagerError final : public std::runtime_error {
    public:
        explicit PackageManagerError(const std::string& message)
            : std::runtime_error(message) {}
    };

    class Manager {
    public:
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
        ) = 0;

        /**
         * Link installed dependencies into the project
         * @param directory The project directory
         * @return true if all dependencies were linked successfully
         * @throws PackageManagerError if linking fails
         */
        virtual bool linkDependencies(const std::string& directory) = 0;
    };
}