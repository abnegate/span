#pragma once

#include "manager.h"
#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace dev::packages {
    class ManagerFactory {
    public:
        using ManagerCreator = std::function<std::shared_ptr<Manager>(std::shared_ptr<Cache>)>;

        static ManagerFactory& getInstance();

        void registerManager(const std::string& name, ManagerCreator creator);

        std::vector<std::shared_ptr<Manager>> createManagers(std::shared_ptr<Cache> cache);

        [[nodiscard]] std::vector<std::string> getRegisteredManagerNames() const;

    private:
        ManagerFactory() = default;
        std::vector<ManagerCreator> creators_;
        std::vector<std::string> names_;
    };

    template<typename T>
    class ManagerRegistrar {
    public:
        explicit ManagerRegistrar(const std::string& name) {
            ManagerFactory::getInstance().registerManager(name, [](std::shared_ptr<Cache> cache) {
                return std::make_shared<T>(cache);
            });
        }
    };
}
