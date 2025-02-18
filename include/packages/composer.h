#pragma once

#include <string>
#include <vector>
#include "packages/manager.h"

namespace dev::packages {
    class Composer final : public Manager {
    public:
        bool isProjectType(const std::string &directory) override;

        std::vector<std::string> getDependencyFiles() override;

        std::unordered_map<std::string, std::string> getInstalledVersions(
            const std::string &directory
        ) override;

        bool installDependencies(
            const std::string &directory
        ) override;

        bool linkDependencies(const std::string &directory) override;
    };
} // namespace dev::packages
