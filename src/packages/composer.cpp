#include "packages/composer.h"
#include "cache/cache.h"
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <simdjson.h>

namespace fs = std::filesystem;

namespace dev::packages {
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

            for (auto package: doc["packages"]) {
                const auto packageName = std::string(package["name"]);
                const auto packageVersion = std::string(package["version"]);

                packageVersions[packageName] = packageVersion;
            }
        } catch (const std::exception &e) {
            std::cerr << "Failed to parse composer.lock: " << e.what() << std::endl;
        }

        return packageVersions;
    }

    bool Composer::installDependencies(const std::string &directory) {
        std::cout << "Running first-time dependency installation..." << std::endl;

        // Get package versions (used for caching)
        std::unordered_map<std::string, std::string> packageVersions = getInstalledVersions(directory);
        if (packageVersions.empty()) {
            std::cout << "No lockfile found. Running first install to generate versions..." << std::endl;
        }

        // Set Composer to install directly into cache
        const fs::path cachePath = fs::path(Cache::getCacheDir()) / "php";

        fs::create_directories(cachePath);

        const std::string command = "cd " + fs::path(directory).string() +
                                    " && COMPOSER_VENDOR_DIR=" + cachePath.string() +
                                    " composer install --prefer-dist --no-interaction";

        if (std::system(command.c_str()) == 0) {
            std::cout << "Dependencies installed successfully in cache." << std::endl;

            // Now that composer.lock exists, extract versions
            packageVersions = getInstalledVersions(directory);

            // Ensure each package has its own versioned cache path
            for (const auto &[package, version]: packageVersions) {
                fs::path packageCachePath = cachePath / package / version;
                fs::create_directories(packageCachePath);

                // Move package files to their dedicated versioned cache location
                fs::rename(cachePath / package, packageCachePath);
            }

            // Link project vendor/ directory to the cache
            fs::remove_all(fs::path(directory) / "vendor");
            fs::create_symlink(cachePath, fs::path(directory) / "vendor");

            return true;
        }

        std::cerr << "Dependency installation failed." << std::endl;
        return false;
    }

    bool Composer::linkDependencies(const std::string &directory) {
        if (!fs::exists(fs::path(directory) / "composer.lock")) {
            std::cout << "No lockfile detected, running first install..." << std::endl;
            return installDependencies(directory);
        }

        auto packageVersions = getInstalledVersions(directory);

        std::cout << "Checking global cache for dependencies..." << std::endl;

        bool allLinked = true;

        const fs::path cachePath = fs::path(Cache::getCacheDir()) / "php";

        for (const auto &[package, version]: packageVersions) {
            fs::path packageCachePath = cachePath / package / version;
            fs::path targetPath = fs::path(directory) / "vendor" / package;

            if (Cache::isCached("php", package, version)) {
                fs::remove_all(targetPath);
                fs::create_symlink(packageCachePath, targetPath);
            } else {
                std::cout << "Package " << package << " not found in cache. Falling back to install." << std::endl;
                allLinked = false;
            }
        }

        return allLinked || installDependencies(directory);
    }
}
