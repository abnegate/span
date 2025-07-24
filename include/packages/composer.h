#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <memory>
#include <chrono>
#include "packages/manager.h"
#include "cache.h"

namespace fs = std::filesystem;

namespace dev::packages {
    class Composer final : public Manager {
    public:
        static constexpr const char* DEPS_FILE_NAME = "composer.json";
        static constexpr const char* LOCK_FILE_NAME = "composer.lock";

        explicit Composer(std::shared_ptr<Cache> cache);

        bool isProjectType(const std::string& directory) override;

        std::unordered_map<std::string, std::string> getInstalledVersions(
            const std::string& directory
        ) override;

    private:
        bool installDependency(
            const std::string& directory,
            const std::string& package,
            const std::string& version
        ) override;

        [[nodiscard]] std::string getManagerName() const override;
        [[nodiscard]] std::string getInstallDirectory() const override;
        [[nodiscard]] std::string getDependencyFileName() const override;

        struct LockFileCache {
            std::unordered_map<std::string, std::string> versions;
            std::chrono::system_clock::time_point lastRead;
            fs::file_time_type fileTimestamp;
        };

        std::unordered_map<std::string, LockFileCache> lockFileCache;
        bool isLockFileCacheValid(const fs::path& lockFile) const;
        void updateLockFileCache(const fs::path& lockFile);
    };
} // namespace dev::packages
