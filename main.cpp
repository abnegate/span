#include "cli.h"
#include "packages/manager_factory.h"
#include "cache.h"
#include <vector>
#include <future>
#include <memory>

namespace fs = std::filesystem;

int main(const int argc, char **argv) {
    CLI::App app{"Universal Package Manager CLI"};

    std::string projectDir = fs::current_path().string();

    app.add_option(
        "-d,--directory",
        projectDir,
        "Project directory (defaults to current directory)"
    );

    const auto cache = std::make_shared<dev::packages::Cache>();
    const auto managers = dev::packages::ManagerFactory::getInstance().createManagers(cache);

    auto detectPackageManagers = [&](
        const std::string &directory
    ) -> std::vector<std::shared_ptr<dev::packages::Manager>> {
        std::vector<std::shared_ptr<dev::packages::Manager>> detected;

        for (auto &manager: managers) {
            if (manager->isProjectType(directory)) {
                detected.push_back(manager);
            }
        }

        return detected;
    };

    app.require_subcommand();

    const auto installCmd = app.add_subcommand(
        "install",
        "Link dependencies from cache, then install missing ones"
    );

    installCmd->callback([&]() {
        const auto detectedManagers = detectPackageManagers(projectDir);
        if (detectedManagers.empty()) {
            std::cerr << "Error: No known package manager detected in " << projectDir << std::endl;
            exit(1);
        }

        std::vector<std::future<bool> > installFutures;
        installFutures.reserve(detectedManagers.size());
        for (const std::shared_ptr<dev::packages::Manager> &manager: detectedManagers) {
            installFutures.push_back(std::async(std::launch::async, [manager, &projectDir]() {
                return manager->installDependencies(projectDir);
            }));
        }

        bool allInstalled = true;
        for (auto &future: installFutures) {
            allInstalled &= future.get();
        }

        if (allInstalled) {
            std::cout << "Dependencies installed successfully for all detected package managers." << std::endl;
        } else {
            std::cerr << "One or more dependency installations failed." << std::endl;
            exit(1);
        }
    });

    CLI11_PARSE(app, argc, argv);

    return 0;
}
