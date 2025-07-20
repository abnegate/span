#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <memory>
#include <chrono>
#include <functional>
#include <thread>

#include "packages/manager.h"
#include "cache.h"

namespace fs = std::filesystem;

namespace dev::packages {
    class Composer final : public Manager {
    public:
        explicit Composer(std::shared_ptr<Cache> cache);

        bool isProjectType(const std::string& directory) override;

        std::vector<std::string> getDependencyFiles() override;

        std::unordered_map<std::string, std::string> getInstalledVersions(
            const std::string& directory
        ) override;

        bool installDependencies(const std::string& directory) override;

        bool linkDependencies(const std::string& directory) override;

        // Progress callback type
        using ProgressCallback = std::function<void(const std::string& package, float progress)>;

        void setProgressCallback(ProgressCallback callback);

        // Timeout settings
        void setTimeout(std::chrono::seconds timeout);

        // Concurrency control
        void setMaxConcurrentInstalls(size_t max);

    private:
        std::shared_ptr<Cache> cache;
        ProgressCallback progressCallback;
        size_t maxConcurrentInstalls{std::thread::hardware_concurrency()};
        std::chrono::seconds timeout{300}; // 5 minutes default

        struct LockFileCache {
            std::unordered_map<std::string, std::string> versions;
            std::chrono::system_clock::time_point lastRead;
            fs::file_time_type fileTimestamp;
        };

        std::unordered_map<std::string, LockFileCache> lockFileCache;
        bool isLockFileCacheValid(const fs::path& lockFile) const;
        void updateLockFileCache(const fs::path& lockFile);

        bool installSingleDependency(
            const std::string& directory,
            const std::string& package,
            const std::string& version
        ) const;
    };
} // namespace dev::packages
