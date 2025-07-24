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
        // Step 1: Check for lock file, fallback to dependency file, throw if neither exists
        auto versions = getInstalledVersions(directory);
        if (versions.empty()) {
            // Check if dependency file exists to give better error message
            const fs::path depsFile = fs::path(directory) / getDependencyFileName();
            if (!fs::exists(depsFile)) {
                throw PackageManagerError("No dependency file found in " + directory);
            }
            return true; // No dependencies to install
        }

        span::threads::ThreadPool pool(maxConcurrentInstalls);
        std::vector<std::future<bool>> results;
        std::atomic<float> progress = 0.0f;
        const float progressStep = 1.0f / static_cast<float>(versions.size());

        for (const auto& [package, version] : versions) {
            results.emplace_back(pool.enqueue([this, &directory, &package, &version, &progress, progressStep] {
                const bool result = this->installSingleDependency(directory, package, version);
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

    bool Manager::installSingleDependency(
        const std::string& directory,
        const std::string& package,
        const std::string& version
    ) {
        const auto vendorPath = fs::path(directory) / getInstallDirectory() / package;

        // Step 2: Check if package is already installed in vendor directory
        if (fs::exists(vendorPath)) {
            Logger::info("Package ", package, " already installed in vendor directory");

            // Step 3: Make sure it's linked to global cache
            if (!cache->linkToCache(getManagerName(), package, version, vendorPath.string())) {
                Logger::error("Failed to link existing package to cache: ", package);
            }
            return true;
        }

        // Step 4: Check if version is in global cache
        if (cache->isCached(getManagerName(), package, version)) {
            Logger::info("Package ", package, " found in cache, linking to project");

            // Step 5: Link from cache to project
            if (cache->linkFromCache(getManagerName(), package, version, vendorPath.string())) {
                return true;
            } else {
                Logger::error("Failed to link package from cache: ", package);
                // Fall through to installation
            }
        }

        // Step 6: Package not in cache, install it then link to cache
        Logger::info("Installing package ", package, " version ", version);

        if (!installDependency(directory, package, version)) {
            Logger::error("Failed to install package: ", package);
            return false;
        }

        // After installation, link the installed package to cache
        if (fs::exists(vendorPath)) {
            if (!cache->linkToCache(getManagerName(), package, version, vendorPath.string())) {
                Logger::error("Package installed but failed to link to cache: ", package);
            }
        }

        return true;
    }

    bool Manager::linkDependencies(const std::string& directory) {
        auto versions = getInstalledVersions(directory);
        if (versions.empty()) {
            return true;
        }

        bool success = true;
        for (const auto& [package, version] : versions) {
            const auto vendorPath = fs::path(directory) / getInstallDirectory() / package;
            if (!cache->linkFromCache(getManagerName(), package, version, vendorPath.string())) {
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
