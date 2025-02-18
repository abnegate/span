#include "packages/composer.h"
#include "cache/cache.h"
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <simdjson.h>
#include <sstream>
#include <future>
#include <vector>
#include <mutex>

namespace fs = std::filesystem;

// Mutex to guard logging so messages don't get jumbled.
static std::mutex logMutex;
// Mutex to serialize composer install/update calls.
static std::mutex composerMutex;

namespace dev::packages {

    // Simple logging helpers.
    void logMessage(const std::string& msg) {
        std::lock_guard lock(logMutex);
        std::cout << msg << std::endl;
    }
    void logError(const std::string& msg) {
        std::lock_guard lock(logMutex);
        std::cerr << msg << std::endl;
    }

    // Helper: Move all contents of packageCachePath into a subdirectory named by version.
    bool Composer::processPackageInstallation(const fs::path& packageCachePath, const std::string& version) {
        fs::path versionedPath = packageCachePath / version;
        try {
            if (!fs::exists(packageCachePath)) {
                logError("Cache package path " + packageCachePath.string() + " does not exist.");
                return false;
            }
            // If already processed, we're good.
            if (fs::exists(versionedPath))
                return true;
            if (fs::is_empty(packageCachePath)) {
                logError("Cache package path " + packageCachePath.string() + " is empty.");
                return false;
            }
            fs::create_directory(versionedPath);
            for (auto& entry : fs::directory_iterator(packageCachePath)) {
                // Skip version folder if it somehow appears.
                if (fs::equivalent(entry.path(), versionedPath))
                    continue;
                fs::rename(entry.path(), versionedPath / entry.path().filename());
            }
            logMessage("Post-processed package at " + packageCachePath.string() + " into version folder " + version);
        } catch (const fs::filesystem_error& e) {
            logError("Post-processing failed for package (" + packageCachePath.string() + "): " + e.what());
            return false;
        }
        return true;
    }

    bool Composer::isProjectType(const std::string &directory) {
        return fs::exists(fs::path(directory) / "composer.json");
    }

    std::vector<std::string> Composer::getDependencyFiles() {
        return {"composer.json", "composer.lock"};
    }

    std::unordered_map<std::string, std::string> Composer::getInstalledVersions(const std::string &directory) {
        std::unordered_map<std::string, std::string> packageVersions;
        fs::path lockFile = fs::path(directory) / "composer.lock";
        if (!fs::exists(lockFile)) {
            return packageVersions;
        }
        try {
            simdjson::ondemand::parser parser;
            simdjson::padded_string json = simdjson::padded_string::load(lockFile.string());
            simdjson::ondemand::document doc = parser.iterate(json);
            for (auto package : doc["packages"]) {
                try {
                    auto packageName = std::string(package["name"].get_string().value_unsafe());
                    auto packageVersion = std::string(package["version"].get_string().value_unsafe());
                    packageVersions[packageName] = packageVersion;
                } catch (const std::exception& e) {
                    logError("Error parsing package: " + std::string(e.what()));
                }
            }
        } catch (const std::exception &e) {
            logError("Failed to parse composer.lock: " + std::string(e.what()));
        }
        return packageVersions;
    }

    // FULL INSTALL: Run composer install with --vendor-dir to force installation into our cache.
    bool Composer::installDependencies(const std::string &directory) {
        logMessage("Running full dependency installation...");

        // Our cache is at: <cacheDir>/php
        fs::path cachePath = fs::path(Cache::getCacheDir()) / "php";
        try {
            fs::create_directories(cachePath);
        } catch (const fs::filesystem_error& e) {
            logError("Failed to create cache directory: " + std::string(e.what()));
            return false;
        }

        // Remove the project's vendor folder to force composer to use our cache.
        fs::path projectVendor = fs::path(directory) / "vendor";
        if (fs::exists(projectVendor)) {
            try {
                fs::remove_all(projectVendor);
            } catch (const fs::filesystem_error& e) {
                logError("Failed to remove project vendor directory: " + std::string(e.what()));
                return false;
            }
        }

        // Build the composer install command using --vendor-dir.
        std::ostringstream cmd;
        cmd << "cd \"" << fs::absolute(directory).string() << "\" && ";
        cmd << "COMPOSER_VENDOR_DIR=\"" << fs::absolute(cachePath).string() << "\" ";
        cmd << "composer install --ignore-platform-reqs --prefer-dist --no-interaction";

        logMessage("Running command: " + cmd.str());
        {
            std::lock_guard lock(composerMutex);
            int result = std::system(cmd.str().c_str());
            if (result != 0) {
                logError("Dependency installation failed with code " + std::to_string(result));
                return false;
            }
        }
        logMessage("Dependencies installed successfully in global cache.");

        // Process each package from composer.lock into a versioned folder.
        auto packageVersions = getInstalledVersions(directory);
        std::vector<std::future<bool>> futures;
        for (const auto& [package, version] : packageVersions) {
            futures.push_back(std::async(std::launch::async, [cachePath, package, version, this]() -> bool {
                fs::path pkgPath = cachePath / package;
                return processPackageInstallation(pkgPath, version);
            }));
        }
        bool postSuccess = true;
        for (auto& fut : futures) {
            if (!fut.get())
                postSuccess = false;
        }
        if (!postSuccess) {
            logError("One or more packages failed to be organized into version folders.");
            return false;
        }
        return true;
    }

