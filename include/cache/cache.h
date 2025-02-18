#pragma once

#include <string>

namespace dev::packages {
    class Cache {
    public:
        static std::string getCacheDir();

        static bool isCached(
            const std::string& language,
            const std::string& package,
            const std::string& version
        );

        static bool linkFromCache(
            const std::string& language,
            const std::string& package,
            const std::string& version,
            const std::string& targetDir
        );

        static bool addToCache(
            const std::string& language,
            const std::string& package,
            const std::string& version,
            const std::string& sourceDir
        );
    };
}