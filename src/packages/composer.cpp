#include "packages/composer.h"
#include "cache/cache.h"
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <simdjson.h>

namespace fs = std::filesystem;

namespace dev::packages {
    bool Composer::isProjectType(const std::string &directory) {
        return exists(fs::path(directory) / "composer.json");
    }

    std::vector<std::string> Composer::getDependencyFiles() {
        return {"composer.json", "composer.lock"};
    }

    std::unordered_map<std::string, std::string> Composer::getInstalledVersions(const std::string &directory) {
        std::unordered_map<std::string, std::string> packageVersions;
        fs::path lockFile = fs::path(directory) / "composer.lock";

        if (!exists(lockFile)) {
            return packageVersions;
        }

        try {
            simdjson::ondemand::parser parser;
            simdjson::padded_string json = simdjson::padded_string::load(lockFile.string());
            simdjson::ondemand::document doc = parser.iterate(json);

            for (auto package: doc["packages"]) {
                auto packageName = std::string(package["name"].get_string().value_unsafe());
                auto packageVersion = std::string(package["version"].get_string().value_unsafe());

                packageVersions[packageName] = packageVersion;
            }
        } catch (const std::exception &e) {
            std::cerr << "Failed to parse composer.lock: " << e.what() << std::endl;
        }

        return packageVersions;
    }

    bool Composer::installDependencies(const std::string &directory) {
        std::cout << "Running full dependency installation..." << std::endl;

        const fs::path cachePath = fs::path(Cache::getCacheDir()) / "php";

        create_directories(cachePath);

        const std::string command = "cd " + fs::path(directory).string() +
                                    " && COMPOSER_VENDOR_DIR=" + cachePath.string() +
                                    " composer install --ignore-platform-reqs --prefer-dist --no-interaction";

        if (std::system(command.c_str()) == 0) {
            std::cout << "Dependencies installed successfully in cache." << std::endl;
            return true;
        }

        std::cerr << "Dependency installation failed." << std::endl;
        return false;
    }

    bool Composer::linkDependencies(const std::string &directory) {
        if (!exists(fs::path(directory) / "composer.lock")) {
            std::cout << "No lockfile detected, running full install..." << std::endl;
            return installDependencies(directory);
        }

        std::unordered_map<std::string, std::string> packageVersions = getInstalledVersions(directory);

        std::cout << "Checking global cache for dependencies..." << std::endl;

        bool allCached = true;
        const fs::path cachePath = fs::path(Cache::getCacheDir()) / "php";

        for (const auto &[package, version]: packageVersions) {
            fs::path packageCachePath = cachePath / package / version;
            fs::path targetPath = fs::path(directory) / "vendor" / package;

            if (Cache::isCached("php", package, version)) {
                remove_all(targetPath);
                create_symlink(packageCachePath, targetPath);
            } else {
                std::cout << "Package " << package << "@" << version << " not found in cache. Installing missing dependencies..." <<
                        std::endl;
                allCached = false;
                break; // Exit loop early to install all at once
            }
        }

        return allCached || installDependencies(directory);
    }
}
