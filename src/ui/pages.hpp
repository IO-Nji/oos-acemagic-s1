#pragma once
#include "core/config.hpp"
#include "data/providers/docker_provider.hpp"
#include "data/providers/identity_provider.hpp"
#include "data/providers/network_provider.hpp"
#include "data/providers/sensors_provider.hpp"
#include "data/providers/services_provider.hpp"
#include "data/providers/system_provider.hpp"
#include "ui/screens/docker_screen.hpp"
#include "ui/screens/identity_screen.hpp"
#include "ui/screens/network_screen.hpp"
#include "ui/screens/sensors_screen.hpp"
#include "ui/screens/services_screen.hpp"
#include "ui/screens/system_screen.hpp"
#include <memory>

// The dashboard's data sources and pages, assembled the same way by the service
// and by the offline preview tool.
struct Providers {
    std::shared_ptr<SystemDataProvider> system;
    std::shared_ptr<NetworkDataProvider> network;
    std::shared_ptr<SensorsDataProvider> sensors;
    std::shared_ptr<DockerDataProvider> docker;
    std::shared_ptr<ServicesDataProvider> services;
    std::shared_ptr<IdentityDataProvider> identity;
    
    explicit Providers(const Config& config)
        : system(std::make_shared<SystemDataProvider>(config.diskPath)),
          network(std::make_shared<NetworkDataProvider>()),
          sensors(std::make_shared<SensorsDataProvider>()),
          docker(std::make_shared<DockerDataProvider>()),
          services(std::make_shared<ServicesDataProvider>(config.services)),
          identity(std::make_shared<IdentityDataProvider>()) {}
};

// Add the pages named in config.pages, in that order
inline void addPages(ScreenManager& screens, const Config& config, const Providers& p,
                     const std::shared_ptr<const Health>& health) {
    for (const auto& name : config.pages) {
        if (name == "system") screens.addPage(std::make_shared<SystemScreen>(p.system, p.sensors));
        else if (name == "network") screens.addPage(std::make_shared<NetworkScreen>(p.network));
        else if (name == "sensors") screens.addPage(std::make_shared<SensorsScreen>(p.sensors));
        else if (name == "docker") screens.addPage(std::make_shared<DockerScreen>(p.docker));
        else if (name == "services") screens.addPage(std::make_shared<ServicesScreen>(p.services));
        else if (name == "identity") screens.addPage(std::make_shared<IdentityScreen>(p.identity, health));
    }
}
