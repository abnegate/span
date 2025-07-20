#pragma once

#include <string>
#include <filesystem>
#include <optional>

namespace dev::packages {
    class Cache {
    public:
        /**
         * Initializes the cache directory, creating it if it doesn't exist.
         *
         * @param customCacheDir Optional custom cache directory. If not provided, defaults to ~/.dev-cache.
         */
        explicit Cache(const std::optional<std::string>& customCacheDir = std::nullopt);

        /**
         * Get the cache directory path.
         *
         * @return The path to the cache directory.
         */
        [[nodiscard]] std::string getCacheDir() const;

        /**
         * Get the total size of the cache directory.
         *
         * @return The total size in bytes.
         */
        [[nodiscard]] size_t getCacheSize() const;

        /**
         * Check if the given version of a package is cached for a specific language.
         *
         * @param language The programming language of the package (e.g., "PHP").
         * @param package The name of the package.
         * @param version The version of the package.
         * @return True if the package is cached, false otherwise.
         */
        [[nodiscard]] bool isCached(
            const std::string& language,
            const std::string& package,
            const std::string& version
        ) const;

        /**
         * Link a package from the cache to the target directory.
         *
         * @param language The programming language of the package (e.g., "PHP").
         * @param package The name of the package.
         * @param version The version of the package.
         * @param targetDir The directory where the package should be linked.
         * @return True if the link was created successfully, false otherwise.
         */
        [[nodiscard]] bool linkFromCache(
            const std::string& language,
            const std::string& package,
            const std::string& version,
            const std::string& targetDir
        ) const;

        /**
         * Verify the integrity of a cached package.
         *
         * @param language The programming language of the package (e.g., "PHP").
         * @param package The name of the package.
         * @param version The version of the package.
         * @return True if the package is valid, false otherwise.
         */
        [[nodiscard]] bool verifyPackageIntegrity(
            const std::string& language,
            const std::string& package,
            const std::string& version
        ) const;

        /**
         * Clean a specific package from the cache.
         *
         * @param language The programming language of the package (e.g., "PHP").
         * @param package The name of the package.
         * @param version The version of the package.
         * @return True if the package was successfully cleaned, false otherwise.
         */
        [[nodiscard]] bool cleanPackage(
            const std::string& language,
            const std::string& package,
            const std::string& version
        ) const;

        /**
         * Clean up the cache directory by removing old or large files.
         *
         * @param maxSizeBytes Maximum size of the cache directory in bytes. Default is 5GB.
         * @return True if cleanup was successful, false otherwise.
         */
        [[nodiscard]]
        bool cleanup(size_t maxSizeBytes = 5ULL * 1024 * 1024 * 1024) const; // 5GB default

    private:
        std::string cacheDir;

        static std::string getDefaultCacheDir();

        void createSymlink(
            const std::filesystem::path& target,
            const std::filesystem::path& link
        ) const;

        [[nodiscard]] std::string escapePath(
            const std::string& path
        ) const;
    };
}