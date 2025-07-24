#include "cache.h"
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_map>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace dev::packages {
    Cache::Cache(const std::optional<std::string>& customCacheDir)
        : cacheDir(customCacheDir.value_or(getDefaultCacheDir())) {
        fs::create_directories(cacheDir);
    }

    std::string Cache::getDefaultCacheDir() {
        if (const char* envCacheDir = std::getenv("DEV_PACKAGE_CACHE")) {
            return fs::path(envCacheDir).make_preferred().string();
        }

#ifdef _WIN32
        const char* homeDir = std::getenv("USERPROFILE");
#else
        const char* homeDir = std::getenv("HOME");
#endif

        if (!homeDir) {
            throw std::runtime_error("Failed to determine home directory");
        }

        return (fs::path(homeDir) / ".dev" / "cache").make_preferred().string();
    }

    std::string Cache::getCacheDir() const {
        return cacheDir;
    }

    bool Cache::isCached(
        const std::string& language,
        const std::string& package,
        const std::string& version
    ) const {
        const auto path = (
            fs::path(cacheDir) /
            escapePath(language) /
            escapePath(package) /
            escapePath(version)
        ).make_preferred();

        return fs::exists(path) && verifyPackageIntegrity(language, package, version);
    }

    bool Cache::linkFromCache(
        const std::string& language,
        const std::string& package,
        const std::string& version,
        const std::string& targetDir
    ) const {
        const auto cachedPath = (
            fs::path(cacheDir) /
            escapePath(language) /
            escapePath(package) /
            escapePath(version)
        ).make_preferred();

        const auto targetPath = fs::path(targetDir);

        if (!isCached(language, package, version)) {
            return false;
        }

        try {
            if (fs::exists(targetPath)) {
                fs::remove_all(targetPath);
            }
            createSymlink(cachedPath, targetPath);
            return true;
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << std::endl;
            return false;
        }
    }

    bool Cache::linkToCache(
        const std::string& language,
        const std::string& package,
        const std::string& version,
        const std::string& sourceDir
    ) const {
        const auto cachedPath = (
            fs::path(cacheDir) /
            escapePath(language) /
            escapePath(package) /
            escapePath(version)
        ).make_preferred();

        const auto sourcePath = fs::path(sourceDir);

        if (!fs::exists(sourcePath)) {
            return false;
        }

        try {
            // Create the parent directories in cache
            fs::create_directories(cachedPath.parent_path());

            // If cache directory already exists, remove it first
            if (fs::exists(cachedPath)) {
                fs::remove_all(cachedPath);
            }

            // Create symlink from cache to source
            createSymlink(sourcePath, cachedPath);
            return true;
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << std::endl;
            return false;
        }
    }

    void Cache::createSymlink(const fs::path& target, const fs::path& link) const {
#ifdef _WIN32
        std::array<char, 32768> buffer;
        std::string cmd = "mklink /J \"" + link.string() + "\" \"" + target.string() + "\"";
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) return false;

        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            // Process output if needed
        }

        return _pclose(pipe) == 0;
#else
        return fs::create_symlink(target, link);
#endif
    }

    std::string Cache::escapePath(const std::string& path) {
        std::string result;
        result.reserve(path.size());

        for (const char c : path) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.') {
                result += c;
            } else {
                result += '_';
            }
        }

        return result;
    }

    bool Cache::cleanPackage(
        const std::string& language,
        const std::string& package,
        const std::string& version
    ) const {
        const auto path = (
            fs::path(cacheDir) /
            escapePath(language) /
            escapePath(package) /
            escapePath(version)
        ).make_preferred();

        try {
            return fs::remove_all(path) > 0;
        } catch (const fs::filesystem_error&) {
            return false;
        }
    }

    bool Cache::verifyPackageIntegrity(
        const std::string& language,
        const std::string& package,
        const std::string& version
    ) const {
        const auto path = (
            fs::path(cacheDir) /
            escapePath(language) /
            escapePath(package) /
            escapePath(version)
        ).make_preferred();

        if (!fs::exists(path)) {
            return false;
        }

        try {
            // TODO: Implement checksum verification when checksums are available
            // Basic integrity check - ensure directory is readable and not empty
            return fs::is_directory(path) && !fs::is_empty(path);
        } catch (const fs::filesystem_error&) {
            return false;
        }
    }

    size_t Cache::getCacheSize() const {
        size_t total = 0;
        for (const auto& entry : fs::recursive_directory_iterator(cacheDir)) {
            if (fs::is_regular_file(entry)) {
                total += fs::file_size(entry);
            }
        }
        return total;
    }

    bool Cache::cleanup(const size_t maxSizeBytes) const {
        struct CacheEntry {
            fs::path path;
            fs::file_time_type lastAccess;
            uintmax_t size;
        };

        try {
            // Accumulate sizes and track last access
            std::unordered_map<fs::path, uintmax_t> sizeMap;
            std::unordered_map<fs::path, fs::file_time_type> accessMap;

            for (
                auto it = fs::recursive_directory_iterator(cacheDir);
                 it != fs::recursive_directory_iterator();
                 ++it
            ) {
                if (!fs::is_regular_file(it->path()) || it.depth() < 4) {
                    continue;
                }

                // Find the version directory at depth 3
                fs::path versionDir = it->path();
                int d = it.depth();
                while (d-- > 3) {
                    versionDir = versionDir.parent_path();
                }

                uintmax_t fileSize = fs::file_size(it->path());
                sizeMap[versionDir] += fileSize;

                // Get file access time (LRU eviction key)
                fs::file_time_type accessTime;
#ifdef _WIN32
                // TODO: Implement Windows-specific access time retrieval
                atime = fs::last_write_time(it->path());
#else
                struct stat sb{};
                if (stat(it->path().c_str(), &sb) == 0) {
                    auto sysTime = std::chrono::system_clock::from_time_t(sb.st_atime);
                    accessTime = fs::file_time_type::clock::from_sys(sysTime);
                } else {
                    accessTime = fs::last_write_time(it->path());
                }
#endif
                auto current = accessMap.find(versionDir);
                if (current == accessMap.end() || accessTime > current->second) {
                    accessMap[versionDir] = accessTime;
                }
            }

            // Build cache entries
            std::vector<CacheEntry> entries;
            entries.reserve(sizeMap.size());
            for (auto const& [path, sz] : sizeMap) {
                entries.push_back({
                    path,
                    accessMap[path],
                    sz
                });
            }

            // Sort by last access ascending (oldest first) for LRU
            std::ranges::sort(entries, [](auto const& a, auto const& b) {
                return a.lastAccess < b.lastAccess;
            });

            // Compute total cache size
            uintmax_t currentSize = 0;
            for (auto const& e : entries) {
                currentSize += e.size;
            }

            // Evict least recently used entries until under limit
            for (auto const& entry : entries) {
                if (currentSize <= maxSizeBytes) {
                    break;
                }
                if (fs::remove_all(entry.path) > 0) {
                    currentSize -= entry.size;
                }
            }

            return currentSize <= maxSizeBytes;
        } catch (const fs::filesystem_error&) {
            return false;
        }
    }
}