    // INSTALL A SINGLE PACKAGE: Run composer update for the missing package, then process and link it.
    bool Composer::installSingleDependency(const std::string &directory, const std::string &package, const std::string &version) {
        logMessage("Installing missing package " + package + "@" + version + " individually...");
        fs::path cachePath = fs::path(Cache::getCacheDir()) / "php";
        std::ostringstream cmd;
        cmd << "cd \"" << fs::absolute(directory).string() << "\" && ";
        cmd << "COMPOSER_VENDOR_DIR=\"" << fs::absolute(cachePath).string() << "\" ";
        cmd << "composer update " << package << " --ignore-platform-reqs --prefer-dist --no-interaction";

        {
            std::lock_guard lock(composerMutex);
            int result = std::system(cmd.str().c_str());
            if (result != 0) {
                logError("Failed to install package " + package + " with code " + std::to_string(result));
                return false;
            }
        }
        fs::path vendorPackagePath = fs::path(directory) / "vendor" / package;
        fs::path cachePackagePath = cachePath / package;
        if (!processPackageInstallation(cachePackagePath, version)) {
            logError("Failed to post-process package " + package);
            return false;
        }
        try {
            fs::remove_all(vendorPackagePath);
            fs::create_symlink(cachePackagePath / version, vendorPackagePath);
        } catch (const fs::filesystem_error &e) {
            logError("Failed to create symlink for " + package + ": " + e.what());
            return false;
        }
        logMessage("Successfully installed and cached " + package + "@" + version);
        return true;
    }

    // LINK DEPENDENCIES: Check which packages exist in the cache and install missing ones as needed.
    bool Composer::linkDependencies(const std::string &directory) {
        fs::path lockPath = fs::path(directory) / "composer.lock";
        if (!fs::exists(lockPath)) {
            logMessage("No lockfile detected, running full install...");
            if (!installDependencies(directory))
                return false;
        }
        auto packageVersions = getInstalledVersions(directory);
        logMessage("Checking global cache for dependencies...");
        fs::path cachePath = fs::path(Cache::getCacheDir()) / "php";

        // Count missing packages in the cache.
        int total = packageVersions.size();
        int missingCount = 0;
        for (const auto& [package, version] : packageVersions) {
            fs::path versionedPath = cachePath / package / version;
            if (!fs::exists(versionedPath))
                missingCount++;
        }

        if (missingCount == total) {
            logMessage("No packages found in cache; running full install...");
            if (!installDependencies(directory))
                return false;
        } else if (missingCount > 0) {
            logMessage("Some packages are missing from cache; installing missing packages individually...");
            for (const auto& [package, version] : packageVersions) {
                fs::path versionedPath = cachePath / package / version;
                if (!fs::exists(versionedPath)) {
                    if (!installSingleDependency(directory, package, version)) {
                        logError("Failed to install missing package " + package + "@" + version);
                        return false;
                    }
                }
            }
        }

        // Link all packages from cache to the project's vendor directory.
        std::vector<std::future<bool>> futures;
        for (const auto& [package, version] : packageVersions) {
            futures.push_back(std::async(std::launch::async, [this, directory, cachePath, package, version]() -> bool {
                fs::path versionedPath = cachePath / package / version;
                fs::path targetPath = fs::path(directory) / "vendor" / package;
                if (!fs::exists(versionedPath)) {
                    logError("Package " + package + "@" + version + " still missing from cache.");
                    return false;
                }
                try {
                    fs::remove_all(targetPath);
                    fs::create_directories(targetPath.parent_path());
                    fs::create_symlink(versionedPath, targetPath);
                    logMessage("Linked " + package + "@" + version + " from cache.");
                } catch (const fs::filesystem_error& e) {
                    logError("Failed to link " + package + ": " + e.what());
                    return false;
                }
                return true;
            }));
        }
        bool allLinked = true;
        for (auto &fut : futures) {
            if (!fut.get())
                allLinked = false;
        }
        return allLinked;
    }
}