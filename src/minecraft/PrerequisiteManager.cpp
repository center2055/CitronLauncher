#include "minecraft/PrerequisiteManager.h"

#include "core/Logger.h"
#include "core/Text.h"
#include "platform/windows/PackageManager.h"

namespace citron {

PrerequisiteManager::PrerequisiteManager(TaskScheduler& scheduler) : scheduler_(scheduler) {}

void PrerequisiteManager::check(std::function<void(platform::GdkEnvironment)> done) {
    scheduler_.run([done = std::move(done)](std::stop_token) {
        platform::initializeApartment();
        auto env = platform::inspectEnvironment();
        log::info("environment: {} | gaming services {} ({}) | game input {} | vclibs {} | app runtime {} | xbox identity {}",
                  text::toUtf8(env.windowsVersion), env.gamingServices, text::toUtf8(env.gamingServicesVersion), env.gameInput, env.vcLibs,
                  env.appRuntime, env.xboxIdentity);
        done(std::move(env));
    });
}

}
