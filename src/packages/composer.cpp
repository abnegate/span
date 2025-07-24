#include "packages/composer.h"
#include "cache.h"
#include "logger.h"
#include <filesystem>
#include <fstream>
#include <simdjson.h>

namespace fs = std::filesystem;

namespace dev::packages {
    Composer::Composer(std::shared_ptr<Cache> cache) : Manager(std::move(cache)) {}

    bool Composer::isProjectType(const std::string& directory) {
        return fs::exists(fs::path(directory) / LOCK_FILE_NAME) ||
               fs::exists(fs::path(directory) / DEPS_FILE_NAME);
    }

    std::unordered_map<std::string, std::string> Composer::getInstalledVersions(
        const std::string& directory
    ) {
        const fs::path lockFile = fs::path(directory) / LOCK_FILE_NAME;
        if (fs::exists(lockFile)) {
            // Check cache first
            if (isLockFileCacheValid(lockFile)) {
                return lockFileCache.at(lockFile.string()).versions;
            }

            updateLockFileCache(lockFile);
            return lockFileCache.at(lockFile.string()).versions;
        }

        // If lock file doesn't exist, parse composer.json
        const fs::path composerJsonFile = fs::path(directory) / DEPS_FILE_NAME;
        if (!fs::exists(composerJsonFile)) {
            return {};
        }

        try {
            simdjson::ondemand::parser parser;
            const simdjson::padded_string json = simdjson::padded_string::load(composerJsonFile.string());
            simdjson::ondemand::document doc = parser.iterate(json);

            std::unordered_map<std::string, std::string> versions;

            auto processPackages = [&versions](simdjson::ondemand::object packages) {
                for (auto field : packages) {
                    try {
                        auto key = field.key();
                        std::string_view keyView{key.raw()};
                        // Skip PHP version requirement for performance
                        if (keyView != "php") {
                            versions.emplace(keyView, "");
                        }
                    } catch (const simdjson::simdjson_error& e) {
                        Logger::error("Error parsing package requirement: ", e.what());
                    }
                }
            };

            // Process require section
            if (auto require = doc["require"]; require.error() == simdjson::SUCCESS) {
                processPackages(require.get_object());
            }
            // Process require-dev section
            if (auto require_dev = doc["require-dev"]; require_dev.error() == simdjson::SUCCESS) {
                processPackages(require_dev.get_object());
            }

            return versions;
        } catch (const simdjson::simdjson_error& e) {
            throw PackageManagerError("Failed to parse composer.json: " + std::string(e.what()));
        } catch (const std::exception& e) {
            throw PackageManagerError("Error reading composer.json: " + std::string(e.what()));
        }
    }

    bool Composer::installDependency(
        const std::string& directory,
        const std::string& package,
        const std::string& version
    ) {
        // For composer, we don't need to check the cache. `composer require` will handle everything.
        // It will either download the package or use its own cache. It will also update
        // composer.lock, which is what we want.

        std::string command = "composer require --working-dir=" + directory + " " + package;
        if (!version.empty()) {
            command += ":" + version;
        }

        Logger::info("Running command: ", command);

        // TODO: A better way to execute commands and handle output.
        int result = std::system(command.c_str());

        if (result != 0) {
            Logger::error("Failed to install package: ", package);
            return false;
        }

        return true;
    }

    std::string Composer::getManagerName() const {
        return "composer";
    }

    std::string Composer::getInstallDirectory() const {
        return "vendor";
    }

    std::string Composer::getDependencyFileName() const {
        return DEPS_FILE_NAME;
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

            auto processPackages = [&cacheEntry](simdjson::ondemand::array packages) {
                for (auto package : packages) {
                    try {
                        std::string_view name = package["name"].get_string();
                        std::string_view version = package["version"].get_string();
                        cacheEntry.versions.emplace(name, version);
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
            throw PackageManagerError("Failed to parse lock file: " + std::string(e.what()));
        } catch (const std::exception& e) {
            throw PackageManagerError("Error reading lock file: " + std::string(e.what()));
        }
    }
}
