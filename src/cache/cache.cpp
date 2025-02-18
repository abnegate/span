#include "cache/cache.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace dev::packages {
    std::string Cache::getCacheDir() {
        if (const char* envCacheDir = std::getenv("DEV_PACKAGE_CACHE")) {
            return {envCacheDir};
        }

#ifdef _WIN32
        const char* homeDir = std::getenv("USERPROFILE");
#else
        const char* homeDir = std::getenv("HOME");
#endif

        if (!homeDir) {
            throw std::runtime_error("Failed to determine home directory.");
        }

        return (fs::path(homeDir) / ".dev-cache").string();
    }

    bool Cache::isCached(
        const std::string& language,
        const std::string& package,
        const std::string& version
    ) {
        return fs::exists(fs::path(getCacheDir()) / language / package / version);
    }

    bool Cache::linkFromCache(
        const std::string& language,
        const std::string& package,
        const std::string& version,
        const std::string& targetDir
    ) {
        const auto cachedPath = fs::path(getCacheDir()) / language / package / version;
        const auto targetPath = fs::path(targetDir);

        if (isCached(language, package, version)) {
#ifdef _WIN32
            // On Windows, create a directory junction instead of a symlink
            std::system(("mklink /J " + targetPath + " " + cachedPath).c_str());
#else
            fs::create_symlink(cachedPath, targetPath);
#endif
            std::cout << "Linked from cache: " << cachedPath << " → " << targetPath << std::endl;
            return true;
        }
        return false;
    }

    bool Cache::addToCache(
        const std::string& language,
        const std::string& package,
        const std::string& version,
        const std::string& sourceDir
    ) {
        const auto cachePath = fs::path(getCacheDir()) / language / package / version;

        if (!fs::exists(cachePath)) {
            fs::create_directories(cachePath);
        }

        fs::copy(fs::path(sourceDir), cachePath, fs::copy_options::recursive);

        std::cout << "Added to cache: " << cachePath << std::endl;

        return true;
    }
}