#include "packages/composer.h"
#include "cache.h"
#include "logger.h"
#include <filesystem>
#include <cstdlib>
#include <simdjson.h>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace dev::packages {
    Composer::Composer(std::shared_ptr<Cache> cache) : Manager(std::move(cache)) {}

    bool Composer::isProjectType(const std::string& directory) {
        return fs::exists(fs::path(directory) / "composer.json");
    }

    std::vector<std::string> Composer::getDependencyFiles() {
        return {"composer.json", "composer.lock"};
    }

    std::unordered_map<std::string, std::string> Composer::getInstalledVersions(
        const std::string& directory
    ) {
        const fs::path lockFile = fs::path(directory) / "composer.lock";

        if (!fs::exists(lockFile)) {
            return {};
        }

        // Check cache first
        if (isLockFileCacheValid(lockFile)) {
            return lockFileCache.at(lockFile.string()).versions;
        }

        updateLockFileCache(lockFile);
        return lockFileCache.at(lockFile.string()).versions;
    }

    bool Composer::isLockFileCacheValid(const fs::path& lockFile) const {
        const auto it = lockFileCache.find(lockFile.string());

        if (it == lockFileCache.end()) {
            return false;
        }

        const auto currentTimestamp = fs::last_write_time(lockFile);

        return it->second.fileTimestamp == currentTimestamp;
    }

    void Composer::updateLockFileCache(const fs::path& lockFile) {
        try {
            simdjson::ondemand::parser parser;
            const simdjson::padded_string json = simdjson::padded_string::load(lockFile.string());
            simdjson::ondemand::document doc = parser.iterate(json);

            LockFileCache cacheEntry;
            cacheEntry.lastRead = std::chrono::system_clock::now();
            cacheEntry.fileTimestamp = fs::last_write_time(lockFile);

            auto processPackages = [&](simdjson::ondemand::array packages) {
                for (auto package : packages) {
                    try {
                        std::string_view name = package["name"].get_string();
                        std::string_view version = package["version"].get_string();
                        cacheEntry.versions[std::string(name)] = std::string(version);
                    } catch (const simdjson::simdjson_error& e) {
                        Logger::error("Error parsing package: ", e.what());
                    }
                }
            };

            // Process both regular and dev packages
            if (auto packages = doc["packages"]; packages.error() == simdjson::SUCCESS) {
                processPackages(packages.get_array());
            }
            if (auto packages_dev = doc["packages-dev"]; packages_dev.error() == simdjson::SUCCESS) {
                processPackages(packages_dev.get_array());
            }

            lockFileCache[lockFile.string()] = std::move(cacheEntry);
        } catch (const simdjson::simdjson_error& e) {
            throw PackageManagerError("Failed to parse composer.lock: " + std::string(e.what()));
        } catch (const std::exception& e) {
            throw PackageManagerError("Error reading composer.lock: " + std::string(e.what()));
        }
    }

    bool Composer::installSingleDependency(
        const std::string& directory,
        const std::string& package,
        const std::string& version
    ) {
        if (!cache) {
            throw PackageManagerError("Cache not initialized");
        }

        if (cache->isCached("composer", package, version)) {
            Logger::debug("Package ", package, ":", version, " is already cached.");
            return true;
        }

        Logger::info("Installing package ", package, ":", version);

        // Placeholder for actual package download and installation logic.
        // In a real implementation, this would involve:
        // 1. Downloading the package from a repository (e.g., Packagist).
        // 2. Verifying its integrity (e.g., checksum).
        // 3. Extracting it to the cache directory.
        // For now, we'll simulate a successful installation.

        // Simulate creating a dummy file in the cache to mark it as "installed"
        auto packageCachePath = fs::path(cache->getCacheDir()) / getManagerName() / package / version;
        fs::create_directories(packageCachePath);
        std::ofstream dummyFile(packageCachePath / "installed.marker");
        dummyFile << "installed";
        dummyFile.close();

        return true;
    }

    std::string Composer::getManagerName() const {
        return "composer";
    }

    std::string Composer::getInstallDirectory() const {
        return "vendor";
    }
}
