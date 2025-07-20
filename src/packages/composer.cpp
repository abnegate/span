#include "packages/composer.h"
#include "cache.h"
#include "logger.h"
#include <filesystem>
#include <cstdlib>
#include <simdjson.h>
#include <sstream>
#include <future>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <thread>

namespace fs = std::filesystem;

namespace {
    class ThreadPool {
    public:
        explicit ThreadPool(const size_t maxThreads = std::thread::hardware_concurrency()) : shutdown_(false) {
            for(size_t i = 0; i < maxThreads; ++i) {
                workers_.emplace_back([this] { workerFunction(); });
            }
        }

        template<class F>
        std::future<typename std::result_of<F()>::type> enqueue(F&& f) {
            using return_type = typename std::result_of<F()>::type;
            auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
            std::future<return_type> res = task->get_future();
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                tasks_.emplace([task]() { (*task)(); });
            }
            condition_.notify_one();
            return res;
        }

        ~ThreadPool() {
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                shutdown_ = true;
            }
            condition_.notify_all();
            for(std::thread& worker : workers_) {
                worker.join();
            }
        }

    private:
        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> tasks_;
        std::mutex queue_mutex_;
        std::condition_variable condition_;
        bool shutdown_;

        void workerFunction() {
            while(true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] {
                        return shutdown_ || !tasks_.empty();
                    });
                    if(shutdown_ && tasks_.empty()) {
                        return;
                    }
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                task();
            }
        }
    };
}

namespace dev::packages {
    Composer::Composer(std::shared_ptr<Cache> cache) : cache(std::move(cache)) {}

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

        if (isLockFileCacheValid(lockFile)) {
            return lockFileCache[lockFile.string()].versions;
        }

        updateLockFileCache(lockFile);

        return lockFileCache[lockFile.string()].versions;
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

            LockFileCache cache;
            cache.lastRead = std::chrono::system_clock::now();
            cache.fileTimestamp = fs::last_write_time(lockFile);

            auto processPackages = [&](simdjson::ondemand::array packages) {
                for (auto package : packages) {
                    try {
                        std::string_view name = package["name"].get_string();
                        std::string_view version = package["version"].get_string();
                        cache.versions[std::string(name)] = std::string(version);
                    } catch (const simdjson::simdjson_error& e) {
                        Logger::error("Error parsing package: " + std::string(e.what()));
                    }
                }
            };

            // Process both regular and dev packages
            processPackages(doc["packages"].get_array());
            processPackages(doc["packages-dev"].get_array());

            lockFileCache[lockFile.string()] = std::move(cache);
        } catch (const simdjson::simdjson_error& e) {
            throw std::runtime_error("Failed to parse composer.lock: " + std::string(e.what()));
        } catch (const std::exception& e) {
            throw std::runtime_error("Error reading composer.lock: " + std::string(e.what()));
        }
    }

    bool Composer::installDependencies(const std::string& directory) {
        auto versions = getInstalledVersions(directory);
        if (versions.empty()) {
            return false;
        }

        ThreadPool pool(maxConcurrentInstalls);
        std::vector<std::future<bool>> results;
        float progress = 0.0f;
        const float progressStep = 1.0f / versions.size();

        for (const auto& [package, version] : versions) {
            results.push_back(pool.enqueue([this, &directory, &package, &version, &progress, progressStep]() {
                const bool result = installSingleDependency(directory, package, version);
                if (progressCallback) {
                    progress += progressStep;
                    progressCallback(package, progress);
                }
                return result;
            }));
        }

        bool success = true;
        for (auto& result : results) {
            try {
                if (!result.get()) {
                    success = false;
                }
            } catch (const std::exception& e) {
                Logger::error("Package installation failed: " + std::string(e.what()));
                success = false;
            }
        }

        return success;
    }

    bool Composer::installSingleDependency(
        const std::string& directory,
        const std::string& package,
        const std::string& version
    ) const {
        if (!cache) {
            throw std::runtime_error("Cache not initialized");
        }

        if (cache->isCached("composer", package, version)) {
            return true;
        }

        // Implementation of package download and installation
        // This is a placeholder - actual implementation would involve
        // downloading from Packagist or other repositories
        return true;
    }

    bool Composer::linkDependencies(const std::string& directory) {
        auto versions = getInstalledVersions(directory);
        if (versions.empty()) {
            return false;
        }

        bool success = true;
        for (const auto& [package, version] : versions) {
            if (!cache->linkFromCache(
                "composer",
                package,
                version,
                (fs::path(directory) / "vendor" / package).string())
            ) {
                Logger::error("Failed to link package: " + package);
                success = false;
            }
        }

        return success;
    }

    void Composer::setProgressCallback(ProgressCallback callback) {
        progressCallback = std::move(callback);
    }

    void Composer::setTimeout(const std::chrono::seconds timeout) {
        this->timeout = timeout;
    }

    void Composer::setMaxConcurrentInstalls(const size_t max) {
        maxConcurrentInstalls = max;
    }
}
