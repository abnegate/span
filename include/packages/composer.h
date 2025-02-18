#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include "packages/manager.h"

namespace fs = std::filesystem;

namespace dev::packages {
    class Composer final : public Manager {
    public:
        bool isProjectType(const std::string &directory) override;

        std::vector<std::string> getDependencyFiles() override;

        std::unordered_map<std::string, std::string> getInstalledVersions(const std::string &directory) override;

        bool installDependencies(const std::string &directory) override;

        bool linkDependencies(const std::string &directory) override;

    private:
        static bool processPackageInstallation(
            const fs::path &packageCachePath,
            const std::string &version
        );

        static bool installSingleDependency(
            const std::string &directory,
            const std::string &package,
            const std::string &version
        );
    };
} // namespace dev::packages
