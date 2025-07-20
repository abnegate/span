#include "packages/manager_factory.h"

namespace dev::packages {
    ManagerFactory& ManagerFactory::getInstance() {
        static ManagerFactory instance;
        return instance;
    }

    void ManagerFactory::registerManager(const std::string& name, ManagerCreator creator) {
        names_.push_back(name);
        creators_.push_back(std::move(creator));
    }

    std::vector<std::shared_ptr<Manager>> ManagerFactory::createManagers(std::shared_ptr<Cache> cache) {
        std::vector<std::shared_ptr<Manager>> managers;
        for (const auto& creator : creators_) {
            managers.push_back(creator(cache));
        }
        return managers;
    }

    std::vector<std::string> ManagerFactory::getRegisteredManagerNames() const {
        return names_;
    }
}

