#include "packages/manager.h"
#include "logger.h"
#include "thread_pool.h"
#include <future>
#include <vector>
#include <atomic>
#include <filesystem>

namespace fs = std::filesystem;

namespace dev::packages {
    Manager::Manager(std::shared_ptr<Cache> cache) : cache(std::move(cache)) {}

    bool Manager::installDependencies(const std::string& directory) {
        auto versions = getInstalledVersions(directory);
        if (versions.empty()) {
            return true; // No dependencies to install
        }

        ThreadPool pool(maxConcurrentInstalls);
        std::vector<std::future<bool>> results;
        std::atomic<float> progress = 0.0f;
        const float progressStep = 1.0f / versions.size();

        for (const auto& [package, version] : versions) {
            results.emplace_back(pool.enqueue([this, &directory, &package, &version, &progress, progressStep] {
                bool result = this->installSingleDependency(directory, package, version);
                if (this->progressCallback) {
                    progress += progressStep;
                    this->progressCallback(package, progress.load());
                }
                return result;
            }));
        }

        bool success = true;
        for (auto& result : results) {
            try {
                if (!result.get()) {
                    success = false;
                }
            } catch (const std::exception& e) {
                Logger::error("Package installation failed: ", e.what());
                success = false;
            }
        }

        return success;
    }

    bool Manager::linkDependencies(const std::string& directory) {
        auto versions = getInstalledVersions(directory);
        if (versions.empty()) {
            return true;
        }

        bool success = true;
        for (const auto& [package, version] : versions) {
            const auto vendorPath = fs::path(directory) / getVendorDir() / package;
            if (!cache->linkFromCache(getLanguageName(), package, version, vendorPath.string())) {
                Logger::error("Failed to link package: ", package);
                success = false;
            }
        }

        return success;
    }

    void Manager::setProgressCallback(ProgressCallback callback) {
        progressCallback = std::move(callback);
    }

    void Manager::setTimeout(std::chrono::seconds timeout) {
        this->timeout = timeout;
    }

    void Manager::setMaxConcurrentInstalls(size_t max) {
        if (max == 0) {
            max = std::thread::hardware_concurrency();
        }
        maxConcurrentInstalls = max;
    }
}
