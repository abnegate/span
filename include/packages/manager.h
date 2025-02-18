#pragma once

#include <vector>
#include <string>
#include <unordered_map>

namespace dev::packages {
    class Manager {
    public:
        virtual ~Manager() = default;

        virtual bool isProjectType(const std::string& directory) = 0;

        virtual std::vector<std::string> getDependencyFiles() = 0;

        virtual std::unordered_map<std::string, std::string> getInstalledVersions(
            const std::string& directory
        ) = 0;

        virtual bool installDependencies(
            const std::string& directory
        ) = 0;

        virtual bool linkDependencies(const std::string& directory) = 0;
    };
}