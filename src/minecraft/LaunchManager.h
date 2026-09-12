#pragma once

#include "core/Error.h"
#include "core/TaskScheduler.h"
#include "minecraft/Version.h"

#include <filesystem>
#include <functional>
#include <memory>

namespace citron {

using LaunchDone = std::function<void(Result<void> result)>;

enum class LaunchMode {
    Direct,
    Helper,
    LocalGdk,
};

class LaunchManager {
public:
    explicit LaunchManager(TaskScheduler& scheduler);

    bool launch(VersionChannel channel, std::filesystem::path installLocation, LaunchMode mode, LaunchDone done);
    bool busy() const;

private:
    TaskScheduler& scheduler_;
    std::shared_ptr<Operation> operation_;
};

}
