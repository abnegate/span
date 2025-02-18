#include "cache/cache.h"
#include <filesystem>
#include <iostream>
#include <cstdlib>

namespace fs = std::filesystem;

namespace dev::packages {
    std::string Cache::getCacheDir() {
        if (const char *envCacheDir = std::getenv("DEV_PACKAGE_CACHE")) {
            return {envCacheDir};
        }

#ifdef _WIN32
        const char* homeDir = std::getenv("USERPROFILE");
#else
        const char *homeDir = std::getenv("HOME");
#endif

        if (!homeDir) {
            throw std::runtime_error("Failed to determine home directory.");
        }

        return (fs::path(homeDir) / ".dev-cache").string();
    }

    bool Cache::isCached(
        const std::string &language,
        const std::string &package,
        const std::string &version
    ) {
        return exists(fs::path(getCacheDir()) / language / package / version);
    }

    bool Cache::linkFromCache(
        const std::string &language,
        const std::string &package,
        const std::string &version,
        const std::string &targetDir
    ) {
        const auto cachedPath = fs::path(getCacheDir()) / language / package / version;
        const auto targetPath = fs::path(targetDir);

        if (isCached(language, package, version)) {
#ifdef _WIN32
            // On Windows, create a directory junction.
            std::string cmd = "mklink /J \"" + targetPath.string() + "\" \"" + cachedPath.string() + "\"";
            int result = std::system(cmd.c_str());
            if (result != 0) {
                std::cerr << "Failed to create junction for " << targetPath << std::endl;
                return false;
            }
#else
            try {
                create_symlink(cachedPath, targetPath);
            } catch (const fs::filesystem_error &e) {
                std::cerr << "Symlink creation failed: " << e.what() << std::endl;
                return false;
            }
#endif
            std::cout << "Linked from cache: " << cachedPath << " → " << targetPath << std::endl;
            return true;
        }
        return false;
    }

    bool Cache::addToCache(
        const std::string &language,
        const std::string &package,
        const std::string &version,
        const std::string &sourceDir
    ) {
        const auto cachePath = fs::path(getCacheDir()) / language / package / version;

        try {
            if (!exists(cachePath)) {
                create_directories(cachePath);
            }

            copy(
                fs::path(sourceDir),
                cachePath,
                fs::copy_options::recursive | fs::copy_options::overwrite_existing
            );
        } catch (const fs::filesystem_error &e) {
            std::cerr << "Failed to add to cache: " << e.what() << std::endl;
            return false;
        }

        std::cout << "Added to cache: " << cachePath << std::endl;
        return true;
    }
}
